#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <memory.h>
#include <float.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <math.h>
#include <sys/timerfd.h>
#include <fcntl.h>
#include <poll.h>

#define handle_error(msg) \
        do { perror(msg);exit(EXIT_FAILURE);} while(0)

#define AMP 1.0
#define FREQ 0.25
#define PROBE 1
#define PERIOD -1
#define PID 1
#define RT 0
#define PRECISION 1000000

struct Sinus{
    float amp;
    float freq;
    float probe;
    float period;
    short int pid;
    short int rt;
};
bool activePeriod=0;//zmienna informujaca czy period timer dziala, dla default period == -1
bool activeProbe=0;
struct Sinus volatile SIN;
struct timespec ref_point;

void set_sinus_parameters(float, float, float, float,short int, short int);
void getPort(char**, short int*);
void rejestration(int, char*, in_port_t);
bool is_RT(int);
double sinus(float, float,double);
void setTime(struct timespec*);
double calculateTime(struct timespec,struct timespec);
void parseDatagramm(int*,char*);
void compareParse(char*,char*);
float parseFloat(char*);
int parseInt(char*);
void setFdTimer(int,int);
int setPeriodTimer(int);
void selectTimer(short int,int);
void checkSin(float, float,float,float,int,int);
void checkSignal(int,int);
char* raport_mes();

#define BUFLEN 255

int main(int argc, char*argv[]) {
    if (argc != 2)
    {
        perror("Invalid input!\n Usage ./monochord %hd\n");
        exit(EXIT_FAILURE);
    }
    short int udp_port;
    set_sinus_parameters( AMP, FREQ,PROBE,PERIOD,PID,RT);//ustawiam wartosci domyslne dla sinusa
    getPort(argv,&udp_port);//zczytuje nr portu udp
    printf("UDP PORT: %hd\n",udp_port);
    int sock_fd=socket(AF_INET, SOCK_DGRAM,0);
    if(sock_fd==-1)
        handle_error("socket");

    rejestration(sock_fd,"127.0.0.1",udp_port);
    selectTimer(udp_port,sock_fd);
}

void set_sinus_parameters(float amp, float freq, float probe, float period, short int pid, short int rt)
{
    SIN.amp=amp;
    SIN.freq=freq;
    SIN.probe=probe;
    SIN.period=period;
    SIN.pid=pid;
    SIN.rt=rt;
}
void getPort(char** argv, short int* x)
{
    sscanf(argv[1], "%hd",x );
}
void rejestration(int sockfd, char* Host, in_port_t Port)
{
    struct sockaddr_in A;
    A.sin_family=AF_INET;
    A.sin_port=htons(Port);
    int R=inet_aton(Host, &A.sin_addr);
    if(!R){
        fprintf(stderr,"incorrect address: %s\n",Host);
        exit(1);
    }
    if(bind(sockfd,(struct sockaddr*)&A,sizeof(A))<0)
        handle_error("bind");
}
bool is_RT(int signal){
    return(signal>31 && signal <=64);
}
double sinus(float amp, float freq,double time)
{
    return (amp*sin(2*M_PI*freq*time));
}
void setTime(struct timespec* ts)
{
    if(clock_gettime(CLOCK_MONOTONIC,ts)==-1)
        handle_error("clock_gettime");
}
double calculateTime(struct timespec x,struct timespec y){
    return (y.tv_sec-x.tv_sec)+(y.tv_nsec-x.tv_nsec)/1e9;

}
void parseDatagramm(int* raport,char* str){
    char* token;
    while ((token = strsep(&str, "\\n")) != NULL)
    {
        char *tok2=NULL;
        char*val=NULL;
        char*temp;
        const char* r="raport";
        int t=1;
        while((  temp = strsep(&token, "\t :"))!=NULL){
            if(t==1){
                tok2=temp;
                t=0;
            }else{
                val=temp;
            }
        }
        compareParse(tok2,val);
        if(*tok2==*r){
            *raport=1;
        }
    }

}
void compareParse(char* buf,char* value){
    const char* amp="amp";
    const char* freq="freq";
    const char* probe="probe";
    const char* period="period";
    const char* pid="pid";
    const char* rt="rt";

    if(!strcmp(buf,amp)){
        SIN.amp=parseFloat(value);
    }
    else  if(!strcmp(buf,freq)){
        SIN.freq=parseFloat(value);
    }
    else if(!strcmp(buf,probe)){
        SIN.probe=parseFloat(value);
    }
    else if(!strcmp(buf,period)){
        SIN.period=parseFloat(value);
    }
    else if(!strcmp(buf,pid)){
        SIN.pid=parseInt(value);
    }
    else if(!strcmp(buf,rt)){
        SIN.rt=parseInt(value);
    }
}
float parseFloat(char* buff)
{
    char *endptr = NULL;
    float val = 0;
    errno = 0;
    val = strtof(buff, &endptr);
    if ((errno == ERANGE && (val == FLT_MAX || val == FLT_MIN)) || (errno != 0 && val == 0)) {
        perror("strtof");
        exit(EXIT_FAILURE);
    }

    if (endptr == buff) {
        fprintf(stderr, "No digits were found\n");
        exit(EXIT_FAILURE);
    }
    if ((int) (val * 1000) % 125 != 0)
    {
        perror("Float value should be provided by one eight of a second value");
        exit(EXIT_FAILURE);
    }
    return val;
}
int parseInt(char *buff)
{
    int val = 0;
    char *endptr = NULL;
    errno = 0;
    val = (int) strtol(buff, &endptr, 10);
    if ((errno == ERANGE && (val == INT_MAX || val == INT_MIN)) ||
        (errno != 0 && val == 0)) {
        perror("strtol");
        exit(EXIT_FAILURE);
    }

    if (endptr == buff) {
        fprintf(stderr, "No digits were found\n");
        exit(EXIT_FAILURE);
    }
    return val;
}
void setFdTimer(int timer, int v)
{
    struct timespec temp;
    temp.tv_sec = SIN.probe;
    temp.tv_nsec = (SIN.probe - (temp.tv_sec))*1e9;
    if(v==0) {
        temp.tv_sec = temp.tv_nsec = 0;
        activeProbe=0;
    }
    struct itimerspec ts={temp,temp};

    if (timerfd_settime(timer, 0, &ts, NULL) == -1)
        handle_error("timerfd_settime");
    activeProbe=1;
}
int setPeriodTimer(int timer)
{
    struct itimerspec ts={0};
    if(SIN.period==0)//infinity -> nie uruchamiam tego timera, ale drugi chodzi
    {
        activeProbe=1;
        activePeriod=0;
        return 0;
    }

    else if(SIN.period<0)//zatrzymaj probkowanie, nie uruchamiaj zadnego timera
    {
        activeProbe=0;
        activePeriod=0;
        return -1;//zatrzymaj probkowanie
    }
    else{
        activeProbe=1;
        activePeriod=1;
        struct timespec temp;
        temp.tv_sec = SIN.period;
        temp.tv_nsec = (SIN.period - (temp.tv_sec))*1e9;
        struct timespec zeros={.tv_sec=0,.tv_nsec=0};
        ts.it_interval=zeros;
        ts.it_value=temp;
        }
        if (timerfd_settime(timer, 0, &ts, NULL) == -1)
            handle_error("timerfd_settime");
        return 1;

}
void selectTimer(short int port, int sock_fd) {

    struct sockaddr_in client;
    socklen_t size=sizeof(client);
    char buf[BUFLEN];
    int N = 3;
    struct timespec temp={0};
    struct itimerspec zero={{0,0},{0,0}};
    struct pollfd pollFd[N];
    int nfds=N;

    int timerFd = timerfd_create(CLOCK_REALTIME, 0);
    int timerPeriodFd = timerfd_create(CLOCK_REALTIME, 0);
    if (timerFd == -1 || timerPeriodFd == -1)
        handle_error("timerfd_create");

    //ustalam parametry dla polla
    pollFd[0].fd = timerFd;
    pollFd[1].fd = timerPeriodFd;
    pollFd[2].fd = sock_fd;
    pollFd[0].events = pollFd[1].events =pollFd[2].events= POLLIN;


    setTime(&ref_point);//punkt referencyjny
    if(setPeriodTimer(timerPeriodFd)!=-1 && is_RT(SIN.rt) && !kill(SIN.pid,0)) {
        setFdTimer(timerFd, 1);
        activeProbe=1;
    }

    while (1) {
        int numEvents;
        printf("about to poll\n");
        numEvents = poll(pollFd,nfds, -1);
        if (numEvents == -1)
            handle_error("poll");
        if ((pollFd[0].revents!=0) &&(pollFd[0].revents & POLLIN)==POLLIN)//ready to read from sinus timer
        {
            union sigval sv;
            read(timerFd, 0, sizeof(uint64_t));
            setTime(&temp);
            double time=calculateTime(ref_point,temp);//czas od pkt referencyjnego
            float wynik=(float)sinus(SIN.amp, SIN.freq, time);
            printf("wynik %lf\n",wynik);
            memcpy(&sv.sival_int,&wynik,sizeof(float));
             printf("%d",sv.sival_int);
            int sig=sigqueue(SIN.pid, SIN.rt, sv);
            checkSignal(timerFd,sig);//sprawdam czy sygnal zostal wyslany i czy nr sygnalu ok
            //jesli nie wstrzymuje probkowanie
        }
        else if ((pollFd[1].revents!=0)&&(pollFd[1].revents & POLLIN)==POLLIN)//ready to read period timer
        {
            read(timerPeriodFd, 0, sizeof(uint64_t));
            printf("end\n");
            activePeriod=false;
            activeProbe=false;
            if(timerfd_settime(timerFd,0,&zero,NULL)==-1)//dezaktywuj probkowanie
                handle_error("timer_settime");
        }
        else if ((pollFd[2].revents!=0) && (pollFd[2].revents & POLLIN)==POLLIN)//ready to read datagram
        {
            bzero(buf,sizeof(buf));
            int dl = recvfrom(sock_fd, buf, sizeof(buf), 0, (struct sockaddr *)&client, &size);
            if (dl != -1) {
                int raport=0;
                float old_amp=SIN.amp;
                float old_freq=SIN.freq;
                float old_period=SIN.period;
                float old_probe=SIN.probe;
                parseDatagramm( &raport, buf);
                checkSin(old_amp,old_freq,old_probe,old_period,timerPeriodFd,timerFd);

                if(raport)//jesli w datagramie bylo slowo raport -> odsyla info o biezacej konfiguracji
                {
                    char* message=raport_mes();
                    if(sendto(sock_fd,(const char*)message,sizeof(buf),0,(struct sockaddr *)&client, sizeof(client))==-1)
                        handle_error("sendto");
                }

                //    printf("%f %lf %lf %lf %hd %hd raport %d \n", SIN.amp, SIN.freq, SIN.probe, SIN.period, SIN.pid, SIN.rt,
                //      raport);
            }
        }
    }
}
void checkSin(float a, float f,float probe, float period, int periodtm, int tm){
    if(a!=SIN.amp || f!=SIN.freq || probe!=SIN.probe|| period!=SIN.period)//nastapila zmiana ktoregos parametorow zwiazanych z ukladem
        setTime(&ref_point);//ustawiam nowy set point

    int active=2;//
    bool rt_pid=is_RT(SIN.rt)&& !kill(SIN.pid,0);

    if(period!=SIN.period)//jesli nastapila zmiana, ponownie probuje settowac period timer
        active=setPeriodTimer(periodtm);//jak zwroci -1 to tm ma NIE dzialac
    printf("active %d active %d %f r pid %d",active,activePeriod,SIN.period,rt_pid);
    if( probe!=SIN.probe)//jezeli nastapila zmiana parametru probkowania i jesli jest mozliwe nastawienie timera
    {
        if((active==0 || active==1 )&&rt_pid)
            setFdTimer(tm,1);
    }

    if((activePeriod || SIN.period==0) && rt_pid)//sprawdzam czy rt i pid zostaly poprawione przy dzialajacym periodzie
    {
        setFdTimer(tm, 1);
    }

    if((active==0 || active==1) && rt_pid)//jesli mozna wlaczyc timer probkujacy, bo wczesniej nie dzialal
        setFdTimer(tm,1);

    if(active==-1)//przy probe ==-1
        setFdTimer(tm, 0);

    if(a!=SIN.amp || f!=SIN.freq)//nowy pkt ref przy zmianie parametrow
        setTime(&ref_point);
}
void checkSignal(int fd,int sig){
    if(!is_RT(SIN.rt) || sig==-1)
    {
        setFdTimer(fd,0);
       // printf("zatrzymano bo zly pid lub rt %d rt %d pid\n",cor_rt,cor_pid);
        return;
    }

}
char* raport_mes()
{
    char* message=(char*)malloc(BUFSIZ*sizeof(char));
    sprintf(message,"amp %f freq %lf probe %f ",SIN.amp,SIN.freq,SIN.probe);
    if(!kill(SIN.pid,0))
        strcat(message,"pid OK");
    else
        strcat(message,"pid WRONG");

    if(is_RT(SIN.rt))
        strcat(message," rt OK");
    else
        strcat(message," rt WRONG");

    if(SIN.period>0 && activePeriod==0)//suspended period timer
        strcat(message," suspended");
    else {
        if (SIN.period < 0)
            strcat(message, " stopped");
        if (SIN.period == 0)
            strcat(message, " non-stop");
    }
    return message;
}

