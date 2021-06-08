#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>

//int arg;
#define handle_error(msg) \
        do { perror(msg);exit(EXIT_FAILURE);} while(0)

struct inputData{
    pid_t pid;
    long signal_no;
};
siginfo_t sigInfo;

void get_options(int, char*[],struct inputData*);
void sigfuntion(int signo, siginfo_t* SI, void* data)
{
  //   *SI=sigInfo;
}
void waitForSignal(struct inputData*);
bool is_RT(int signal);

int main(int argc, char* argv[]) {
    if (argc < 4)
    {
        perror("Invalid input!\n Usage ./info_rejestrator -c %d pid ");
        exit(EXIT_FAILURE);
    }
    //arg=atoi(argv[4]);
    struct inputData data;
    get_options(argc, argv,&data);
    printf("pid: %d\nsignal: %ld\n",data.pid,data.signal_no);
    waitForSignal(&data);
    exit(EXIT_SUCCESS);
}

void get_options(int argc, char* argv[],struct inputData* data)
{
    int opt=0;
    char* end=NULL;
    char* end2=NULL;
    while((opt=getopt(argc,argv,"c:"))!= -1)
    {
        if(opt=='c') {
            data->signal_no=strtol(optarg, &end, 0);
            if (*end != '\0' || !is_RT(data->signal_no)) {
                fprintf(stderr, "wrong arg -c %s\n", optarg);
            }
        }
    }
    if(optind<argc)
    {   char* buf=(char*)calloc(strlen(argv[optind])+1,sizeof(char));
        strcpy(buf,argv[optind]);
        data->pid=strtol(buf,&end2,0);
    if (*end2 != '\0') {
        fprintf(stderr, "wrong arg -c %s\n", optarg);
       }
    }
}

void waitForSignal(struct inputData* data){
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction=sigfuntion;
    sa.sa_flags=SA_SIGINFO;
    sigset_t mask={0};
    union sigval sv;
    struct timespec timeout={.tv_sec=2,.tv_nsec=0};

    if(sigaction(data->signal_no,&sa,NULL)==-1)
        handle_error("sigaction");

    //dodanie do maski sygnalu na ktory czekam
    if (sigaddset(&mask, data->signal_no) == -1)
    {
        perror("Error in sigaddset!\n");
        exit(EXIT_FAILURE);
    }
    sv.sival_int=255;
    //send signal
    if(sigqueue(data->pid,data->signal_no,sv)==-1)
    {
        handle_error("siqqueue");
    }
    errno=0;
    if(sigtimedwait(&mask, &sigInfo, &timeout)==-1){
        if(errno==EAGAIN)
            fprintf(stderr,"No signal received within  the  timeout  period\n");
    }
    //rozkoduj
    else{
       //printf("odpowiedz: %d\n",sigInfo.si_value.sival_int);
        int z=sigInfo.si_value.sival_int;
        if(z &(1<<0))
            printf("rejestracja działa\n");
        if(z&(1<<1))
            printf("używany jest punkt referencyjny\n");
        if(z&(1<<2))
            printf("używana jest identyfikacja źródeł\n");
        if(z&(1<<3))
            printf("używany jest format binarny\n");

    }
}
bool is_RT(int signal){
    return(signal>31 && signal <=64);
}
