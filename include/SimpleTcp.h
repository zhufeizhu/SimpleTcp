#ifndef _SIMPLE_TCP_
#define _SIMPLE_TCP_

#define MsgType char

#define SYN 0b00000001
#define ACK 0b00000010
#define FIN 0b00000100

typedef struct{
    int seq;
    int ack;
    MsgType type;
    int window;
    int checksum;
} TcpHeader;

typedef struct{
    char* data;
    int len;
} Data;

TcpHeader* createTcpHeader(int seq, int ack, MsgType type,int window,int checksum);

char* createTcpMsg(TcpHeader* header,char* buf, int* len);

TcpHeader* getTcpHeader(char* buf,int len);

char* getTcpMsg(char* buf,int *len);

int calculateCRC(int crc, char* buf, int len);

#define GBN 0
#define SR 1
#define ERROR_PROTOCOL 2

#endif