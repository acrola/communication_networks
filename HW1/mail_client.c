#include "mail_common.h"

#define SHOW_INBOX_STR "SHOW_INBOX"
#define GET_MAIL_STR "GET_MAIL"
#define DELETE_MAIL_STR "DELETE_MAIL"
#define COMPOSE_STR "COMPOSE"
#define QUIT_STR "QUIT"

#define USERNAME_FIELD_PREFIX "User:"
#define USERNAME_FIELD_NAME "username"
#define PASSWORD_FIELD_PREFIX "Password:"
#define PASSWORD_FIELD_NAME "password"





char getOpCode(char *token);

void analyzeProgramArguments(int argc, char *argv[], char **hostname, char **port);

void establishInitialConnection(int sockfd, char *hostname, char *port);

void sendData(int sockfd, char *buf);

void getLoginInputFromUser(char *fieldBuf, const char *fieldPrefix, const char *fieldName, int maxFieldLength,
                           int sockfd);

void start_login_request(int sockfd);

void printDataFromServer(int sockfd);

void getInputFromUser(char *buf, size_t bufSize, const char *errorMsg, int sockfd);

void validateComposeField(int iterationNumber, char *token, int sockfd);

void log_kill_handler();

void getOperationFromUser(int sockfd, bool *clientIsActive);


int main(int argc, char *argv[])
{
    int sockfd;
    char *hostname, *port;
    bool clientIsActive;

    // analyze hostname and port no.
    analyzeProgramArguments(argc, argv, &hostname, &port);

    // open TCP socket with IPv4
    // (no opened socket yet - hence we pass a -1 as a sockfd in case of a syscall failure)
    trySysCall((sockfd = socket(PF_INET, SOCK_STREAM, 0)), "Error in opening client's socket", -1);

    // connect to server socket
    establishInitialConnection(sockfd, hostname, port);

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
            case LOG_KILL:
                log_kill_handler(); // todo - what is the difference between this and OP_QUIT...? server disconnected us anyway. consider union
                clientIsActive = false;
                break;
            case OP_PRINT:
                printDataFromServer(sockfd);
                break;
            case OP_HALT:
                getOperationFromUser(sockfd, &clientIsActive); // maybe set clientIsActive to zero
                break;
            case OP_QUIT:
                quit_operation();
                clientIsActive = false;
                break;
            default:;

        }
    }
    tryClose(sockfd);
    return EXIT_SUCCESS;
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



/*void validateUser(int sockfd)
{
    int attemptsLeft = MAX_INPUT_ATTEMPTS;
    char buf[BUF_SIZE], username[MAX_USERNAME], password[MAX_PASSWORD];
    char serverAnswer;
    do
    {
        //zero all buffers
        memset(buf, 0, BUF_SIZE);
        memset(username, 0, MAX_USERNAME);
        memset(password, 0, MAX_PASSWORD);
        serverAnswer = 0;

        //get username and password from user
        getLoginInputFromUser(username, "User:", "username", MAX_USERNAME, sockfd);
        getLoginInputFromUser(password, "Password:", "password", MAX_PASSWORD, sockfd);

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

}*/

void getLoginInputFromUser(char *fieldBuf, const char *fieldPrefix, const char *fieldName, int maxFieldLength,
                           int sockfd)
{
    char *token, buf[BUF_SIZE] = {0}, errBuf[ERROR_BUF_SIZE] = {0};

    sprintf(errBuf, "Error - reading %s failed\n", fieldName);
    getInputFromUser(buf, BUF_SIZE, errBuf, sockfd);

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



void quit_operation()
{

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

void getInputFromUser(char *buf, size_t bufSize, const char *errorMsg, int sockfd)
{

    //zero buffer
    memset(buf, 0, bufSize);
    if (!fgets(buf, bufSize, stdin))
    {
        printf("%s. Exiting...\n",errorMsg);
        tryClose(sockfd);
        exit(EXIT_FAILURE);

    }

}

void validateComposeField(int iterationNumber, char *token, int sockfd)
{
    bool gotError = false;
    char *field = "", errMsg[50] = {0};
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

void log_kill_handler()
{
    printf("Failed logging to server - failed %d times.\nexiting...\n", MAX_LOGIN_ATTEMPTS);
}

void getOperationFromUser(int sockfd, bool *clientIsActive)
{
    char buf[BUF_SIZE];
    char *token, opCode;
    short mailId;
    while(true) // iterate until a valid opcode is given;
    {
        //zero buffer
        memset(buf, 0, BUF_SIZE);
        //get input from user
        if (!fgets(buf, BUF_SIZE, stdin))
        {
            printf("Error getting operation from user\n");
            tryClose(sockfd);
            exit(EXIT_FAILURE);
        }
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
    trySysCall(send(sockfd, &opCode, sizeof(char), 0), "Sending opcode to server failed", 0);


    switch (opCode)
    {
        case OP_SHOWINBOX:
            break; // client returns to listen to the server

        case OP_DELETEMAIL: // operations involving mail id
        case OP_GETMAIL:
            token = strtok(NULL, " \t\r\n");
            //cast mail id to short in network order
            mailId = htons((uint16_t) strtol(token, NULL, 10));
            // send mail id to server
            sendall_imm(sockfd, &mailId, sizeof(mailId));
            break;
        case OP_COMPOSE:
            //3 loops - To, Subject, Text
            for (int i = 0; i < 3; ++i)
            {
                // get input (To / Subject / Text)
                getInputFromUser(buf, BUF_SIZE, "Error getting input from user (compose operation)", sockfd);
                token = strtok(buf, " \t\r");
                validateComposeField(i, token,sockfd);
                // the field is valid, hence we continue parsing until a new line is encountered
                token = strtok(NULL, "\n");
                //copy the data , null terminate it and send it to the server
                strcpy(buf, token);
                buf[strlen(token)] = 0;
                sendData(sockfd, buf);
            }

            sendData(sockfd, buf);
            printf("%s", buf);
            break;
        case OP_QUIT:
            //todo - do we really need to let the server know we're quitting? if not - change above..
            *clientIsActive = false;
            send_char(sockfd, OP_QUIT);
            break;
        default:
            handleUnexpectedError("Invalid opcode", sockfd);
    }

}


void printErrorMsgAndExit(const char *errMsg, int sockfd)
{
    printf("%s.\nExiting...\n",errMsg);
    tryClose(sockfd);
    exit(EXIT_FAILURE);

}




