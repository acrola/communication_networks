//
// Created by mattan on 28/11/16.
//


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

void sendall(int sock, char *buf, int *len) {
    /* sendall code as seen in recitation*/
    int total = 0; /* how many bytes we've sent */
    int bytesleft = *len; /* how many we have left to send */
    int n;
    while (total < *len) {
        n = send(sock, buf + total, bytesleft, 0);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
    }
    *len = total; /* return number actually sent here */
    if (n == -1) {
        perror("Failed sending through socket");
        exit(errno);
    }
}

int sendall_imm(int sock, char *buf, int len) {
    return sendall(sock, buf, &len);
}

void recvall(int sock, char *buf, int *len) {
    int total = 0; /* how many bytes we've read */
    int bytesleft = *len; /* how many we have left to read */
    int n;
    while (total < *len) {
        n = recv(sock, buf + total, bytesleft, 0);
        if (n == -1) { break; }
        if (n == 0) {
            *len = 0;
            n = -1;
            break;
        }
        total += n;
        bytesleft -= n;
    }
    *len = total; /* return number actually sent here */
    if (n == -1) {
        perror("Failed sending through socket");
        exit(errno);
    }
}


int recvall_imm(int sock, char *buf, int len) {
    return recvall(sock, buf, &len);
}

void send_char(int sock, char c) {
    sendall_imm(sock, &c, 1);
}

char recv_char(int sock) {
    char c;
    return recvall_imm(sock, &c, 1);
}

short recieveTwoBytesAndCastToShort(int sock) {
    char twoBytesFromServer[2];
    short* result;
    recvall_imm(sock, twoBytesFromServer, 2);
    result = (short*)twoBytesFromServer;
    return *result;
}

void getData(int sock, char* buff) {
    short bytesToRead = recieveTwoBytesAndCastToShort(sock);
    recvall_imm(sock, buff, bytesToRead);
    buff[bytesToRead] = '\0';
}
void sendData(int sock, char* buff) {
    short bytesToSend = strlen(buff);
    sendall_imm(sock, buff, bytesToSend);
    send_char(sock, '\0');
}

void sendToClientPrint(int sock, char* msg) {
    send_char(sock, OP_PRINT);
    sendData(sock, msg);
}

void sendHalt(int sock) {
    send_char(sock, OP_HALT);
}