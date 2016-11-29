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

#ifndef MAIL_COMMON_H
#define MAIL_COMMON_H

#define DEFAULT_HOSTNAME "localhost"
#define DEFAULT_PORT "6423"
#define TOTAL_TO 20
#define MAXMAILS 32000
#define MAX_USERNAME 50
#define MAX_PASSWORD 50
#define MAX_SUBJECT 100
#define MAX_CONTENT 2000
#define NUM_OF_CLIENTS 20
#define BUF_SIZE 4096
#define ERROR_BUF_SIZE 100
#define MAX_LOGIN_ATTEMPTS 3

#define OP_SHOWINBOX 's'
#define OP_GETMAIL 'g'
#define OP_DELETEMAIL 'd'
#define OP_COMPOSE 'c'
#define OP_PRINT 'p'
#define OP_HALT 'h'
#define OP_QUIT 'q'
#define OP_ERROR '0'

#define LOG_REQUEST 'r'
#define LOG_KILL 'k'


void sendall(int sockfd, void *buf, int *len);

void sendall_imm(int sockfd, void *buf, int len);

void recvall(int sockfd, void *buf, int *len);

void recvall_imm(int sockfd, void *buf, int len);

void send_char(int sockfd, char c);

char recv_char(int sockfd);

short getDataSize(int sockfd);

void recvData(int sockfd, char *buf);

void sendData(int sockfd, char *buf);

void trySysCall(int syscallResult, const char *msg, int sockfd);

void tryClose(int sockfd);

void shutdownSocket(int sock);

void handleUnexpectedError(const char *errorMsg, int sockfd);

#endif //MAIL_COMMON_H
