#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <stdbool.h>

#ifndef COMMUNICATION_NETWORKS_MAIL_COMMON_H
#define COMMUNICATION_NETWORKS_MAIL_COMMON_H

#define DEFAULT_PORT 6423
#define TOTAL_TO 20
#define MAXMAILS 32000
#define MAX_USERNAME 50
#define MAX_PASSWORD 50
#define MAX_SUBJECT 100
#define MAX_CONTENT 2000
#define NUM_OF_CLIENTS 20
#define MAX_MAIL_MSG 4096

#define OP_SHOWINBOX 's'
#define OP_GETMAIL 'g'
#define OP_DELETEMAIL 'd'
#define OP_COMPOSE 'c'
#define OP_PRINT 'p'
#define OP_HALT 'h'
#define OP_QUIT 'q'

#define LOG_SUCCESS 's'
#define LOG_FAILURE 'f'
#define LOG_KILL 'k'
#define LOG_INIT 'i'

void sendall(int sock, char *buf, int *len);

void sendall_imm(int sock, char *buf, int len);

void recvall(int sock, char *buf, int *len);

void recvall_imm(int sock, char *buf, int len);

void send_char(int sock, char c);

char recv_char(int sock);

short recieveTwoBytesAndCastToShort(int sock);

void getData(int sock, char *buff);

void sendData(int sock, char *buff);

void sendToClientPrint(int sock, char *msg);

void sendHalt(int sock);

#endif //COMMUNICATION_NETWORKS_MAIL_COMMON_H
