#include "SimpleTcp.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

TcpHeader* createTcpHeader(int seq, int ack,int window,int checksum){
    TcpHeader* header = (TcpHeader*)malloc(sizeof(TcpHeader));
    header->ack = ack;
    header->seq = seq;
    header->window = window;
    header->checksum = checksum;
    return header;
}

char* createTcpMsg(TcpHeader* header,char* buf, int* len){
    int header_len = sizeof(TcpHeader);
    char* msg = (char*)malloc(header_len + *len);
    memcpy(msg,header,header_len);
    memcpy(msg + header_len, buf, *len);
    *len = *len + header_len;
    return msg;
}

TcpHeader* getTcpHeader(char* buf,int len){
    if(len < sizeof(TcpHeader)){
        return NULL;
    }
    TcpHeader* header = (TcpHeader*)malloc(sizeof(TcpHeader));
    memcpy(header,buf,sizeof(TcpHeader));
    return header;
}

char* getTcpMsg(char* buf,int* len){
    if(*len <= sizeof(TcpHeader)){
        return NULL;
    }
    *len = *len - sizeof(TcpHeader);
    char* msg = (char*)malloc(*len);
    memcpy(msg,buf+sizeof(TcpHeader),*len);
    return msg;
}

int calculateCRC(int crc, char* buf, int len){
    unsigned int byte;
    unsigned char k;
    unsigned short ACC,TOPBIT;
    unsigned short remainder = crc;
    TOPBIT = 0x8000;
    for(byte = 0;byte < len; ++byte){
        ACC = buf[byte];
        remainder ^= (ACC << 8);
        for(k = 8; k > 0;--k){
            if(remainder & TOPBIT){
                remainder = (remainder<<1)^0x8005;
            }else{
                remainder = (remainder<<1);
            }
        }
    }
    remainder = remainder^0x0000;
    return remainder;
}