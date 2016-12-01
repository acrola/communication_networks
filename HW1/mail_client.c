#include "mail_common.h"

/*
 * LOCAL DEFINES DECLERATIONS
 */
// operations strings
#define SHOW_INBOX_STR "SHOW_INBOX"
#define GET_MAIL_STR "GET_MAIL"
#define DELETE_MAIL_STR "DELETE_MAIL"
#define COMPOSE_STR "COMPOSE"
#define QUIT_STR "QUIT"
// username and password fields
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

void getLoginInputFromUser(char *fieldBuf, const char *fieldPrefix, const char *fieldName, int maxFieldLength,
                           int sockfd);

void start_login_request(int sockfd);

void printDataFromServer(int sockfd);

void getInputFromUser(char *buf, size_t bufSize, int sockfd);

void validateComposeField(int iterationNumber, char *token, int sockfd);

void getOperationFromUser(int sockfd, bool *clientIsActive);

void composeNewMail(int sockfd);

void validateMailId(long parsedMailId, int sockfd);

int main(int argc, char *argv[])
{
    int sockfd, exitCode = EXIT_SUCCESS;
    char *hostname, *port;
    bool clientIsActive;

    // analyze hostname and port no.
    analyzeProgramArguments(argc, argv, &hostname, &port);

    // setup a connection and connect to server socket
    sockfd = establishInitialConnection(hostname, port);

    clientIsActive = true;
    // loop runs as long as the client wants to get data and no errors occur.
    // if the client quits or an error occurs - the clientIsActive is set to false,
    // we get out of the loop and shutdown the client program gracefully
    while (clientIsActive)
    {
        switch (recv_char(sockfd))
        {
            case LOG_REQUEST:
                start_login_request(sockfd);
                break;
            case OP_HALT:
                getOperationFromUser(sockfd, &clientIsActive); // maybe set clientIsActive to zero (QUIT operation)
                break;
            case OP_PRINT:
                printDataFromServer(sockfd);
                break;
            case LOG_KILL: // happens when user failed to login after MAX_LOGIN_ATTEMPTS
                printf("Failed logging in to the server (after %d attempts).\nExiting...\n", MAX_LOGIN_ATTEMPTS);
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
    // CREDIT: most of this code was taken (with some changes) from beej's communication network guide:
    // http://beej.us/guide/bgnet/examples/client.c
    int rv, sockfd;
    struct addrinfo hints, *servinfo, *p;

    /* zero hints struct*/
    memset(&hints, 0, sizeof(hints));
    /* we want IPv4 address and TCP*/
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    // open TCP socket with IPv4
    // (no opened socket yet - hence we pass a -1 as a sockfd in case of a syscall failure)
    trySysCall((sockfd = socket(PF_INET, SOCK_STREAM, 0)), "Error in opening client's socket", -1);

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
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == 0)
        {
            break; //connection established
        }

        p = p->ai_next;
    }

    if (p == NULL) //We didn't find a host
    {
        tryClose(sockfd);
        printf( "Could not connect to server");
        exit(EXIT_FAILURE);
    }
    /*free the servinfo struct - we're done with it"*/
    freeaddrinfo(servinfo);
    // return the socket fd so it can be used outside this function
    return sockfd;
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
        *hostname = DEFAULT_HOSTNAME;
    }
    else
    {
        *hostname = argv[1];
    }

    /* place default port if required*/
    if (argc < 3)
    {
        *port = DEFAULT_PORT;
    }
    else
    {
        *port = argv[2];
    }
}

void getLoginInputFromUser(char *fieldBuf, const char *fieldPrefix, const char *fieldName, int maxFieldLength,
                           int sockfd)
{
    char *token, buf[BUF_SIZE], errorMsg[ERROR_MSG_SIZE] = {0};

    sprintf(errorMsg, "Error - reading %s failed", fieldName);
    //try to read input from user to buf
    getInputFromUser(buf, BUF_SIZE, sockfd);

    // parse input until first space
    token = strtok(buf, " ");

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


char getOpCode(char *token)
{
    if (!strcmp(token, SHOW_INBOX_STR))
    {
        return OP_SHOWINBOX;
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
    // bad operation - return error
    return OP_ERROR;
}

void start_login_request(int sockfd)
{
    char username[MAX_USERNAME + 1] = {0}; // extra char for the null terminator
    char password[MAX_PASSWORD + 1] = {0}; // same here :)
    getLoginInputFromUser(username, USERNAME_FIELD_PREFIX, USERNAME_FIELD_NAME, MAX_USERNAME, sockfd);
    getLoginInputFromUser(password, PASSWORD_FIELD_PREFIX, PASSWORD_FIELD_NAME, MAX_PASSWORD, sockfd);
    sendData(sockfd, username);
    sendData(sockfd, password);
}

void printDataFromServer(int sockfd)
{
    char buf[BUF_SIZE] = {0};
    recvData(sockfd, buf);
    printf("%s", buf);
}

void getInputFromUser(char *buf, size_t bufSize, int sockfd)
{

    //zero buffer
    memset(buf, 0, bufSize);
    if (!fgets(buf, bufSize, stdin))
    {
        printf("fgets error - failed to get input from user. Exiting...\n");
        tryClose(sockfd);
        exit(EXIT_FAILURE);

    }
    while(buf[strlen(buf) - 1] == '\r' || buf[strlen(buf) - 1] == '\n') {
        buf[strlen(buf) - 1] = 0; // kill break lines
    }

}

void validateComposeField(int iterationNumber, char *token, int sockfd)
{
    bool gotError = false;
    char *field = "", errMsg[ERROR_MSG_SIZE] = {0};
    switch (iterationNumber)
    {
        case 0: //To
            if (strcmp(token, "To:"))
            {
                gotError = true;
                field = "To:";
            }
            break;

        case 1: // Subject
            if (strcmp(token, "Subject:"))
            {
                gotError = true;
                field = "Subject:";
            }
            break;
        case 2: // Text
            if (strcmp(token, "Text:"))
            {
                gotError = true;
                field = "Text:";
            }
            break;
        default:
            sprintf(errMsg,"got invalid iteration number (%d)", iterationNumber);
            handleUnexpectedError(errMsg, sockfd);
    }
    if(gotError)
    {
        printf("Invalid field - suppose to start with \'%s \'", field);
        tryClose(sockfd);
        exit(EXIT_FAILURE);
    }

}

void getOperationFromUser(int sockfd, bool *clientIsActive)
{
    char buf[BUF_SIZE];
    char *token, opCode;
    short mailId;
    long parsedMailId;
    while(true) // iterate until a valid opcode is given;
    {
        getInputFromUser(buf, BUF_SIZE, sockfd);

        token = strtok(buf, " \t\r\n");
        opCode = getOpCode(token);

        // if opcode is valid - break out of the loop
        if (opCode != OP_ERROR)
        {
            break;
        }
        printf("You entered an invalid operation. Please try again.\n");
    }
    // send the valid opcode to the server
    trySysCall(send(sockfd, &opCode, sizeof(char), 0), "Sending opcode to server failed", sockfd);

    switch (opCode)
    {
        case OP_SHOWINBOX:
            break; // client returns listening to the server
        case OP_DELETEMAIL: // operations involving mail id
        case OP_GETMAIL:
            token = strtok(NULL, " \t\r\n");
            parsedMailId = strtol(token, NULL, 10);
            validateMailId(parsedMailId, sockfd);
            //cast mail id to short in network order
            mailId = htons((uint16_t)parsedMailId);
            // send mail id to server
            sendall_imm(sockfd, &mailId, sizeof(mailId));
            break;
        case OP_COMPOSE:
            composeNewMail(sockfd);
            break;
        case OP_QUIT:
            // let the server know we're quitting
            send_char(sockfd, OP_QUIT);
            // change clientIsActive to false so we will not try to receive anything from the server but exit directrly
            *clientIsActive = false;
            break;
        default:
            handleUnexpectedError("Invalid opcode", sockfd);
    }

}

char* incStartIdx(char* buf, int i) {
    switch (i) {
        case 0: // To:
            return buf + 4;
        case 1: // Subject:
            return buf + 9;
        case 2: // Text:
            return buf + 6;
        default:
            return NULL;
}
}

void composeNewMail(int sockfd)
{
    char buf[BUF_SIZE];
    char *token;
    //3 loops - To, Subject, Text
    for (int i = 0; i < 3; ++i)
    {
        // get input (To / Subject / Text)
        getInputFromUser(buf, BUF_SIZE, sockfd);
        printf("temp %s\n", buf);
        token = strtok(NULL, "\n");
        token = strtok(buf, " \t\r");
        //printf("tempz %s\n", token);
        validateComposeField(i, token, sockfd);
        // the field is valid, hence we continue parsing until a new line is encountered
        //token = strtok(NULL, "\n");
        //copy the data , null terminate it and send it to the server
        //strcpy(buf, token);
        //buf[strlen(token)] = 0;
        sendData(sockfd, incStartIdx(buf, i));
        printf("%s\n", buf);
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