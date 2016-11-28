#include "mail_common.h"

void sendall(int sock, char *buf, int *len)
{
    /* sendall code as seen in recitation*/
    int total = 0; /* how many bytes we've sent */
    int bytesleft = *len; /* how many we have left to send */
    int n = 0;
    while (total < *len)
    {
        n = send(sock, buf + total, (size_t) bytesleft, 0);
        if (n == -1)
        {
            break;
        }
        total += n;
        bytesleft -= n;
    }
    *len = total; /* return number actually sent here */
    if (n == -1)
    {
        perror("Failed sending through socket");
        exit(errno);
    }
}

void sendall_imm(int sock, char *buf, int len)
{
    sendall(sock, buf, &len);
}

void recvall(int sock, char *buf, int *len)
{
    int total = 0; /* how many bytes we've read */
    int bytesleft = *len; /* how many we have left to read */
    int n = 0;
    while (total < *len)
    {
        n = recv(sock, buf + total, (size_t) bytesleft, 0);
        if (n == -1)
        {
            break;
        }
        if (n == 0)
        {
            *len = 0;
            n = -1;
            break;
        }
        total += n;
        bytesleft -= n;
    }
    *len = total; /* return number actually sent here */
    if (n == -1)
    {
        perror("Failed sending through socket");
        exit(errno);
    }
}


void recvall_imm(int sock, char *buf, int len)
{
    recvall(sock, buf, &len);
}

void send_char(int sock, char c)
{
    sendall_imm(sock, &c, 1);
}

char recv_char(int sock)
{
    char c;
    recvall_imm(sock, &c, 1);
    return c;
}

short recieveTwoBytesAndCastToShort(int sock)
{
    char twoBytesFromServer[2];
    short *result;
    recvall_imm(sock, twoBytesFromServer, 2);
    result = (short *) twoBytesFromServer;
    return *result;
}


void getData(int sock, char *buff)
{
    short bytesToRead = recieveTwoBytesAndCastToShort(sock);
    recvall_imm(sock, buff, bytesToRead);
    buff[bytesToRead] = '\0';
}

void sendData(int sock, char *buff)
{
    short bytesToSend = (short) strlen(buff);
    sendall_imm(sock, buff, bytesToSend);
    send_char(sock, '\0');
}

void sendToClientPrint(int sock, char *msg)
{
    send_char(sock, OP_PRINT);
    sendData(sock, msg);
}

void sendHalt(int sock)
{
    send_char(sock, OP_HALT);
}