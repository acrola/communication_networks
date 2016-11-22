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

#define BUF_SIZE 4096
#define MAX_USERNAME_LENGTH 32
#define MAX_PASSWORD_LENGTH 32
#define MAX_INPUT_ATTEMPTS 3

typedef enum _eOpCode
{
    OPCODE_SHOW_INBOX = 1,
    OPCODE_GET_MAIL = 2,
    OPCODE_DELETE_MAIL  = 3,
    OPCODE_COMPOSE = 4,
    OPCODE_QUIT  = 5
} eOpCode;


void shutdownSocket(int sock);

int sendall(int sock, void *buf, int *len);

int sendMessage(int sock, unsigned int operation, unsigned short mail_id);

void connectToServer(int sockfd, char *hostname, char *port);

void tryClose(int sock);

int recvall(int sock, void *buf, int *len);

void analyzeProgramArguments(int argc, char *argv[], char **hostname, char **port);

void validateUser(int sockfd);

void establishInitialConnection(int sockfd, char *hostname, char *port);

void recvData(int sockfd, char *buf);

void trySysCall(int syscallResult, const char *msg);

void validateUser(int sockfd);

void sendData(int sockfd, char *buf);

void getInputFromUser(char *fieldBuf, const char *fieldPrefix, const char *fieldName, int maxFieldLength);

int main(int argc, char *argv[])
{
    int sockfd;
    char *hostname, *port;

    analyzeProgramArguments(argc, argv, &hostname, &port);

    /* open TCP socket with IPv4*/
    if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Error in opening client's socket");
        exit(EXIT_FAILURE);
    }

    /* connect to server*/
    connectToServer(sockfd, hostname, port);



    /* get correct input from user*/
    int inputDone = 0;
    char input;
    while (!inputDone)
    {
        /* TODO - get from user, username and password correctly, the best way i thought of it is to read char by char*/
        scanf("%c", &input);
        //if (...)
        //inputDone = 1;
        /* TODO - if authentication was uncorrect/the input wan't valid- disconnect from the server*/

        //TODO - delete this
        inputDone = 1;
    }



    /* TODO - after connection was established succesfully, get input operations from user, parse correctly and use "sendMessage" function to send to server*/
    /* TODO - complete reading something from server*/
    while (((recvall(sockfd, (char *) &buff, &len1)) == 0) &&
           ((recvall(sockfd, (char *) &nums, &len2)) == 0))
    {
        int num;
        int inputDone = 0;
        char c;
        unsigned short n;
        if (len1 != 1 || len2 != 4)
        {
            perror("Error receiving data from server");
            tryClose(sockfd);
            exit(errno);
        }


        /* if number is not in range, send 1023 to make move invalid*/
        n = (num > 0 && num <= 1000) ? (unsigned short) num : 0x3FF;
        /* try to send the message*/
        if (sendMessage(sockfd, c, n) < 0)
        {
            perror("Error sending message");
            break;
        }
        len1 = 1;
        len2 = 4;
    }
    /* if we had a read error, display it*/
    if (len1 != 0 && len2 != 0)
    {
        if (len1 != 1 || len2 != 4)
        {
            perror("Error receiving message");
        }
    }
    tryClose(sockfd);
    return 0;
}

int sendMessage(int sock, unsigned int operation, unsigned short mail_id)
{
    /* TODO - implement with protocol I did in the server*/
    return 0;
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

void connectToServer(int sockfd, char *hostname, char *port)
{
    //struct sockaddr_in *h;
    //struct sockaddr_in dest_addr;

//    /* server addr is IPv4*/
//    dest_addr.sin_family = AF_INET;
//    /* write server port*/
//    dest_addr.sin_port = htons((short) strtol(port, NULL, 10));

    char buf[BUF_SIZE];

    //establish the initial connection with the server
    establishInitialConnection(sockfd, hostname, port);
    //The client needs to recieve the server's welcome message
    recvData(sockfd, buf);
    //print welcome message
    printf("%s", buf);
    /*now the client takes username and password from the user and validates then with the server:*/
    validateUser(sockfd);
    // if we got here - connection succeeded
    printf("Connected to server\n");
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



void tryClose(int sock)
{
    if (close(sock) < 0)
    {
        perror("Could not close socket");
        exit(errno);
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

void trySysCall(int syscallResult, const char *msg)
{
    if(syscallResult < 0)
    {
        perror(msg);
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
        getInputFromUser(username, "User:", "username", MAX_USERNAME_LENGTH);
        getInputFromUser(password, "Password:", "password", MAX_PASSWORD_LENGTH);

        //copy the data to buf in the form of "<username>\t<password>" and send to the server for validation
        strcpy(buf, username);
        strcat(buf, "\t");
        strcat(buf, password);
        sendData(sockfd, buf);

        // receive 1 byte answer from server
        if (recv(sockfd, &serverAnswer, 1, 0) < 0)
        {
            perror("Receiving connection answer from the server failed");
            exit(EXIT_FAILURE);
        }
        if(serverAnswer) // connection succeeded
        {
            return;
        }
        attemptsLeft--;
        printf("Connection to server failed. Attempts Left: %d (out of %d)\n", attemptsLeft, MAX_INPUT_ATTEMPTS);
    }
    while(attemptsLeft > 0);
    printf("No more attempts left. Exiting...");
    exit(EXIT_FAILURE);

}

void getInputFromUser(char *fieldBuf, const char *fieldPrefix, const char *fieldName, int maxFieldLength)
{
    char *token, buf[BUF_SIZE] = {0};
    int attemptsLeft = MAX_INPUT_ATTEMPTS;

    //try to read input from user to buf
    if (!fgets(buf, BUF_SIZE, stdin))
    {
        printf("Error - reading %s failed\n", fieldName);
        exit(EXIT_FAILURE);
    }

    // parse input until first space or tab
    token = strtok(buf, " \t\r");

    if(strcmp(token, fieldPrefix))
    {
        printf("Invalid input format - The format is: \'%s <%s>\'. Exiting...\n", fieldPrefix, fieldName);
        exit(EXIT_FAILURE);
    }
    // parse input again to get field name
    token = strtok(NULL, " \t\r\n");
    if (strlen(token) > maxFieldLength)
    {
        printf("%s is too long (maximal length is %d). Exiting...\n", fieldName, maxFieldLength);
        exit(EXIT_FAILURE);
    }
    strcpy(fieldBuf, token);

}

void sendData(int sockfd, char *buf)
{
    int dataLength = htons((uint16_t)strlen(buf));
    int dataLengthBytes = sizeof(dataLength);

    //send data length
    if (sendall(sockfd, &dataLength, &dataLengthBytes) < 0)
    {
        perror("Failed to send data length");
        exit(EXIT_FAILURE);
    }

    //send the real data
    if (sendall(sockfd, buf, &dataLength) < 0)
    {
        perror("Failed to send data");
        exit(EXIT_FAILURE);
    }

}


void recvData(int sockfd, char *buf)
{
    int dataLength = 0;
    int dataLengthBytes = sizeof(dataLength);

    //receive data length
    if (recvall(sockfd, &dataLength, &dataLengthBytes) < 0)
    {
        perror("Failed to receive data length");
        exit(EXIT_FAILURE);
    }
    //translate length to host architecture
    dataLength = ntohs((uint16_t)dataLength);

    //zero buf
    memset(buf, 0, BUF_SIZE);
    //receive the real data
    if (recvall(sockfd, buf, &dataLength) < 0)
    {
        perror("Failed to receive data");
        exit(EXIT_FAILURE);
    }
}