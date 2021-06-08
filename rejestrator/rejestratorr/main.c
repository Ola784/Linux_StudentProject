#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <stdbool.h>
#include <signal.h>
#include <time.h>

#define handle_error(msg) \
        do { perror(msg);exit(EXIT_FAILURE);} while(0)

#define DEFAULT_T "1"
#define BUFSIZE 255

struct inputData
{
    int text_fd;
    int bin_fd;
    char* b;
    char* t;
    long d;
    long c;
    bool binary;
    bool text;
    bool works;
    bool pkt_ref;
    bool iden_zrod;
    int value;

};

struct inputData volatile data;
struct timespec real_time, ref_time;

void get_options(int, char*[]);
int openFiles();
void mainLoop();
void controlFunction(siginfo_t*);
struct timespec calculateTime(struct timespec, struct timespec);
struct timespec* TimeSpec();
int writeToBin(float, struct timespec*, pid_t*);
void writeToText(float*, struct timespec*, pid_t*);
bool is_RT(int signal);
static void sigfunction_d(int signo, siginfo_t* SI, void* data){
}
void sigfunction_c(int signo, siginfo_t* SI, void* data) {
}

int main(int argc, char* argv[]) {

    if (argc < 5)
    {
        perror("Invalid input!\n Usage ./rejestrator -c -d ");
        exit(EXIT_FAILURE);
    }

    data.t=(char*)DEFAULT_T;
    data.value= 0b0000;
    get_options(argc, argv);

 //   printf("d: %ld c: %ld b %s t %s is used ext %d\n",data.d,data.c,data.b,data.t,data.text);
    if(openFiles()<0)
        handle_error("open");
     mainLoop();

    exit(EXIT_SUCCESS);
}

void get_options(int argc, char* argv[])
{
    int opt=0;
    char* end=NULL;
    char* end2=NULL;
    while((opt=getopt(argc,argv,"b:t:d:c:"))!= -1) {
        switch (opt) {
            case 'b':
                data.b=optarg;
                data.binary=1;
                data.value=data.value | 0b1000;
                break;
            case 't':
                if(!strcmp(optarg,"-"))
                    data.t="0";
                else {
                    data.t = optarg;
                    data.text=1;
                }
                break;
            case 'd':
                data.d=strtol(optarg, &end, 0);
                if (*end != '\0' || !is_RT(data.d)) {
                    fprintf(stderr, "wrong arg -d %s\n", optarg);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'c':
                data.c=strtol(optarg, &end2, 0);
                if (*end2 != '\0' || !is_RT(data.c)) {
                    fprintf(stderr, "wrong arg -c %s\n", optarg);
                    exit(EXIT_FAILURE);
                }
                break;
            default: break;
        }
    }
    data.iden_zrod=0;
    data.pkt_ref=0;
    data.works=0;
}
int openFiles()
{
    if(data.text) {
        data.text_fd = open(data.t, O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR);
        if (data.text_fd == -1)
            return -1;
    }

    if(data.binary) {
        data.bin_fd = open(data.b, O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR);
        if (data.bin_fd == -1)
            return -1;
    }
    return 0;
}
void mainLoop() {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = sigfunction_c;

    struct sigaction sa_d;
    sigemptyset(&sa_d.sa_mask);
    sa_d.sa_flags = SA_SIGINFO;
    sa_d.sa_sigaction = sigfunction_d;

    if ((sigaction(data.d, &sa_d, NULL) == -1) || (sigaction(data.c, &sa, NULL) == -1))
        handle_error("sigaction");
    //-- for waiting
    sigset_t set;
    siginfo_t si={0};
    int sig;
    sigemptyset(&set);
    if ((sigaddset(&set, data.d) == -1) || (sigaddset(&set, data.c) == -1))
        handle_error("sigaddset");

    while (1) {
        sig=sigwaitinfo(&set, &si);
        if(sig==-1)
            handle_error("sigwaitinfo");
        // pause();
        if (si.si_signo == data.c) {
            controlFunction(&si);
        }
        else if (si.si_signo == data.d && data.works) {
            struct timespec* ts=TimeSpec();
           float temp=0;
            memcpy(&temp,&si.si_value.sival_int,sizeof(int));
            writeToText(&temp,ts,&si.si_pid);
            if(data.binary) {
                writeToBin(temp, ts, &si.si_pid);
            }
        }
    }
}
void controlFunction(siginfo_t* si)
{
    switch (si->si_value.sival_int) {
        case 0:
            data.works=0;
            data.value=data.value & 0b1110;
            break;
        case 1: {
            data.pkt_ref = 0;
            data.works=1;
            data.value = data.value | 0b0001;
            data.value = data.value & 0b1101;
            break;
        }
        case 2: {
            if (clock_gettime(CLOCK_MONOTONIC, &ref_time) == -1)
                handle_error("clock_gettime");
            data.value = data.value | 0b0011;
            data.pkt_ref = 1;
            data.works=1;
            break;
        }
        case 3: {
            if (ref_time.tv_sec == 0 && ref_time.tv_nsec == 0) {//gdy brak starego pkt referencyjnego
                if (clock_gettime(CLOCK_MONOTONIC, &ref_time) == -1)
                    handle_error("clock_gettime");
            }
            data.value = data.value | 0b0011;
            data.pkt_ref=1;
            data.works=1;
            break;
        }
        case 5:
            data.value = data.value | 0b0101;
            data.iden_zrod = 1;
            data.works=1;
            break;
        case 9: {
            data.works=1;
            struct stat sb;
            if (fstat(data.text_fd, &sb) == -1)
                handle_error("stat");
            if ((sb.st_mode & S_IFMT)==S_IFREG) {
                ftruncate(data.text_fd, 0);
            }
            break;
        }
        case 255: {
                union sigval sv;
                sv.sival_int=data.value;
                sigqueue(si->si_pid, si->si_signo, sv);
                break;
        }
        default:
            break;
    }
}
struct timespec calculateTime(struct timespec a, struct timespec b){
    struct timespec res={0};
    res.tv_sec=a.tv_sec-b.tv_sec;
    res.tv_nsec=a.tv_nsec-b.tv_nsec;
    return res;
}
struct timespec* TimeSpec()
{
    if(!data.pkt_ref)
    {
        if (clock_gettime(CLOCK_REALTIME, &real_time) == -1)
            handle_error("clock_gettime");
        return &real_time;
    }
    else {
        struct timespec* res=(struct timespec*)malloc(sizeof(struct timespec));
        struct timespec temp={0};
        if (clock_gettime(CLOCK_MONOTONIC, &temp) == -1)
            handle_error("clock_gettime");
        *res=calculateTime(temp,ref_time);
        return res;
    }
}
int writeToBin(float x, struct timespec* ts, pid_t* s){
    if((write(data.bin_fd,&x,sizeof(float))==-1)||
            (write(data.bin_fd,ts,sizeof(struct timespec))==-1)||
            ( write(data.bin_fd,s,sizeof(pid_t))==-1))
        return -1;
 return 0;

}
void writeToText(float* temp, struct timespec* ts, pid_t* s){
    struct tm *tms={0};
    tms=localtime(&ts->tv_sec);
    char buf[BUFSIZE];
    if(!data.pkt_ref) {
             if(data.iden_zrod){
            sprintf(buf, "%d.%d.%d %d:%d:%d.%d %lf %d\n", tms->tm_year + 1900, tms->tm_mon + 1,
                    tms->tm_mday, tms->tm_hour, tms->tm_min, tms->tm_sec,(int)(ts->tv_nsec/1e6), *temp, *s);
            }
             else {
                 sprintf(buf, "%d.%d.%d %d:%d:%d.%d %lf\n", tms->tm_year + 1900, tms->tm_mon + 1,
                         tms->tm_mday, tms->tm_hour, tms->tm_min, tms->tm_sec,(int)(ts->tv_nsec/1e6), *temp);
             }
    }
    else {
        int n=ts->tv_sec;
        int hours = n / 3600;
        n%=3600;
        int min=n/60;
        n%=60;
        int sec=n;
        if(data.iden_zrod) {
            sprintf(buf, "%d:%d:%d.%d %lf %d\n", hours, min, sec,(int)(ts->tv_nsec/1e6), *temp, *s);
        }
        else
            sprintf(buf, "%d:%d:%d.%d %lf\n",hours, min, sec,(int)(ts->tv_nsec/1e6), *temp);
    }
    size_t nbytes = strlen(buf);
    if(write(data.text_fd, buf, nbytes)==-1)
        handle_error("write");
}
bool is_RT(int signal){
    return(signal>31 && signal <=64);
}
