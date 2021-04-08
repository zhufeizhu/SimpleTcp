#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

extern int errno;

#include "SimpleTcp.h"

#define RECV_PORT 996

#define MAX_SIZE 100

int WINDOW_SIZE;
int MAX_BYTE;
int PROTOCOL;
int MAX_SEQ;
int TIMEOUT;

/*receiver sock fd*/
int sockfd;


struct sockaddr_in addr;
socklen_t len = sizeof(addr);
int expected_num = 0;
int ack_lost = 0;
int recv_base = 0;
FILE *output;
int last_seq = -1;
int re_num = 0;
int original_num = 0;
char* last_buf = NULL;
int base = 0;

Data* slide_windwos[MAX_SIZE];
int flag_windows[MAX_SIZE];

void sendUdt(char* msg,int n){
    fwrite(msg,1,n,output);
    fflush(output);
}

/*build Tcp connection and transport protocol window_sie .etc message*/
int initSocket(){
    printf("**********************************\n");
    printf("** Establish Tcp Connect Start! **\n");
    sockfd = socket(AF_INET,SOCK_STREAM,0);
    bzero(&addr,sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);

    inet_pton(AF_INET,"127.0.0.1",&addr.sin_addr.s_addr);

    int ret = connect(sockfd,(struct sockaddr*)&addr,sizeof(addr));

    if (ret){
        printf("Establish Tcp Connect Failed!\n");
        printf("**********************************\n\n");
        return 0;
    }
    printf("** Establish Tcp Connect Succeed *\n");
    printf("**********************************\n\n");

    char buf[100];
    printf("Please input the arguments for Tcp connection!\n");
    printf("RPOTOCOL: ");
    scanf("%s",buf);
    if(strcmp(buf,"GBN")==0){
        PROTOCOL = GBN;
    } else if(strcmp(buf,"SR")){
        PROTOCOL = SR;
    } else {
        PROTOCOL = ERROR_PROTOCOL;
        return 0;
    }
    printf("slide_window size: ");
    scanf("%d",&WINDOW_SIZE);

    printf("transport data size: ");
    scanf("%d",&MAX_BYTE);

    printf("time out: ");
    scanf("%d",&TIMEOUT);

    printf("range of sequence: ");
    scanf("%d",&MAX_SEQ);

    Protocol pro;
    pro.pro_type = PROTOCOL;
    pro.window_size = (PROTOCOL==GBN)?1:WINDOW_SIZE;
    pro.msg_len = MAX_BYTE;
    pro.time_out = TIMEOUT;
    pro.seq_range = MAX_SEQ;
    memcpy(buf,&pro,sizeof(Protocol));
    int n = send(sockfd,buf,sizeof(Protocol),0);
    if(n <= 0){
        printf("send initial msg failed!\n");
        return 0;
    }
    printf("send initial msg succeed!\n");
    memset(flag_windows,0,WINDOW_SIZE);
    for(int i = 0; i < WINDOW_SIZE; i++){
        slide_windwos[i] = NULL;
    }
    last_buf = (char*)malloc(MAX_BYTE);
}

int onGBNReceiveMsg(TcpHeader* header,char* msg, int n){
    int ack = expected_num - 1;
    if (header->seq == expected_num){
        int res = calculateCRC(0,msg,n);
        if(res != header->checksum){
            printf("CheckSum Failed!\n");
            return -1;
        }else{
            printf("CheckSum OK!\n");
        }
        memcpy(last_buf,msg,n);
        if(n < MAX_BYTE) last_buf[n]='\0';
        sendUdt(msg,n);
        ack = header->seq;
        expected_num++;
        printf("current window is [%d]\n",expected_num);
        original_num++;
    }else{
        re_num++;
    }
    return ack;
}

void ReplyAck(int ack){
    TcpHeader *ackHeader = createTcpHeader(0,ack,0,0);
    int n = send(sockfd,ackHeader,sizeof(TcpHeader),0);
    if(n <= 0){
        printf("Ack send Failed!\n");
        return;
    }
    printf("Ack %d sent\n",ack);
    free(ackHeader);
}

int onSRReceiveMsg(TcpHeader* header, char* msg, int n ){
    int res = calculateCRC(0,msg,n);
    if(res != header->checksum){
        re_num++;
        printf("CheckSum Failed!\n");
        return -1;
    } else {
        printf("CheckSum OK!\n");
    }
    int seq = header->seq;
    if((seq >= recv_base) && seq < (recv_base + WINDOW_SIZE)){
        original_num++;
        /*失序*/
        if(seq != recv_base){
            if(slide_windwos[seq%WINDOW_SIZE]){
                free(slide_windwos[seq%WINDOW_SIZE]);
                slide_windwos[seq%WINDOW_SIZE] = NULL;
            }
            Data* data = (Data*)malloc(sizeof(Data));
            data->data = (char*)malloc(n);
            memcpy(data->data,msg,n);
            data->len = n;
            slide_windwos[seq%WINDOW_SIZE] = data;
            flag_windows[seq%WINDOW_SIZE] = 1;
        } else {
            memcpy(last_buf,msg,n);
            if(n < MAX_BYTE) last_buf[n]='\0';
            sendUdt(msg,n);
            recv_base++;
            int index = recv_base%WINDOW_SIZE;
            /*将缓存的数据传走*/
            while(flag_windows[index]){
                memcpy(last_buf,slide_windwos[index]->data,slide_windwos[index]->len);
                if(slide_windwos[index]->len < MAX_BYTE) last_buf[slide_windwos[index]->len]='\0';
                sendUdt(slide_windwos[index]->data,slide_windwos[index]->len);
                free(slide_windwos[index]);
                slide_windwos[index] = NULL;
                flag_windows[index] = 0;
                index = (index + 1)%WINDOW_SIZE;
                recv_base++;
            }
        }
        printf("Current window [");
        int start = recv_base%WINDOW_SIZE;
        for(int i = 0; i < WINDOW_SIZE; i++){
            if(flag_windows[(start+i)%WINDOW_SIZE]){
                printf("%d, ",recv_base + i);
            }
        }
        printf("]\n");
        return seq;
    } else {
        if( seq >= recv_base - WINDOW_SIZE && seq <= recv_base -1){
            printf("Current window [");
            int start = recv_base%WINDOW_SIZE;
            for(int i = 0; i < WINDOW_SIZE; i++){
                if(flag_windows[(start+i)%WINDOW_SIZE]){
                    printf("%d, ",recv_base + i);
                }
            }
            printf("]\n");
            return seq;
        }
    }
    return -1;
}

int main(int argc, char** argv){
    /*如果socket初始化失败 程序退出*/
    if(!initSocket()) return 0;
    output = fopen("/Users/zhuqs/workspace/dai/SimpelTCP/version1/test/output.txt","w");
    if(!output){
        printf("errno is %d\n",errno);
        printf("open out file failed\n");
        return 0;
    }
    char buf[MAX_BYTE + 100];
    int last_len = 0;
    while(1){
        int n = recv(sockfd,buf,MAX_BYTE + 100,0);
        if(n <= 0){
            printf("recv all data!\n");
            break;
        }
        TcpHeader* header = getTcpHeader(buf,n);
        char* msg = getTcpMsg(buf,&n);
        last_seq = header->seq;
        int ack;
        if(PROTOCOL == GBN){
            ack = onGBNReceiveMsg(header,msg,n);
        }else{
            ack = onSRReceiveMsg(header,msg,n);
        }
        if(ack >=0 ) ReplyAck(ack);
        free(header);
        free(msg);
    }
    printf("\n\nLast packet seq %d received:%s\n",last_seq,last_buf);
    printf("Number of original packets received:%d\n",original_num);
    printf("Number of retransmitted packets received:%d\n",re_num);
    return 0;
}