#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "SimpleTcp.h"
#include <time.h>
#include <unistd.h>

#define RECV_PORT 996
#define SEND_PORT 998

#define MAX_SIZE 100

struct sockaddr_in rev_addr,send_addr;
int sockfd;
int base = 0;
int next_seq = 0;
int is_timeout = 0;
int max_seq = 1024;
int isclock = 1;
time_t old_time;
int last_seq = -1;
int close_timer = 0;
int lost_pac = 0;

int WINDOW_SIZE;
int MAX_BYTE;
int PROTOCOL;
int MAX_SEQ;
int timeout;

int re_num = 0;
int origin_num = 0;

pthread_mutex_t mutex;

Data* slide_window[MAX_SIZE];
int window_flag[MAX_SIZE];
time_t old_times[MAX_SIZE];

void start_timer(int index){
    time(&old_times[index]);
}

void stop_timer(int index){
    old_times[index] = 0;
}

void closeTcpConnect(){
    printf("close Tcp Connect!\n");
    TcpHeader* header = createTcpHeader(0,0,FIN,0,0);
    sendto(sockfd,header,sizeof(TcpHeader),0,(struct sockaddr*)(&rev_addr),sizeof(rev_addr));
}

void* recvGBNAck(){
    printf("start recv ack\n");
    char buf[MAX_BYTE + 100];
    int count = 0;
    while(1){
        recvfrom(sockfd,buf,MAX_BYTE+100,0,NULL,NULL);
        TcpHeader header;
        memcpy(&header,buf,sizeof(TcpHeader));
        if(header.type != ACK)  continue;
        printf("ACK %d received\n",header.ack);
        if(header.ack == last_seq){
            printf("revc all ack and close connection!\n");
            stop_timer(0);
            closeTcpConnect();
            close_timer = 1;
            break;
        }
        base = header.ack + 1;
        printf("Current window = [");
        for(int i = base; i < (base + WINDOW_SIZE); i++){
            if(i == (base + WINDOW_SIZE) - 1) printf("%d",i);
            else printf("%d, ",i);
        }
        printf("]\n");
        if (base == next_seq) {
            stop_timer(0);
        } else {
            start_timer(0);
        }
    }
    pthread_exit(pthread_self());
}

void* recvSRAck(){
    printf("start recv ack\n");
    char buf[MAX_BYTE + 100];
    int count = 0;
    while(1){
        recvfrom(sockfd,buf,MAX_BYTE+100,0,NULL,NULL);
        TcpHeader header;
        memcpy(&header,buf,sizeof(TcpHeader));
        if(header.type != ACK)  continue;
        printf("ACK %d received\n",header.ack);
        if(header.ack >= base && header.ack <= base+WINDOW_SIZE){
            pthread_mutex_lock(&mutex);
            free(slide_window[header.ack%WINDOW_SIZE]->data);
            slide_window[header.ack%WINDOW_SIZE]->data=NULL;
            window_flag[header.ack%WINDOW_SIZE] = 1;
            stop_timer(header.ack%WINDOW_SIZE);
            pthread_mutex_unlock(&mutex);
            /*如果ack==base 将slide_window向后移动到next_seq*/
            if(header.ack == base){
                printf("windos slide %d\n",base);
                int index = base%WINDOW_SIZE;
                while(window_flag[index]){
                    window_flag[index] = 0;
                    index = (index + 1)%WINDOW_SIZE;
                    base++;
                }
            }
            printf("Current window = [");
            for(int i = base; i < (base + WINDOW_SIZE); i++){
                if(i == (base + WINDOW_SIZE) - 1) printf("%d",i);
                else printf("%d, ",i);
            }
            printf("]\n");
            if(header.ack == last_seq){
                close_timer = 1;
                closeTcpConnect();
                break;
            }    
        }
    }
    pthread_exit(pthread_self());
}

void onGBNTimeout(){
    printf("Packet %d *****Timed Out*****\n",base);
    start_timer(0);
    int start = base%WINDOW_SIZE;
    int end = (next_seq-1)%WINDOW_SIZE;
    if(start == end){
        return;
    }else if(start < end){
        for(int i = start; i <= end; i++){
            if( last_seq != -1 && (base+i) > last_seq) break;
            Data* data = slide_window[i];
            sendto(sockfd,data->data,data->len,0,(struct sockaddr*)(&rev_addr),sizeof(rev_addr));
            re_num++;
            printf("Packet %d re-transmitted\n",base + i);
        }
    } else {
        for(int i = start; i < WINDOW_SIZE; i++){
            if( last_seq != -1 && (base+i) > last_seq) break;
            Data* data = slide_window[i];
            sendto(sockfd,data->data,data->len,0,(struct sockaddr*)(&rev_addr),sizeof(rev_addr));
            re_num++;
            printf("Packet %d re-transmitted\n",base + i);
        }
        for(int i = 0; i <= end; i++){
            if( last_seq != -1 && (base+i) > last_seq) break;
            Data* data = slide_window[i];
            sendto(sockfd,data->data,data->len,0,(struct sockaddr*)(&rev_addr),sizeof(rev_addr));
            re_num++;
            printf("Packet %d re-transmitted\n",base + i + WINDOW_SIZE - start - 1);
        }
    } 
}

void onSRTimeout(int index){
    start_timer(index);
    Data* data = slide_window[index];
    sendto(sockfd,data->data,data->len,0,(struct sockaddr*)(&rev_addr),sizeof(rev_addr));
    printf("Packet %d re-transmitted\n",base + index);
    re_num++;
}

void* timer(){
    time_t cur_time;
    while(!close_timer){
        if(isclock){
            time(&cur_time);
            if(PROTOCOL == SR){
                for(int i = 0; i < WINDOW_SIZE; i++){
                    if (old_times[i] != 0 && (cur_time - old_times[i]) > timeout){
                        onSRTimeout(i);
                    }
                }
            }
            if(PROTOCOL == GBN){
                if ((cur_time - old_times[0]) > timeout){
                    onGBNTimeout();
                }
            }
            
        }
        sleep(1);
    }
    pthread_exit(pthread_self());
}

int establishTcpConnect(){
    printf("**********************************\n");
    printf("** Establish Tcp Connect Start! **\n");
    TcpHeader* header = createTcpHeader(0,0,SYN,0,0);
    sendto(sockfd,header,sizeof(TcpHeader),0,(struct sockaddr*)(&rev_addr),sizeof(rev_addr));
    recvfrom(sockfd,(void*)header,sizeof(TcpHeader),0,NULL,NULL);
    if(header->type != (SYN&ACK) || header->ack != 1){
        printf("** Establish Tcp Connect Failed!**\n");
        printf("**********************************\n\n");
        return 0;
    }
    int ack = header->ack;
    int seq = header->seq;
    free(header);
    header = createTcpHeader(ack,seq+1,ACK,0,0);
    sendto(sockfd,header,sizeof(TcpHeader),0,(struct sockaddr*)(&rev_addr),sizeof(rev_addr));
    printf("** Establish Tcp Connect Succeed**\n");
    printf("**********************************\n\n");
    return 1;
}


pthread_t startAckReceiver(){
    pthread_t pid;
    if(PROTOCOL == GBN){
        if(!pthread_create(&pid,NULL,&recvGBNAck,NULL)){
            return pid;
        } else {
            return 0;
        }
    }
    if(PROTOCOL == SR){
        if(!pthread_create(&pid,NULL,&recvSRAck,NULL)){
            return pid;
        } else {
            return 0;
        }
    }
}

pthread_t startTimer(){
    pthread_t pid;
    if(!pthread_create(&pid,NULL,&timer,NULL)){
        return pid;
    } else {
        return 0;
    }
}

void initSocket(){
    sockfd = socket(AF_INET,SOCK_DGRAM,0);
    bzero(&rev_addr,sizeof(rev_addr));
    rev_addr.sin_family = AF_INET;
    rev_addr.sin_port = htons(RECV_PORT);
    inet_pton(AF_INET,"127.0.0.1",&rev_addr.sin_addr);
    send_addr.sin_family = AF_INET;
    send_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    send_addr.sin_port = htons(SEND_PORT);
    bind(sockfd,(struct sockaddr*)&send_addr,sizeof(send_addr));
    memset(&window_flag,0,WINDOW_SIZE);
    for(int i = 0; i < WINDOW_SIZE; i++){
        old_times[i] = 0;
        slide_window[i] = NULL;
    }
    pthread_mutex_init(&mutex,NULL);
}

int main(int argc, char** argv){
    // if(argc < 2) {
    //     printf("no input file\n");
    //     return 0;
    // }
    int c;
    while((c = getopt(argc,argv,"p:l:s:r:t:")) != -1){
        switch(c){
            case 'p':{
                if(strcmp("GBN",optarg)==0){
                    PROTOCOL = GBN;
                } else if(strcmp("SR",optarg) == 0){
                    PROTOCOL = SR;
                } else {
                    PROTOCOL = ERROR_PROTOCOL;
                }
                break;
            }
            case 'l':{
                MAX_BYTE = atoi(optarg);
                break;
            }
            case 's':{
                WINDOW_SIZE = atoi(optarg);
                break;
            }
            case 'r':{
                MAX_SEQ = atoi(optarg);
                break;
            }
            case 't':{
                timeout = atoi(optarg);
            }
        }
    }
    printf("MAX BYTE is %d\n\n",MAX_BYTE);
    FILE* file =  fopen("../test/input.txt","r");
    if(!file){
        printf("open file failed\n");
        return 0;
    }
    initSocket();
    if(!establishTcpConnect()){
        return 0;
    }
    pthread_t recv_id = startAckReceiver();
    pthread_t timer_id = startTimer();
    if (recv_id == 0 || timer_id == 0){
        printf("init error!\n");
        return 0;
    }
    char buf[MAX_BYTE];
    time_t start,end;
    time(&start);
    while(1){
        if (next_seq >=(base + WINDOW_SIZE)){
            // printf("wait %d %d\n",next_seq,base);
            continue;
        }
        int n = fread(buf,1,MAX_BYTE,file);
        /*计算crc*/
        int crc = calculateCRC(0,buf,n);
        TcpHeader* header = createTcpHeader(next_seq,0,0,0,crc);
        char* msg = createTcpMsg(header,buf,&n);
        /*模拟丢失数据*/
        if(next_seq == 10 && lost_pac ==0){
            lost_pac++;
        }else{
            sendto(sockfd,msg,n,0,(struct sockaddr*)(&rev_addr),sizeof(rev_addr));
        }
        origin_num++;
        printf("Packet %d sent\n",next_seq);
        pthread_mutex_lock(&mutex);
        if(slide_window[next_seq%WINDOW_SIZE]){
            if(slide_window[next_seq%WINDOW_SIZE]->data)
                free(slide_window[next_seq%WINDOW_SIZE]->data);
        }else{
            Data* data = (Data*)malloc(sizeof(Data));
            slide_window[next_seq%WINDOW_SIZE] = data;
        }
        slide_window[next_seq%WINDOW_SIZE]->data = (char*)malloc(n);
        slide_window[next_seq%WINDOW_SIZE]->len = n;
        memcpy(slide_window[next_seq%WINDOW_SIZE]->data,msg,n);
        if(PROTOCOL == GBN){
            if(base == next_seq){
                start_timer(0);
            }
        }else{
            window_flag[next_seq%WINDOW_SIZE] = 0;
            start_timer(next_seq%WINDOW_SIZE);
        }
        pthread_mutex_unlock(&mutex);
        next_seq++;
        if(feof(file)){
            printf("send all data\n");
            free(header);
            free(msg);
            last_seq = next_seq - 1;
            break;
        }
    }
    pthread_join(recv_id,NULL);
    pthread_join(timer_id,NULL);
    time(&end);
    printf("\nSession successfully terminated\n");
    int total_time = end-start;
    printf("\nNumber of original packets sent:%d\n",origin_num);
    printf("Number of retransmitted packets:%d\n",re_num);
    printf("Total elapsed time:%d\n",total_time);
    printf("Total throughput %.4fMbps\n",1.0*((origin_num+re_num)*MAX_BYTE)/1024/1024/total_time);
    printf("Effective throughput %.4fMbps\n",1.0*(origin_num*MAX_BYTE)/1024/1024/total_time);
    return 0;
}