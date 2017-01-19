#include "mail_common.h"

#define WELCOME_MSG "Welcome! I am simple-mail-server\n"
#define CONNECTED_MSG "Connected to server\n"


typedef struct Account
{
    char username[MAX_USERNAME + 1];
    char password[MAX_PASSWORD + 1];
    unsigned short *inbox_mail_indices;
    unsigned short inbox_size;
    bool online;
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

unsigned int users_num; /* value will be init'ed in read_file() ... */
unsigned short mails_num = 0;

int isInt(char *str);

bool read_file(char *path);

void serverLoop(int sock, Account *currentAccount);

void show_inbox_operation(int sock, Account *account);

void compose_operation(int sock, Account *account);

void get_mail_operation(int sock, Account *account);

void delete_mail_operation(int sock, Account *account);

void sendToClientPrint(int sock, char *msg);

void sendHalt(int sock);

Account *loginToAccount(int sock);

void closeAllSockets(int fdmax, fd_set *set);

void multipleSockets_trySyscall(int syscallResult, char *msg, fd_set *connectedFds, int fdmax);

int main(int argc, char *argv[])
{
    char *port;
    char *path;
    int listen_sock, new_sock, fdmax;
    socklen_t sin_size;
    struct sockaddr_in myaddr, their_addr;
    Account *currentAccount;
    mails_num = 0;

    /*we initiliaze fd sets*/
    fd_set master;
    fd_set read_fds;
    FD_ZERO(&master);
    FD_ZERO(&read_fds);

    if (argc > 3)
    {
        printf("Incorrect Argument Count");
        exit(EXIT_FAILURE);
    }

    path = argv[1];

    if (argc == 3)
    {
        if (!isInt(argv[2]))
        {
            printf("Please enter a valid number as the port number");
            exit(EXIT_FAILURE);
        }
        port = argv[2];
    }
    else
    {
        port = DEFAULT_PORT;
    }

    read_file(path);

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

    /*start serving...*/
    while (true)
    {
        read_fds = master;
        multipleSockets_trySyscall(select(fdmax + 1, &read_fds, NULL, NULL, NULL), "Select operation failed", &master,
                                   fdmax);

        /*handle read-ready sockets*/
        for (int sockfd = 0; sockfd <= fdmax; ++sockfd)
        {
            if (FD_ISSET(sockfd, &read_fds)) /*we got a read-ready socket!*/
            {
                if (sockfd == listen_sock) /*got a new connection request*/
                {
                    multipleSockets_trySyscall(
                            (new_sock = accept(listen_sock, (struct sockaddr *) &their_addr, &sin_size)),
                            "Could not accept connection", &master, fdmax);

                }
            }

        }
        /* accept a new client (connection with it will be done via new_sock) */
        trySysCall((new_sock = accept(listen_sock, (struct sockaddr *) &their_addr, &sin_size)),
                   "Could not accept connection",
                   listen_sock);
        /*the connection with the user is established - send a welcome message*/
        sendToClientPrint(new_sock, WELCOME_MSG);
        /* reached here, has connection with client - validate username and password */
        if ((currentAccount = loginToAccount(new_sock)) != NULL)
        {
            /* validation is done - start taking orders from client */
            serverLoop(new_sock, currentAccount);
        }
        /* close the accepted socket and wait for a new client */
        tryClose(new_sock);

    }
    /* todo free dynamic accounts and mails */
}

void multipleSockets_trySyscall(int syscallResult, char *msg, fd_set *connectedFds, int fdmax)
{
    if (syscallResult < 1)
    {
        perror(msg);
        closeAllSockets(fdmax, connectedFds);
        exit(EXIT_FAILURE);
    }

}

void closeAllSockets(int fdmax, fd_set *set)
{
    /*iterate thorugh all possible fds, close only those that really exist*/
    for (int sockfd = 0; sockfd <= fdmax; ++sockfd)
    {
        if (FD_ISSET(sockfd, set))
        {
            tryClose(sockfd);
        }
    }

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


void compose_operation(int sock, Account *account)
{
    Account *tempAccount;
    char targets[TOTAL_TO * (MAX_USERNAME + 1)];
    char *recipient;
    bool allTargetsValid = true;
    Mail *currentMail = (Mail *) malloc(sizeof(Mail));
    if (currentMail == NULL)
    {
        printf("Failed allocating memory for new mail struct in compose_operation.\nExiting...\n");
        tryClose(sock);
        exit(EXIT_FAILURE);
    }

    recvData(sock, targets);
    recvData(sock, currentMail->subject);
    recvData(sock, currentMail->content);
    currentMail->sender = account;
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
                tryClose(sock);
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
        sendToClientPrint(sock, "Mail was not sent - unknown recipients.\n");
        /*even though we say the mail was not sent we still save it on our system*/
    }
    else
    {
        if (allTargetsValid)
        {
            sendToClientPrint(sock, "Mail sent\n");
        }
        else
        {
            sendToClientPrint(sock, "Mail sent (some recipients were unidentified)\n");
        }
    }
    sendHalt(sock);
}


Mail *account_mail_access(Account *account, short mail_idx)
{
    return (mail_idx > account->inbox_size || account->inbox_mail_indices[mail_idx - 1] == MAXMAILS) ? NULL
                                                                                                     : mails[account->inbox_mail_indices[
                    mail_idx - 1]];
}


void show_inbox_operation(int sock, Account *account)
{
    short i;
    char mail_msg[BUF_SIZE];
    Mail *currentMail;
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
            sendToClientPrint(sock, mail_msg);
        }
    }
    sendHalt(sock);
}


void get_mail_operation(int sock, Account *account)
{
    short mail_idx = getDataSize(sock);
    int i;
    Mail *currentMail = account_mail_access(account, mail_idx);

    if (currentMail == NULL)
    {
        sendToClientPrint(sock, "Invalid selection\n");
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
        sendToClientPrint(sock, mail_msg);
    }
    sendHalt(sock);
}

void delete_mail_operation(int sock, Account *account)
{
    short mail_idx = getDataSize(sock);
    Mail *currentMail = account_mail_access(account, mail_idx);

    if (currentMail == NULL)
    {
        sendToClientPrint(sock, "Invalid selection\n");
    }
    else
    {
        account->inbox_mail_indices[mail_idx - 1] = MAXMAILS;
    }
    sendHalt(sock);
}

bool read_file(char *path)
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
            printf("Allocation of new account failed in read_file.\nExiting...\n");
            exit(EXIT_FAILURE);
        }
        account_ptr->inbox_mail_indices = NULL;
        account_ptr->online = false;
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

Account *loginToAccount(int sock)
{
    int i, auth_attempts = 0;
    char username[MAX_USERNAME] = {0};
    char password[MAX_PASSWORD] = {0};
    while (true)
    {
        send_char(sock, LOG_REQUEST);
        recvData(sock, username);
        recvData(sock, password);
        for (i = 0; i < users_num; i++)
        {
            if ((strcmp(accounts[i]->username, username) == 0) && (strcmp(accounts[i]->password, password) == 0))
            {
                accounts[i]->online = true;
                sendToClientPrint(sock, CONNECTED_MSG);
                sendHalt(sock); /* login successful, wait for input from user */
                return accounts[i];
            }
        }

        auth_attempts++;
        if (auth_attempts >= MAX_LOGIN_ATTEMPTS)
        {
            sendToClientPrint(sock, "LOGIN FAILED - GOT KILL FROM SERVER\n");
            send_char(sock, LOG_KILL);
            return NULL;
        }
        sendToClientPrint(sock, "LOGIN ATTEMPT FAILED\n");
    }
}

void serverLoop(int sock, Account *currentAccount)
{
    /* when here, after sock establishment and user auth. keep listening for ops */
    while (true)
    {
        switch (recv_char(sock))
        {
            case OP_SHOWINBOX:
                show_inbox_operation(sock, currentAccount);
                break;
            case OP_GETMAIL:
                get_mail_operation(sock, currentAccount);
                break;
            case OP_DELETEMAIL:
                delete_mail_operation(sock, currentAccount);
                break;
            case OP_QUIT: /* client quitted - we update that it's offline and return*/
                currentAccount->online = false;
                return;
            case OP_COMPOSE:
                compose_operation(sock, currentAccount);
                break;
            default:;
        }
    }
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