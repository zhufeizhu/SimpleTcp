#ifndef _SIMPLE_TCP_
#define _SIMPLE_TCP_

#define RPOTOCOL_TYPE int

#define GBN 0
#define SR 1
#define ERROR_PROTOCOL 2

#define SERVER_PORT 9960

typedef struct{
    int seq;
    int ack;
    int window;
    int checksum;
} TcpHeader;

typedef struct{
    RPOTOCOL_TYPE pro_type;
    int window_size;
    int msg_len;
    int time_out;
    int seq_range;
} RPOTOCOL;

typedef struct{
    char* data;
    int len;
} Data;

TcpHeader* createTcpHeader(int seq, int ack, int window,int checksum);

char* createTcpMsg(TcpHeader* header,char* buf, int* len);

TcpHeader* getTcpHeader(char* buf,int len);

char* getTcpMsg(char* buf,int *len);

int calculateCRC(int crc, char* buf, int len);

#endif