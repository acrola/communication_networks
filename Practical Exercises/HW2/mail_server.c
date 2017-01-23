#include "mail_common.h"

#define WELCOME_MSG "Welcome! I am simple-mail-server\n"
#define CONNECTED_MSG "Connected to server\n"
#define OFFLINE_MSG_SUBJECT "Message received offline"


typedef struct Account
{
    char username[MAX_USERNAME + 1];
    char password[MAX_PASSWORD + 1];
    unsigned short *inbox_mail_indices;
    unsigned short inbox_size;
    int sockfd;
} Account;

typedef struct Mail
{
    Account *sender;
    Account **recipients;
    unsigned int recipients_num;
    char subject[MAX_SUBJECT + 1];
    char content[MAX_CONTENT + 1];
} Mail;

Account *accounts[NUM_OF_CLIENTS];
Mail *mails[MAXMAILS];
unsigned int users_num; /* value will be init'ed in loadUsersFromFile() ... */
unsigned short mails_num = 0;
fd_set master;
int fdmax;

void analyzeProgramArguments(int argc, char **argv, char **path, char **port);

int isInt(char *str);

bool loadUsersFromFile(char *path);

void show_inbox_operation(int sockfd);

void chat_message_operation(int sockfd);

void compose_operation(int sockfd);

void get_mail_operation(int sockfd);

void delete_mail_operation(int sockfd);

void sendToClientPrint(int sock, char *msg);

void sendHalt(int sock);

void handleLoginRequest(int sockfd);

void closeAllSockets();

void multipleSockets_trySyscall(int syscallResult, char *msg);

Account *getAccountBySockfd(int sockfd);

void show_online_users(int sockfd);

void handleQuitOperation(int sockfd);

int main(int argc, char *argv[])
{
    char *port;
    char *path;
    char op;
    int listen_sock, new_sock, sockfd;
    socklen_t sin_size;
    struct sockaddr_in myaddr, their_addr;

    /*we initiliaze fd sets*/
    fd_set read_fds;
    FD_ZERO(&master);
    FD_ZERO(&read_fds);

    mails_num = 0;

    analyzeProgramArguments(argc, argv, &path, &port);

    loadUsersFromFile(path);

    /* open IPv4 TCP socket*/
    trySysCall((listen_sock = socket(PF_INET, SOCK_STREAM, 0)), "Could not open socket", -1);

    /* IPv4 with given port. We dont care what address*/
    myaddr.sin_family = AF_INET;
    myaddr.sin_port = htons((uint16_t) strtol(port, NULL, 10));
    myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    /* try to bind*/
    trySysCall(bind(listen_sock, (struct sockaddr *) &(myaddr), sizeof(myaddr)), "Could not bind socket", listen_sock);
    /* try to listen to the ether*/
    trySysCall(listen(listen_sock, 1), "Could not listen to socket", listen_sock);

    sin_size = sizeof(struct sockaddr_in);

    /* by this point we have a connection*/

    /*we update our fd sets - for now we only have the listening socket.*/
    FD_SET(listen_sock, &master);
    fdmax = listen_sock;

    /*server loop*/
    while (true)
    {
        read_fds = master;
        multipleSockets_trySyscall(select(fdmax + 1, &read_fds, NULL, NULL, NULL), "Select operation failed");

        /*handle read-ready sockets*/
        for (sockfd = 0; sockfd <= fdmax; ++sockfd)
        {
            if (FD_ISSET(sockfd, &read_fds)) /*we got a read-ready socket!*/
            {
                if (sockfd == listen_sock) /*got a new connection request*/
                {
                    /* accept a new client (connection with it will be done via new_sock) */
                    new_sock = accept(listen_sock, (struct sockaddr *) &their_addr, &sin_size);
                    multipleSockets_trySyscall(new_sock, "Could not accept new connection");
                    /*we add the new socket to the master fd set and update fdmax if necessary*/
                    FD_SET(new_sock, &master);
                    if (new_sock > fdmax)
                    {
                        fdmax = new_sock;
                    }
                    /*the connection with the user is established - send a welcome message*/
                    sendToClientPrint(new_sock, WELCOME_MSG);
                    /*let the client know it should login and continue handling other fds*/
                    send_char(new_sock, LOG_REQUEST);
                }
                else
                {
                    multipleSockets_trySyscall(recv(sockfd, &op, 1, 0), "Failed to receive op from client");
                    switch (op)
                    {
                        case LOG_REQUEST:
                            handleLoginRequest(sockfd);
                            break;
                        case OP_SHOWINBOX:
                            show_inbox_operation(sockfd);
                            break;
                        case OP_SHOW_ONLINE_USERS:
                            show_online_users(sockfd);
                            break;
                        case OP_CHAT_MSG:
                            chat_message_operation(sockfd);
                            break;
                        case OP_GETMAIL:
                            get_mail_operation(sockfd);
                            break;
                        case OP_DELETEMAIL:
                            delete_mail_operation(sockfd);
                            break;
                        case OP_QUIT:
                            handleQuitOperation(sockfd);
                            break;
                        case OP_COMPOSE:
                            compose_operation(sockfd);
                            break;
                        default:
                            printf("Error: opcode is %c\n", op);
                            handleUnexpectedError("Invalid opcode", sockfd);
                    }
                }
            }

        }
    }
    /* todo free dynamic accounts and mails */
}

void handleQuitOperation(int sockfd)
{
    Account *account = getAccountBySockfd(sockfd);
    /*update account's online status. fd of an offline account is being set to -1*/
    account->sockfd = -1;
    /*close the client's socket and update the socket fd set*/
    tryClose(sockfd);
    FD_CLR(sockfd, &master);


}


Account *getAccountByUsername(char *username)
{
    int i;
    for (i = 0; i < users_num; i++)
    {
        if (strcmp(accounts[i]->username, username) == 0)
        {
            return accounts[i];
        }
    }
    return NULL;
}

void chat_message_operation(int sockfd)
{
    Account *tempAccount;
    char recipient[MAX_USERNAME + 1];
    char content[MAX_CONTENT + 1];
    char sendBuffer[MAX_USERNAME + MAX_CONTENT + 20] = {0};
    Mail *mail;

    /* client sends all data at once, so we will not hang here */
    recvData(sockfd, recipient);
    recvData(sockfd, content);

    if ((tempAccount = getAccountByUsername(recipient)) == NULL)
    {
        sendToClientPrint(sockfd, "Chat could not sent - unknown recipient\n");
    }
    else
    {
        if (tempAccount->sockfd > 0)
        { /* recipient is online */
            strcat(sendBuffer, "New message from ");
            strcat(sendBuffer, getAccountBySockfd(sockfd)->username);
            strcat(sendBuffer, ": ");
            strcat(sendBuffer, content);
            strcat(sendBuffer, "\n");

            sendToClientPrint(tempAccount->sockfd, sendBuffer);
            sendHalt(tempAccount->sockfd);
        }
        else
        {
            /* recipient is offline */
            if ((mail = (Mail *) malloc(sizeof(Mail))) == NULL)
            {
                printf("Failed allocating memory for new mail struct in compose_operation.\nExiting...\n");
                tryClose(sockfd);
                exit(EXIT_FAILURE);
            }

            mail->sender = getAccountBySockfd(sockfd);
            strcpy(mail->content, content);
            strcpy(mail->subject, OFFLINE_MSG_SUBJECT);
            mail->recipients = (Account **) malloc(sizeof(Account *));
            mail->recipients_num = 1;
            mail->recipients[0] = tempAccount;

            if (tempAccount->inbox_mail_indices == NULL)
            {
                tempAccount->inbox_mail_indices = (unsigned short *) malloc((size_t) (sizeof(unsigned short)));
            }
            else
            {
                tempAccount->inbox_mail_indices = (unsigned short *) realloc(tempAccount->inbox_mail_indices,
                                                                             (size_t) ((tempAccount->inbox_size + 1) *
                                                                                       sizeof(unsigned short)));
            }
            if (tempAccount->inbox_mail_indices == NULL)
            {
                printf("Failed allocating memory for inbox mail indices in compose_operation.\nExiting...\n");
                tryClose(sockfd);
                exit(EXIT_FAILURE);
            }
            mails[mails_num] = mail;
            tempAccount->inbox_mail_indices[tempAccount->inbox_size] = mails_num; /* add mail idx to indices list */
            tempAccount->inbox_size++;
            mails_num++;
        }
    }
    sendHalt(sockfd);
}


void compose_operation(int sockfd)
{
    Account *tempAccount;
    char targets[TOTAL_TO * (MAX_USERNAME + 1)];
    char *recipient;
    bool allTargetsValid = true;
    Mail *currentMail = (Mail *) malloc(sizeof(Mail));
    if (currentMail == NULL)
    {
        printf("Failed allocating memory for new mail struct in compose_operation.\nExiting...\n");
        tryClose(sockfd);
        exit(EXIT_FAILURE);
    }

    /* client sends all data at once, so we will not hang here */
    recvData(sockfd, targets);
    recvData(sockfd, currentMail->subject);
    recvData(sockfd, currentMail->content);

    currentMail->sender = getAccountBySockfd(sockfd);
    mails[mails_num] = currentMail;
    currentMail->recipients_num = 0;

    recipient = strtok(targets, ",");
    if (recipient != NULL)
    {
        do
        {
            /*kill blank spaces*/
            while (*recipient == ' ')
            {
                recipient++;
            }
            tempAccount = getAccountByUsername(recipient);

            if (tempAccount == NULL)
            {
                allTargetsValid = false;
                continue;
            }

            currentMail->recipients = (Account **) realloc(currentMail->recipients,
                                                           (size_t) ((currentMail->recipients_num + 1) *
                                                                     sizeof(Account *)));
            currentMail->recipients[currentMail->recipients_num] = tempAccount;
            currentMail->recipients_num++;

            if (tempAccount->inbox_mail_indices == NULL)
            {
                tempAccount->inbox_mail_indices = (unsigned short *) malloc((size_t) (sizeof(unsigned short)));
            }
            else
            {
                tempAccount->inbox_mail_indices = (unsigned short *) realloc(tempAccount->inbox_mail_indices,
                                                                             (size_t) ((tempAccount->inbox_size + 1) *
                                                                                       sizeof(unsigned short)));
            }
            if (tempAccount->inbox_mail_indices == NULL)
            {
                printf("Failed allocating memory for inbox mail indices in compose_operation.\nExiting...\n");
                closeAllSockets();
                exit(EXIT_FAILURE);
            }
            tempAccount->inbox_mail_indices[tempAccount->inbox_size] = mails_num; /* add mail idx to indices list */
            tempAccount->inbox_size++;

        }
        while ((recipient = strtok(NULL, ",")));
    }

    mails_num++; /* increase number of total mails in system */
    if (currentMail->recipients_num == 0)
    {
        sendToClientPrint(sockfd, "Mail was not sent - unknown recipients.\n");
        /*even though we say the mail was not sent we still save it on our system*/
    }
    else
    {
        if (allTargetsValid)
        {
            sendToClientPrint(sockfd, "Mail sent\n");
        }
        else
        {
            sendToClientPrint(sockfd, "Mail sent (some recipients were unidentified)\n");
        }
    }
    sendHalt(sockfd);
}


Mail *account_mail_access(Account *account, short mail_idx)
{
    return (mail_idx > account->inbox_size || account->inbox_mail_indices[mail_idx - 1] == MAXMAILS) ? NULL
                                                                                                     : mails[account->inbox_mail_indices[
                    mail_idx - 1]];
}


void show_inbox_operation(int sockfd)
{
    short i;
    char mail_msg[BUF_SIZE];
    Mail *currentMail;
    Account *account = getAccountBySockfd(sockfd);
    for (i = 1; i <= account->inbox_size; i++)
    {
        currentMail = account_mail_access(account, i);
        if (currentMail != NULL)
        {  /* mail idx MAXMAILS is marked as a deleted mail */
            memset(mail_msg, 0, BUF_SIZE);
            sprintf(mail_msg, "%d", i);
            strcat(mail_msg, " ");
            strcat(mail_msg, currentMail->sender->username);
            strcat(mail_msg, " \"");
            strcat(mail_msg, currentMail->subject);
            strcat(mail_msg, "\"\n");
            sendToClientPrint(sockfd, mail_msg);
        }
    }
    sendHalt(sockfd);
}


Account *getAccountBySockfd(int sockfd)
{
    int i;
    for (i = 0; i < users_num; ++i)
    {
        if (accounts[i]->sockfd == sockfd)
        {
            return accounts[i];
        }

    }
    /*this function is being used only after user authentication, hence it should not get here*/
    handleUnexpectedError("No account was found for the given socket fd", sockfd);
    return NULL;
}


void get_mail_operation(int sockfd)
{
    short mail_idx = getDataSize(sockfd);
    int i;
    Account *account = getAccountBySockfd(sockfd);
    Mail *currentMail = account_mail_access(account, mail_idx);

    if (currentMail == NULL)
    {
        sendToClientPrint(sockfd, "Invalid mail ID\n");
    }
    else
    {
        char mail_msg[BUF_SIZE];
        memset(mail_msg, 0, BUF_SIZE);
        strcat(mail_msg, "From: ");
        strcat(mail_msg, currentMail->sender->username);
        strcat(mail_msg, "\nTo: ");
        for (i = 0; i < currentMail->recipients_num; i++)
        {
            if (i > 0)
            {
                strcat(mail_msg, ",");
            }
            strcat(mail_msg, currentMail->recipients[i]->username);
        }
        strcat(mail_msg, "\nSubject: ");
        strcat(mail_msg, currentMail->subject);
        strcat(mail_msg, "\nText: ");
        strcat(mail_msg, currentMail->content);
        strcat(mail_msg, "\n");
        sendToClientPrint(sockfd, mail_msg);
    }
    sendHalt(sockfd);
}

void delete_mail_operation(int sockfd)
{
    short mail_idx = getDataSize(sockfd);
    Account *account = getAccountBySockfd(sockfd);
    Mail *currentMail = account_mail_access(account, mail_idx);

    if (currentMail == NULL)
    {
        sendToClientPrint(sockfd, "Invalid mail ID\n");
    }
    else
    {
        account->inbox_mail_indices[mail_idx - 1] = MAXMAILS;
    }
    sendHalt(sockfd);
}

bool loadUsersFromFile(char *path)
{
    FILE *fp;
    Account *account_ptr;
    users_num = 0;   /* count how many lines are in the file */
    /*first we'll check if the file exists*/
    if (access(path, F_OK) < 0)
    {
        perror("Failed to access the users file");
        exit(EXIT_FAILURE);
    }
    fp = fopen(path, "r");
    /* read each line and put into accounts */
    while (!feof(fp))
    {
        account_ptr = (Account *) malloc(sizeof(Account));
        if (!account_ptr)
        {
            printf("Allocation of new account failed in loadUsersFromFile.\nExiting...\n");
            exit(EXIT_FAILURE);
        }
        account_ptr->inbox_mail_indices = NULL;
        account_ptr->sockfd = -1; /* mark as offline */
        account_ptr->inbox_size = 0;
        if (fscanf(fp, "%s\t%s", account_ptr->username, account_ptr->password) < 0)
        {
            free(account_ptr);
            if (errno != 0)
            {
                perror("Cannot load account");
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            accounts[users_num] = account_ptr;
            users_num++;
        }
    }
    return true;
}

void handleLoginRequest(int sockfd)
{
    int i;
    char username[MAX_USERNAME + 1] = {0};
    char password[MAX_PASSWORD + 1] = {0};

    recvData(sockfd, username);
    recvData(sockfd, password);
    for (i = 0; i < users_num; i++)
    {
        /*if we found a matching user - update the user's socket fd*/
        if ((strcmp(accounts[i]->username, username) == 0) && (strcmp(accounts[i]->password, password) == 0))
        {
            accounts[i]->sockfd = sockfd;
            sendToClientPrint(sockfd, CONNECTED_MSG);
            sendHalt(sockfd); /* login successful, wait for input from user */
            return;

        }
    }
    /*failed to authenticate user - kill client, close socket and delete it from the socket set*/
    send_char(sockfd, LOG_KILL);
    tryClose(sockfd);
    FD_CLR(sockfd, &master);
}


int isInt(char *str)
{
    /* tests if a string is just numbers*/
    if (*str == 0)
    {
        return 0;
    }
    while (*str != 0)
    {
        if (!isdigit(*str))
        {
            return 0;
        }
        str++;
    }
    return 1;
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


void analyzeProgramArguments(int argc, char **argv, char **path, char **port)
{
    if (argc > 3 || argc < 2)
    {
        printf("Invalid number of arguments\n");
        exit(EXIT_FAILURE);
    }

    *path = argv[1];

    if (argc == 3)
    {
        if (!isInt(argv[2]))
        {
            printf("Please enter a valid number as the port number\n");
            exit(EXIT_FAILURE);
        }
        *port = argv[2];
    }
    else
    {
        *port = DEFAULT_PORT;
    }
}

void show_online_users(int sockfd)
{
    short i;
    char buf[BUF_SIZE] = {0};
    strcat(buf, "Online users: ");
    bool flag = false;
    for (i = 0; i < users_num; i++)
    {
        if (accounts[i]->sockfd > 0)
        {
            if (flag)/*comma separation handling - the "if" will be entered only if there's more than one user online*/
            {
                strcat(buf, ", ");

            }
            strcat(buf, accounts[i]->username);
            flag = true;
        }
    }
    strcat(buf, "\n");
    sendToClientPrint(sockfd, buf);
    sendHalt(sockfd);
}


void closeAllSockets()
{
    int sockfd;
    /*iterate thorugh all possible fds, close only those that really exist*/
    for (sockfd = 0; sockfd <= fdmax; ++sockfd)
    {
        if (FD_ISSET(sockfd, &master))
        {
            tryClose(sockfd);
        }
    }

}

void multipleSockets_trySyscall(int syscallResult, char *msg)
{
    if (syscallResult < 0)
    {
        perror(msg);
        closeAllSockets();
        exit(EXIT_FAILURE);
    }

}