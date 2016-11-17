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

#define BUF_SIZE 256

typedef enum _eOpCode
{
    OPCODE_SHOW_INBOX 0x200000,
    OPCODE_GET_MAIL 0x400000,
    OPCODE_DELETE_MAIL 0x600000,
    OPCODE_QUIT 0x800000,
    OPCODE_COMPOSE 0xA00000
} eOpCode;


void shutdownSocket(int sock);

int sendall(int sock, char *buf, int *len);

int sendMessage(int sock, unsigned int operation, unsigned short mail_id);

int connectToServer(int sockfd, char *hostname, char *port);

void tryClose(int sock);

int recvall(int sock, char *buf, int *len);

void analyzeProgramArguments(int argc, char *argv[], char **hostname, char **port);

void getAndSendUserDetails(int sockfd);

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

int sendall(int sock, char *buf, int *len)
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

int connectToServer(int sockfd, char *hostname, char *port)
{



    //struct sockaddr_in *h;
    //struct sockaddr_in dest_addr;

//    /* server addr is IPv4*/
//    dest_addr.sin_family = AF_INET;
//    /* write server port*/
//    dest_addr.sin_port = htons((short) strtol(port, NULL, 10));


    int rv;
    struct addrinfo hints, *servinfo, *p;
    char buf[BUF_SIZE] = {0};


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

    /* The initial connection has been established.
     * The client needs to recieve the server's welcome message:*/
    //todo - needs to be changed. not robust enouge (what if the welcome message is changed?)
    int msg_length = sizeof(char) * strlen("Welcome! I am simple-mail-server.\n");
    recvall(sockfd, buf, &msg_length);
    printf("%s", buf);

    /*now the client takes input from the user and sends it to the server:*/
    getAndSendUserDetails(sockfd);


    printf("Connected to server\n");
    return 0;
}

void getAndSendUserDetails(int sockfd)
{
    const char delimiters[] = " \t\n";
    char *token, input[BUF_SIZE] = {0}, username[BUF_SIZE] = {0}, password[BUF_SIZE] = {0};
    char sendBuf[(BUF_SIZE*2)+1];

    printf("User: ");
    if (!fgets(input, BUF_SIZE, stdin))
    {
        printf("Error - reading username failed\n");
        exit(EXIT_FAILURE);
    }
    token = strtok(input, delimiters);
    strcpy(username, token);

    memset(input, 0, BUF_SIZE);
    printf("Password: ");
    if (!fgets(input, BUF_SIZE, stdin))
    {
        printf("Error - reading password failed\n");
        exit(EXIT_FAILURE);
    }
    token = strtok(input, delimiters);
    strcpy(password, token);
    //copy the data to sendBuf in the form of <username>\t<password>
    strcpy(sendBuf, username);
    strcat(sendBuf, "\t");
    strcat(sendBuf, password);
    int bytesToSend = strlen(sendBuf);
    //send the usernameand password
    sendall(sockfd, sendBuf, &bytesToSend);
}

void tryClose(int sock)
{
    if (close(sock) < 0)
    {
        perror("Could not close socket");
        exit(errno);
    }
}

int recvall(int sock, char *buf, int *len)
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

    /*
    else
    {
        //TODO - needs code review to see if we really catch everything..
        char *temp;
        long tempPort = strtol(argv[2], &temp, 10);
        //check if error occurred during the conversion
        if (temp == argv[2] || *temp != '\0' || ((tempPort == LONG_MIN || tempPort == LONG_MAX)&& errno == ERANGE))
        {
            printf("Invalid port number error");
            exit(EXIT_FAILURE);
        }
        *port = (short)tempPort;
    }*/

}



