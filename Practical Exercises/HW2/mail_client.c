#include "mail_common.h"

/*
 * LOCAL DEFINES DECLERATIONS
 */
/* operations strings */
#define SHOW_INBOX_STR "SHOW_INBOX"
#define GET_MAIL_STR "GET_MAIL"
#define DELETE_MAIL_STR "DELETE_MAIL"
#define COMPOSE_STR "COMPOSE"
#define QUIT_STR "QUIT"
#define SHOW_ONLINE_USERS_STR "SHOW_ONLINE_USERS"
#define CHAT_MSG_STR "MSG"

/* username and password and chat fields */
#define USERNAME_FIELD_PREFIX "User:"
#define USERNAME_FIELD_NAME "username"
#define PASSWORD_FIELD_PREFIX "Password:"
#define PASSWORD_FIELD_NAME "password"


/*
 * LOCAL FUNCTIONS DECLERATIONS
 */
char getOpCode(char *token);

void analyzeProgramArguments(int argc, char *argv[], char **hostname, char **port);

int establishInitialConnection(char *hostname, char *port);

void sendData(int sockfd, char *buf);

bool getLoginInputFromUser(char *fieldBuf, const char *fieldPrefix, const char *fieldName, int maxFieldLength,
                           int sockfd);

void startLoginRequest(int sockfd);

void printDataFromServer(int sockfd);

void getInputFromUser(char *buf, size_t bufSize, int sockfd);

int validateComposeField(int iterationNumber, char *token, int sockfd);

void getOperationFromUser(int sockfd, bool *clientIsActive);

void composeNewMail(int sockfd);

void validateMailId(long parsedMailId, int sockfd);

void sendChatMessage(int sockfd, char *buf);

int main(int argc, char *argv[])
{
    int sockfd, exitCode = EXIT_SUCCESS;
    char *hostname, *port;
    bool clientIsActive;

    /* analyze hostname and port no. */
    analyzeProgramArguments(argc, argv, &hostname, &port);

    /* setup a connection and connect to server socket */
    sockfd = establishInitialConnection(hostname, port);

    clientIsActive = true;
    /* loop runs as long as the client wants to get data and no errors occur. */
    /* if the client quits or an error occurs - the clientIsActive is set to false, */
    /* we get out of the loop and shutdown the client program gracefully */
    while (clientIsActive)
    {
        switch (recv_char(sockfd))
        {
            case LOG_REQUEST:
                startLoginRequest(sockfd);
                break;
            case OP_HALT:
                getOperationFromUser(sockfd, &clientIsActive); /* maybe set clientIsActive to zero (QUIT operation) */
                break;
            case OP_PRINT:
                printDataFromServer(sockfd);
                break;
            case LOG_KILL: /* happens when user failed to login after MAX_LOGIN_ATTEMPTS */
                printf("Login failed - got KILL from the server.\nExiting...\n");
                clientIsActive = false;
                exitCode = EXIT_FAILURE;
                break;
            default:;
        }
    }
    tryClose(sockfd);
    return exitCode;
}

int establishInitialConnection(char *hostname, char *port)
{
    /* CREDIT: most of this code was taken (with some changes) from beej's communication network guide: */
    /* http://beej.us/guide/bgnet/examples/client.c */
    int rv, sockfd;
    struct addrinfo hints, *servinfo, *p;

    /* zero hints struct*/
    memset(&hints, 0, sizeof(hints));
    /* we want IPv4 address and TCP */
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    /* open TCP socket with IPv4 */
    /* (no opened socket yet - hence we pass a -1 as a sockfd in case of a syscall failure) */
    trySysCall((sockfd = socket(PF_INET, SOCK_STREAM, 0)), "Error in opening client's socket", -1);

    /* try get address info */
    if ((rv = getaddrinfo(hostname, port, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "Error getting address info: %s\n", gai_strerror(rv));
        tryClose(sockfd);
        exit(EXIT_FAILURE);
    }
    p = servinfo;
    while (p != NULL)
    {
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == 0)
        {
            break; /* connection established */
        }

        p = p->ai_next;
    }

    if (p == NULL) /* We didn't find a host */
    {
        tryClose(sockfd);
        printf("Could not connect to server\n");
        exit(EXIT_FAILURE);
    }
    /*free the servinfo struct - we're done with it" */
    freeaddrinfo(servinfo);
    /* return the socket fd so it can be used outside this function */
    return sockfd;
}


void analyzeProgramArguments(int argc, char **argv, char **hostname, char **port)
{
    /* exit in case of incorrect argument count */
    if (argc > 3)
    {
        printf("Invalid number of arguments\n");
        exit(EXIT_FAILURE);
    }
    /* place default hostname if required */
    if (argc == 1)
    {
        *hostname = DEFAULT_HOSTNAME;
    }
    else
    {
        *hostname = argv[1];
    }

    /* place default port if required */
    if (argc < 3)
    {
        *port = DEFAULT_PORT;
    }
    else
    {
        *port = argv[2];
    }
}

bool getLoginInputFromUser(char *fieldBuf, const char *fieldPrefix, const char *fieldName, int maxFieldLength,
                           int sockfd)
{
    char *token, buf[BUF_SIZE], errorMsg[ERROR_MSG_SIZE] = {0};

    sprintf(errorMsg, "Error - reading %s failed", fieldName);
    /* try to read input from user to buf */
    getInputFromUser(buf, BUF_SIZE, sockfd);

    /* parse input until first space */
    token = strtok(buf, " ");

    if (strcmp(token, fieldPrefix))
    {
        printf("Invalid input format - The format is: \'%s <%s>\'. Please try again\n", fieldPrefix, fieldName);
        return false;
    }
    /* parse input again to get field name */
    token = strtok(NULL, " \t\r\n");
    if (strlen(token) > maxFieldLength)
    {
        printf("%s is too long (maximal length is %d). Please try again\n", fieldName, maxFieldLength);
        return false;
    }
    strcpy(fieldBuf, token);
    return true;
}


char getOpCode(char *token)
{
    if (!strcmp(token, SHOW_INBOX_STR))
    {
        return OP_SHOWINBOX;
    }
    else if (!strcmp(token, SHOW_ONLINE_USERS_STR))
    {
        return OP_SHOW_ONLINE_USERS;
    }
    else if (!strcmp(token, CHAT_MSG_STR))
    {
        return OP_CHAT_MSG;
    }
    else if (!strcmp(token, GET_MAIL_STR))
    {
        return OP_GETMAIL;
    }
    else if (!strcmp(token, DELETE_MAIL_STR))
    {
        return OP_DELETEMAIL;
    }
    else if (!strcmp(token, COMPOSE_STR))
    {
        return OP_COMPOSE;
    }
    else if (!strcmp(token, QUIT_STR))
    {
        return OP_QUIT;
    }
    /* bad operation - return error */
    return OP_ERROR;
}

void startLoginRequest(int sockfd)
{
    char username[MAX_USERNAME + 1] = {0}; /* extra char for the null terminator */
    char password[MAX_PASSWORD + 1] = {0}; /* same here :) */
    while (!getLoginInputFromUser(username, USERNAME_FIELD_PREFIX, USERNAME_FIELD_NAME, MAX_USERNAME, sockfd));
    while (!getLoginInputFromUser(password, PASSWORD_FIELD_PREFIX, PASSWORD_FIELD_NAME, MAX_PASSWORD, sockfd));
    /* let the server know we're sending login credentials */
    send_char(sockfd, LOG_REQUEST);
    sendData(sockfd, username);
    sendData(sockfd, password);
}

void printDataFromServer(int sockfd)
{
    char buf[BUF_SIZE] = {0};
    if (recvData(sockfd, buf) == 0)
    {
        handleUnexpectedError("server disconnected before client", sockfd);
    }
    printf("%s", buf);
}

void getInputFromUser(char *buf, size_t bufSize, int sockfd)
{

    /* zero buffer */
    memset(buf, 0, bufSize);
    if (!fgets(buf, bufSize, stdin))
    {
        printf("fgets error - failed to get input from user. Exiting...\n");
        tryClose(sockfd);
        exit(EXIT_FAILURE);

    }
    while (buf[strlen(buf) - 1] == '\r' || buf[strlen(buf) - 1] == '\n')
    {
        buf[strlen(buf) - 1] = 0; /* kill break lines */
    }

}

int validateComposeField(int iterationNumber, char *token, int sockfd)
{
    bool gotError = false;
    char *field = "", errMsg[ERROR_MSG_SIZE] = {0};
    switch (iterationNumber)
    {
        case 0: /* To */
            if (strcmp(token, "To:"))
            {
                gotError = true;
                field = "To:";
            }
            break;

        case 1: /* Subject */
            if (strcmp(token, "Subject:"))
            {
                gotError = true;
                field = "Subject:";
            }
            break;
        case 2: /* Text */
            if (strcmp(token, "Text:"))
            {
                gotError = true;
                field = "Text:";
            }
            break;
        default:
            sprintf(errMsg, "got invalid iteration number (%d)", iterationNumber);
            handleUnexpectedError(errMsg, sockfd);
    }
    if (gotError)
    {
        printf("Invalid field - suppose to start with \'%s \'\n", field);
        return -1;
    }
    return 0;

}

void getOperationFromUser(int sockfd, bool *clientIsActive)
{
    char buf[BUF_SIZE];
    char *token, opCode;
    short mailId;
    long parsedMailId;
    while (true) /* iterate until a valid opcode is given */
    {
        getInputFromUser(buf, BUF_SIZE, sockfd);

        token = strtok(buf, " \t\r\n");
        opCode = getOpCode(token);

        /* if opcode is valid - break out of the loop */
        if (opCode != OP_ERROR)
        {
            break;
        }
        printf("You entered an invalid operation. Please try again.\n");
    }

    switch (opCode)
    {
        case OP_SHOWINBOX:
        case OP_SHOW_ONLINE_USERS:
            /* send the valid opcode to the server */
            trySysCall(send(sockfd, &opCode, sizeof(char), 0), "Sending opcode to server failed", sockfd);
            break; /* client returns listening to the server */
        case OP_CHAT_MSG:
            sendChatMessage(sockfd, buf + strlen(CHAT_MSG_STR) + 1);
            break;
        case OP_DELETEMAIL: /* operations involving mail id */
        case OP_GETMAIL:
            token = strtok(NULL, " \t\r\n");
            parsedMailId = strtol(token, NULL, 10);
            validateMailId(parsedMailId, sockfd);
            /* cast mail id to short in network order */
            mailId = htons((uint16_t)parsedMailId);
            /* send the valid opcode to the server */
            trySysCall(send(sockfd, &opCode, sizeof(char), 0), "Sending opcode to server failed", sockfd);
            /* send mail id to server */
            sendall_imm(sockfd, &mailId, sizeof(mailId));
            break;
        case OP_COMPOSE:
            composeNewMail(sockfd);
            break;
        case OP_QUIT:
            /* let the server know we're quitting */
            send_char(sockfd, OP_QUIT);
            /* change clientIsActive to false so we will not try to receive anything from the server but exit directrly */
            *clientIsActive = false;
            break;
        default:
            handleUnexpectedError("Invalid opcode", sockfd);
    }

}

void sendChatMessage(int sockfd, char *buf)
{
    char target[(MAX_USERNAME + 1)] = {0};
    char msg[BUF_SIZE] = {0};
    char *token;
    char opCode = OP_CHAT_MSG;
    /*tokenize the target and the message and send to the server*/
    token = strtok(buf, ":");
    strcpy(target, token);
    token = strtok(NULL, "\n") + 1; /*+1 for to remove the space from the beginning*/
    strcpy(msg, token);
    /* send the valid opcode to the server */
    trySysCall(send(sockfd, &opCode, sizeof(char), 0), "Sending opcode to server failed", sockfd);
    sendData(sockfd, target);
    sendData(sockfd, msg);
}

char *incStartIdx(char *buf, int i)
{
    switch (i)
    {
        case 0: /* To: */
            return buf + 4;
        case 1: /* Subject: */
            return buf + 9;
        case 2: /* Text: */
            return buf + 6;
        default:
            return NULL;
    }
}

void composeNewMail(int sockfd)
{
    char buf[BUF_SIZE];
    char *token;
    /* 3 loops - To, Subject, Text */
    int i;
    for (i = 0; i < 3; ++i)
    {
        /* get input (To / Subject / Text) */
        getInputFromUser(buf, BUF_SIZE, sockfd);

        token = strtok(buf, " \t\r");

        if (validateComposeField(i, token, sockfd) < 0)
        {
            printf("Please try again.\n");
            i--; /* so the current iteration will start over */
            continue;
        }
        sendData(sockfd, incStartIdx(buf, i));
    }
}

void validateMailId(long parsedMailId, int sockfd)
{
    if (parsedMailId < 1)
    {
        printf("Error - Invalid mail ID.\nExiting...");
        tryClose(sockfd);
        exit(EXIT_FAILURE);
    }
}