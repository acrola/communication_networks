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
#include <mail_common.c>


#define BUF_SIZE 4096
#define MAX_USERNAME_LENGTH 32
#define MAX_PASSWORD_LENGTH 32
#define MAX_INPUT_ATTEMPTS 3

#define SHOW_INBOX_STR "SHOW_INBOX"
#define GET_MAIL_STR "GET_MAIL"
#define DELETE_MAIL_STR "DELETE_MAIL"
#define COMPOSE_STR "COMPOSE"
#define QUIT_STR "QUIT"



typedef enum LoginState_t {
    ASK_USER,
    ASK_PASS,
    LOGIN_SUCCESS,
    LOGIN_FAILURE,
    LOGIN_KILL

} LoginState;



typedef enum _eOpCode
{
    OPCODE_ERROR = 0,
    OPCODE_SHOW_INBOX = 1,
    OPCODE_GET_MAIL = 2,
    OPCODE_DELETE_MAIL = 3,
    OPCODE_COMPOSE = 4,
    OPCODE_QUIT = 5
} eOpCode;


void shutdownSocket(int sock);

int sendall(int sock, void *buf, int *len);

void connectToServer(int sockfd, char *hostname, char *port);

void tryClose(int sockfd);

int recvall(int sock, void *buf, int *len);

void analyzeProgramArguments(int argc, char *argv[], char **hostname, char **port);

void validateUser(int sockfd);

void establishInitialConnection(int sockfd, char *hostname, char *port);

void recvData(int sockfd, char *buf);

void trySysCall(int syscallResult, const char *msg, int sockfd);

void validateUser(int sockfd);

void sendData(int sockfd, char *buf);

void getInputFromUser(char *fieldBuf, const char *fieldPrefix, const char *fieldName, int maxFieldLength, int sockfd);

eOpCode getOpCode(char *token);

int main(int argc, char *argv[])
{
    uint16_t mailId;
    int sockfd, clientIsActive, mailIdSize = sizeof(mailId), mailLength;
    char *hostname, *port, buf[BUF_SIZE], *token, serverResult;


    analyzeProgramArguments(argc, argv, &hostname, &port);

    /* open TCP socket with IPv4*/
    // (no opened socket yet - hence we pass a -1 as a sockfd)
    trySysCall((sockfd = socket(PF_INET, SOCK_STREAM, 0)), "Error in opening client's socket", -1);

    /* connect to server*/
    connectToServer(sockfd, hostname, port);

    clientIsActive = 1;
    // loop runs as long as the client wants to get data and no errors occur.
    // if the client quits or an error occurs - the clientIsActive is set to 0,
    // we get out of the loop and shutdown the client program gracefully
    while (clientIsActive)
    {
        //zero buf
        memset(buf, 0, BUF_SIZE);
        //get input from user
        if (!fgets(buf, BUF_SIZE, stdin))
        {
            printf("Error getting input from user\n");
            break;
        }
        token = strtok(buf, " \t\r");
        char opCode = (char) getOpCode(token);

        // if opcode is valid - send it to the server (o.w we ask the user to try again)
        if (opCode != OPCODE_ERROR)
        {
            trySysCall(send(sockfd, &opCode, sizeof(char), 0), "Sending opcode to server failed", 0);
        }
        switch (opCode)
        {
            case OPCODE_SHOW_INBOX:
                //todo - what about the edge case where a user has 32000 mails of maximal length..?
                // receive inbox data
                recvData(sockfd, buf);
                printf("%s", buf);
                break;
            case OPCODE_GET_MAIL:
                token = strtok(NULL, " \t\r\n");
                mailId = htons((uint16_t) strtol(token, NULL, 10));

                // send mail id
                if (sendall(sockfd, &mailId, &mailIdSize) < 0)
                {
                    perror("Sending mail ID to server failed");
                    clientIsActive = 0;
                    break;
                }
                // receive data
                recvData(sockfd, buf);
                printf("%s", buf);
                break;
            case OPCODE_DELETE_MAIL:
                token = strtok(NULL, " \t\r\n");
                mailId = htons((uint16_t) strtol(token, NULL, 10));

                // send mail id
                if (sendall(sockfd, &mailId, &mailIdSize) < 0)
                {
                    perror("Sending mail ID to server failed");
                    clientIsActive = 0;
                    break;
                }
                //todo - maybe receive an OK from the server (that the mail was actually deleted)?
                break;
            case OPCODE_COMPOSE:
                /*todo - proposed protocol
                 * generally - get the data from the user, send it (raw!) to the server.
                 * the srver handles it and sends a 1-byte ack to the client
                 * the client prints a success or error message
                 */
                mailLength = 0;
                //3 loops - To, Subject, Text
                for (int i = 0; i < 3; ++i)
                {
                    if (!fgets(buf + mailLength, BUF_SIZE - mailLength, stdin))
                    {
                        printf("Error getting input from user (compose)\n");
                        break;
                    }
                    //update mail length
                    mailLength = strlen(buf);

                }

                sendData(sockfd, buf);
                printf("%s", buf);
                break;
            case OPCODE_QUIT:
                //todo - do we really need to let the server know we're quitting? if not - change above..
                clientIsActive = 0;
                break;
            case OPCODE_ERROR:
            default:
                printf("Invalid operation. Please try again.\n");
                break;
        }
    }
    //close socket and exit
    tryClose(sockfd);
    return EXIT_SUCCESS;
}

eOpCode getOpCode(char *token)
{
    if (!strcmp(token, SHOW_INBOX_STR))
    {
        return OPCODE_SHOW_INBOX;
    }
    else if (!strcmp(token, GET_MAIL_STR))
    {
        return OPCODE_GET_MAIL;
    }
    else if (!strcmp(token, DELETE_MAIL_STR))
    {
        return OPCODE_DELETE_MAIL;
    }
    else if (!strcmp(token, COMPOSE_STR))
    {
        return OPCODE_COMPOSE;
    }
    else if (!strcmp(token, QUIT_STR))
    {
        return OPCODE_QUIT;
    }
    // bad operation - return error
    return OPCODE_ERROR;
}


int sendall(int sock, void *buf, int *len)
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
    return n == -1 ? -1 : 0; /*-1 on failure, 0 on success */
}

int send_imm1(int sock, char b) {
    int temp = 1;
    sendall(sock, &b, &temp);
}

bool connectToServer(int sockfd, char *hostname, char *port)
{
    char buf[BUF_SIZE];

    //establish the initial connection with the server
    establishInitialConnection(sockfd, hostname, port);

    char byteFromServer;
    while (true) {
        recv_imm1(sockfd, &byteFromServer);
        if (byteFromServer == ASK_USER) {
            printf("Enter username\n");
            fgets(buf, BUF_SIZE, stdin);
            send_imm1(sockfd, strlen(buf));
            sendall(sockfd, buf, strlen(byteFromServer));
            continue;
        }
        else if (byteFromServer == ASK_PASSWORD) {
            printf("Enter password\n");
            fgets(buf, BUF_SIZE, stdin);
            send_imm1(sockfd, strlen(buf));
            sendall(sockfd, buf, strlen(byteFromServer));
            continue;
        }
        else if (byteFromServer == LOGIN_FAILURE) {
            printf("Login failed\n");
            continue;
        }
        else if (byteFromServer == LOGIN_SUCCESS) {
            printf("Login successful\n");
            return true;
        }
        else if (byteFromServer == LOGIN_KILL) {
            printf("Login attempt killed\n");
            return false;
        }
    }
    /*
   //The client needs to recieve the server's welcome message
     recvData(sockfd, buf);
    //print welcome message
    printf("%s", buf);
    //now the client takes username and password from the user and validates then with the server:
    validateUser(sockfd);
    // if we got here - connection succeeded
    printf("Connected to server\n");
    */
}



void establishInitialConnection(int sockfd, char *hostname, char *port)
{
    int rv;
    struct addrinfo hints, *servinfo, *p;

    /* zero hints struct*/
    memset(&hints, 0, sizeof(hints));
    /* we want IPv4 address and TCP*/
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    /* try get address info*/
    if ((rv = getaddrinfo(hostname, port, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "Error getting address info: %s\n", gai_strerror(rv));
        tryClose(sockfd);
        exit(EXIT_FAILURE);
    }
    p = servinfo;
    while (p != NULL)
    {
        /* put last address into sent pointer*/
        //h = (struct sockaddr_in *) p->ai_addr;
        //dest_addr.sin_addr = h->sin_addr;


        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == 0)
        {
            break; //connection established
        }

        p = p->ai_next;
    }

    if (p == NULL) //We didn't find a host
    {
        tryClose(sockfd);
        fprintf(stderr, "Could not connect to server");
        exit(EXIT_FAILURE);
    }
    /*free the servinfo struct - we're done with it"*/
    freeaddrinfo(servinfo);
}



void tryClose(int sockfd)
{
    if (sockfd == -1) // invalid sockfd - occurs only if "socket" fails, and it is in prpose
    {
        return;
    }
    if (close(sockfd) < 0)
    {
        perror("Could not close socket");
        exit(EXIT_FAILURE);
    }
}

int recvall(int sock, void *buf, int *len)
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
    return n == -1 ? -1 : 0; /*-1 on failure, 0 on success */
}

int recv_imm1(int sock, char target)
{
    int temp = 1;
    return recvall(sock, &target, &temp);
}

void shutdownSocket(int sock)
{
    int res = 0;
    char buff[10];
    /* send shtdwn message, and stop writing*/
    shutdown(sock, SHUT_WR);
    while (1)
    {
        /* read until the end of the stream*/
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
    exit(0);
}

void analyzeProgramArguments(int argc, char **argv, char **hostname, char **port)
{
    /*exit in case of incorrect argument count*/
    if (argc > 3)
    {
        printf("Incorrect Argument Count");
        exit(EXIT_FAILURE);
    }
    /* place default hostname if required*/
    if (argc == 1)
    {
        *hostname = "localhost";
    }
    else
    {
        *hostname = argv[1];
    }

    /* place default port if required*/
    if (argc < 3)
    {
        *port = "6423";
    }
    else
    {
        *port = argv[2];
    }
}

void trySysCall(int syscallResult, const char *msg, int sockfd)
{
    if (syscallResult < 0)
    {
        perror(msg);
        tryClose(sockfd);
        exit(EXIT_FAILURE);
    }
}



void validateUser(int sockfd)
{
    int attemptsLeft = MAX_INPUT_ATTEMPTS;
    char buf[BUF_SIZE], username[MAX_USERNAME_LENGTH], password[MAX_PASSWORD_LENGTH];
    char serverAnswer;
    do
    {
        //zero all buffers
        memset(buf, 0, BUF_SIZE);
        memset(username, 0, MAX_USERNAME_LENGTH);
        memset(password, 0, MAX_PASSWORD_LENGTH);
        serverAnswer = 0;

        //get username and password from user
        getInputFromUser(username, "User:", "username", MAX_USERNAME_LENGTH, sockfd);
        getInputFromUser(password, "Password:", "password", MAX_PASSWORD_LENGTH, sockfd);

        //copy the data to buf in the form of "<username>\t<password>" and send to the server for validation
        strcpy(buf, username);
        strcat(buf, "\t");
        strcat(buf, password);
        sendData(sockfd, buf);

        // receive 1 byte answer from server
        trySysCall(recv(sockfd, &serverAnswer, 1, 0), "Receiving connection answer from the server failed", sockfd);

        if (serverAnswer) // connection succeeded
        {
            return;
        }
        attemptsLeft--;
        printf("Connection to server failed. Attempts Left: %d\n", attemptsLeft);
    } while (attemptsLeft > 0);
    printf("No more attempts left. Exiting...");
    tryClose(sockfd);
    exit(EXIT_FAILURE);

}

void getInputFromUser(char *fieldBuf, const char *fieldPrefix, const char *fieldName, int maxFieldLength, int sockfd)
{
    char *token, buf[BUF_SIZE] = {0};

    //try to read input from user to buf
    if (!fgets(buf, BUF_SIZE, stdin))
    {
        printf("Error - reading %s failed\n", fieldName);
        tryClose(sockfd);
        exit(EXIT_FAILURE);
    }

    // parse input until first space or tab
    token = strtok(buf, " \t\r");

    if (strcmp(token, fieldPrefix))
    {
        printf("Invalid input format - The format is: \'%s <%s>\'. Exiting...\n", fieldPrefix, fieldName);
        tryClose(sockfd);
        exit(EXIT_FAILURE);
    }
    // parse input again to get field name
    token = strtok(NULL, " \t\r\n");
    if (strlen(token) > maxFieldLength)
    {
        printf("%s is too long (maximal length is %d). Exiting...\n", fieldName, maxFieldLength);
        tryClose(sockfd);
        exit(EXIT_FAILURE);
    }
    strcpy(fieldBuf, token);
}

void sendData(int sockfd, char *buf)
{
    int dataLength = htons((uint16_t) strlen(buf));
    int dataLengthBytes = sizeof(dataLength);

    //send data length
    trySysCall(sendall(sockfd, &dataLength, &dataLengthBytes), "Failed to send data length", sockfd);

    //send the real data
    trySysCall(sendall(sockfd, buf, &dataLength), "Failed to send data", sockfd);
}


void recvData(int sockfd, char *buf)
{
    int dataLength = 0;
    int dataLengthBytes = sizeof(dataLength);

    //receive data length
    trySysCall(recvall(sockfd, &dataLength, &dataLengthBytes), "Failed to receive data length", sockfd);

    //translate length to host architecture
    dataLength = ntohs((uint16_t) dataLength);

    //zero buf
    memset(buf, 0, BUF_SIZE);

    //receive the real data
    trySysCall(recvall(sockfd, buf, &dataLength), "Failed to receive data", sockfd);
}