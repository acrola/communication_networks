#include "mail_common.h"

void sendall(int sockfd, char *buf, int *len)
{
    /* sendall code as seen in recitation */
    int total = 0; /* how many bytes we've sent */
    int bytesleft = *len; /* how many we have left to send */
    int n = 0;
    while (total < *len)
    {
        n = send(sockfd, buf + total, (size_t) bytesleft, 0);
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
        perror("Failed sending data");
        exit(errno);
    }
}

void sendall_imm(int sockfd, void *buf, int len)
{
    sendall(sockfd, buf, &len);
}

int recvall(int sockfd, char *buf, int *len)
{
    int total = 0; /* how many bytes we've read */
    int bytesleft = *len; /* how many we have left to read */
    int n = 0;
    while (total < *len)
    {
        n = recv(sockfd, buf + total, (size_t) bytesleft, 0);
        if (n < 0)
        {
            perror("Failed receiving data");
            tryClose(sockfd);
            exit(errno);
        }
        if (n == 0) /* means the sender has closed the connection */
        {
            *len = 0;
            break;
        }
        total += n;
        bytesleft -= n;
    }
    *len = total; /* return number actually sent here */
    return n;
}


int recvall_imm(int sockfd, void *buf, int len)
{
    return recvall(sockfd, buf, &len);
}

void send_char(int sockfd, char c)
{
    char toSend = c;
    trySysCall(send(sockfd, &toSend, 1, 0), "Failed to send char", sockfd);
}

char recv_char(int sockfd)
{
    char c;
    trySysCall(recv(sockfd, &c, 1, 0), "Failed to receive char", sockfd);
    return c;
}

short getDataSize(int sockfd)
{
    short dataSize;
    if (recvall_imm(sockfd, &dataSize, sizeof(dataSize)) == 0)
    {
        return 0;
    }
    /* set to host bytes order */
    dataSize = ntohs((uint16_t) dataSize);
    return dataSize;
}


int recvData(int sockfd, char *buf)
{
    short dataSize = getDataSize(sockfd);
    int n = recvall_imm(sockfd, buf, dataSize);
    buf[dataSize] = '\0'; /* null terminator to terminate the string */
    /* if both dataSize and n are positive it means that the other side is still connected */
    return (dataSize && n) ? 1 : 0;

}

void sendData(int sockfd, char *buf)
{
    short dataSize = (short) strlen(buf);
    /* cast to network order */
    short dataSize_networkOrder = htons((uint16_t) dataSize);
    /* send data size */
    sendall_imm(sockfd, &dataSize_networkOrder, sizeof(dataSize_networkOrder));
    /* send data */
    sendall_imm(sockfd, buf, dataSize);
}

void tryClose(int sockfd)
{
    if (sockfd == -1) /* invalid sockfd - occurs only if "socket" fails, and it is in prpose */
    {
        return;
    }
    if (close(sockfd) < 0)
    {
        perror("Could not close socket");
        exit(errno);
    }
}

void trySysCall(int syscallResult, const char *msg, int sockfd)
{
    if (syscallResult < 0)
    {
        perror(msg);
        tryClose(sockfd);
        exit(errno);
    }
}

void shutdownSocket(int sock)
{
    int res = 0;
    char buff[10];
    /* send shtdwn message, and stop writing */
    shutdown(sock, SHUT_WR);
    while (1)
    {
        /* read until the end of the stream */
        res = recv(sock, buff, 10, 0);
        if (res < 0)
        {
            perror("Shutdown error");
            exit(errno);
        }
        if (res == 0)
        {
            break;
        }
    }
    tryClose(sock);
    exit(EXIT_SUCCESS);
}

void handleUnexpectedError(const char *errorMsg, int sockfd)
{
    printf("An unexpected error occurred: %s.\nExiting...", errorMsg);
    tryClose(sockfd);
    exit(EXIT_FAILURE);
}
