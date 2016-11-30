#include "mail_common.h"

#define WELCOME_MSG "Welcome! I am simple-mail-server\n"


typedef struct Account {
    char username[MAX_USERNAME+1];
    char password[MAX_PASSWORD+1];
    unsigned short *inbox_mail_indices;
    unsigned short inbox_size;
} Account;

typedef struct Mail {
    Account *sender;
    Account **recipients;
    unsigned int recipients_num;
    char subject[MAX_SUBJECT+1];
    char content[MAX_CONTENT+1];
} Mail;

Account* accounts[NUM_OF_CLIENTS];
Mail* mails[MAXMAILS];

unsigned int users_num; // value will be init'ed in read_file() ...
unsigned int mails_num = 0;

int isInt(char *str);

void shutdownSocket(int sock);

bool read_file(char *path);

void parseMessage(int sock, unsigned char type, unsigned int nums);

void serverLoop(int sock, Account* currentAccount);

void show_inbox_operation(int sock, Account* account);

void quit_operation(int sock);

void compose_operation(int sock, Account* account);

void get_mail_operation(int sock, Account* account);

void delete_mail_operation(int sock, Account* account);

void sendToClientPrint(int sock, char *msg);

void sendHalt(int sock);

Account* loginToAccount(int sock);

// function taken from  http://stackoverflow.com/questions/9210528/split-string-with-delimiters-in-c

char** str_split(char* a_str, const char a_delim)
{
    char** result    = 0;
    size_t count     = 0;
    char* tmp        = a_str;
    char* last_comma = 0;
    char delim[2];
    delim[0] = a_delim;
    delim[1] = 0;

    /* Count how many elements will be extracted. */
    while (*tmp)
    {
        if (a_delim == *tmp)
        {
            count++;
            last_comma = tmp;
        }
        tmp++;
    }

    /* Add space for trailing token. */
    count += last_comma < (a_str + strlen(a_str) - 1);

    /* Add space for terminating null string so caller
       knows where the list of returned strings ends. */
    count++;

    result = malloc(sizeof(char*) * count);

    if (result)
    {
        size_t idx  = 0;
        char* token = strtok(a_str, delim);

        while (token)
        {
            assert(idx < count);
            *(result + idx++) = strdup(token);
            token = strtok(0, delim);
        }
        assert(idx == count - 1);
        *(result + idx) = 0;
    }

    return result;
}

Account* getAccountByUsername(char* username) {
    int i;
    for (i = 0; i < users_num; i++) {
        if (strcmp(accounts[i]->username, username) == 0) { return accounts[i]; }
    }
    return NULL;
}


void compose_operation(int sock, Account* account) {
    Mail* currentMail = (Mail*)malloc(sizeof(Mail));
    //char subject[MAX_SUBJECT+1], targets[TOTAL_TO * (MAX_USERNAME + 1)], content[MAX_CONTENT+1];
    char targets[TOTAL_TO * (MAX_USERNAME + 1)];
    Account* tempAccount;

    recvData(sock, targets);
    printf("targets recieved: %s\n", targets);
    recvData(sock, currentMail->subject);
    printf("subject recieved: %s\n", currentMail->subject);

    recvData(sock, currentMail->content);
    printf("content recieved: %s\n", currentMail->content);

    printf("k\n");

    mails[mails_num] = currentMail;
    printf("added mail %d at place %p\n", mails_num, (void*)currentMail);
    printf("%s %s\n", currentMail->content, currentMail->subject);
    //strcpy(currentMail->subject, subject);
    //strcpy(currentMail->content, content);
    currentMail->recipients_num = 0;
    printf("c\n");

    char** tokens = str_split(targets, ',');
    printf("%s",targets);

    // code taken from stack overflow
    if (tokens)
    {
        int i;
        for (i = 0; *(tokens + i); i++)
        {
            printf("a %s a\n", *(tokens + i));
            tempAccount = getAccountByUsername(*(tokens + i));
            currentMail->recipients = (Account**)realloc(currentMail->recipients, (size_t)((currentMail->recipients_num + 1) * sizeof(Account*)));
            currentMail->recipients[currentMail->recipients_num] = tempAccount;
            currentMail->recipients_num++;
            printf("b %p\n", (void*)tempAccount->inbox_mail_indices);

            if (tempAccount->inbox_mail_indices == NULL) {
                tempAccount->inbox_mail_indices = (unsigned short *)malloc((size_t)(sizeof(unsigned short)));
            }
            else {
                tempAccount->inbox_mail_indices = (unsigned short *)realloc(tempAccount->inbox_mail_indices, (size_t)((tempAccount->inbox_size + 1) * sizeof(unsigned short)));
            }
            if (tempAccount->inbox_mail_indices == NULL) {
                perror("Failed allocating memory");
                exit(EXIT_FAILURE);
            }
            printf("t\n");

            tempAccount->inbox_mail_indices[tempAccount->inbox_size] = mails_num; // add mail idx to indices list
            tempAccount->inbox_size++;
            printf("y\n");

            free(*(tokens + i));
        }
        free(tokens);
    }

    printf("z\n");

    mails_num++; // increase number of total mails in system
    sendToClientPrint(sock, "Mail sent\n");
    sendHalt(sock);
}


Mail* account_mail_access(Account* account, short mail_idx) {
    printf("hhello there\n");
    return (mail_idx > account->inbox_size || account->inbox_mail_indices[mail_idx - 1] == MAXMAILS) ? NULL : mails[account->inbox_mail_indices[mail_idx - 1]];
}


void show_inbox_operation(int sock, Account* account) {
    int i;
    char mail_msg[BUF_SIZE];
    Mail* currentMail;
    printf("account size %d \n", account->inbox_size);
    for (i = 1; i <= account->inbox_size; i++) {
        printf("hello\n");
        currentMail = account_mail_access(account, i);
        printf("account pointer %p \n", (void *) currentMail);
        if (currentMail != NULL) {  // mail idx MAXMAILS is marked as a deleted mail
            printf("hello mate\n");
            memset(mail_msg, 0, BUF_SIZE);
            sprintf(mail_msg, "%d", i);
            strcat(mail_msg, " ");
            printf("hello mate2\n");

            strcat(mail_msg, currentMail->sender->username);
            strcat(mail_msg, " ~");
            strcat(mail_msg, currentMail->subject);
            printf("hello mate3\n");

            strcat(mail_msg, " ~");
            strcat(mail_msg, "*\n");
            sendToClientPrint(sock, mail_msg);
        }
    }
    sendHalt(sock);
}


void get_mail_operation(int sock, Account* account) {
    short mail_idx = getDataSize(sock);
    int i;
    Mail* currentMail = account_mail_access(account, mail_idx);

    if (currentMail == NULL) {
        sendToClientPrint(sock, "Invalid selection\n");
    }

else {
        char mail_msg[BUF_SIZE];

        memset(mail_msg, 0, BUF_SIZE);
        strcat(mail_msg, "From: ");
        strcat(mail_msg, currentMail->sender->username);
        strcat(mail_msg, "\nTo: ");
        for (i = 0; i < currentMail->recipients_num; i++) {
            if (i > 0) { strcat(mail_msg, ","); }
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

void delete_mail_operation(int sock, Account* account) {
    short mail_idx = getDataSize(sock);
    Mail* currentMail = account_mail_access(account, mail_idx);

    if (currentMail == NULL) {
        sendToClientPrint(sock, "Invalid selection\n");
    }
    else {
        account->inbox_mail_indices[mail_idx - 1] = MAXMAILS;
    }
    sendHalt(sock);
}
int main(int argc, char *argv[]) {
    mails_num = 0;
    char *port;
    char *path;
    int sock, new_sock;
    socklen_t sin_size;
    struct sockaddr_in myaddr, their_addr;
    Account *currentAccount = NULL;
    if (argc > 3) {
        printf("Incorrect Argument Count");
        exit(1);
    }

    path = argv[1];

    if (argc == 3) {
        if (!isInt(argv[2])) {
            printf("Please enter a valid number as the port number");
            exit(1);
        }
        port = argv[2];
    } else {
        port = DEFAULT_PORT;
    }

    read_file(path);

    /* open IPv4 TCP socket*/
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Could not open socket");
        exit(errno);
    }
    /* IPv4 with given port. We dont care what address*/
    myaddr.sin_family = AF_INET;
    myaddr.sin_port = htons((uint16_t) strtol(port, NULL, 10));
    myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    /* try to bind*/
    if (bind(sock, (struct sockaddr *) &(myaddr), sizeof(myaddr)) < 0) {
        perror("Could not bind socket");
        exit(errno);
    }
    /* try to listen to the ether*/
    if (listen(sock, 1) < 0) {
        perror("Could not listen to socket");
        exit(errno);
    }
    sin_size = sizeof(struct sockaddr_in);
    /* accept the connection*/
    /* by this point we have a connection. play the game*/
    /* we can close the listening socket and play with the active socket*/
    /* send welcome message to the client*/
    //send_to_print(new_sock, "Welcome! I am simple-mail-server.\n");
    /* server's logic*/
    bool keepAlive = true;
    while (keepAlive) {
        if ((new_sock = accept(sock, (struct sockaddr *) &their_addr, &sin_size)) < 0) {
            perror("Could not accept connection");
            exit(errno);
        }
        // reached here, has connection with client
        if ((currentAccount = loginToAccount(new_sock)) == NULL) { continue; }
        serverLoop(new_sock, currentAccount);
    }
    tryClose(sock);
    // todo free dynamic accounts and mails
}

bool read_file(char *path) {
    FILE *fp;
    users_num = 0;   // count how many lines are in the file
    fp = fopen(path, "r");
/*
 *     int current_character;
    while (!feof(fp)) {
        current_character = fgetc(fp);
        if (current_character == '\n')
            users_num++;
    }
    rewind(fp);

    if (accounts == NULL)
        return false;

*/
    // read each line and put into accounts
    while (!feof(fp)) {
        Account* account_ptr = (Account*)malloc(sizeof(Account));
        account_ptr->inbox_mail_indices = 0;
        account_ptr->inbox_mail_indices = NULL;
        accounts[users_num] = account_ptr;

        if (fscanf(fp, "%s\t%s", account_ptr->username, account_ptr->password) < 0) {
            free(account_ptr);
            if (errno != 0) {
                perror("Cannot add account");
            }
        }
        else {
            users_num++;
        }
        //printf("adding account ~%s~ with password ~%s~ \n", accounts[i]->username, accounts[i]->password);
    }
    return true;
}

Account* loginToAccount(int sock) {
    int i, auth_attempts = 0;
    const int AUTH_ATTEMPTS = MAX_LOGIN_ATTEMPTS;
    char username[MAX_USERNAME] = {0};
    char password[MAX_PASSWORD] = {0};
    while (true) {
        send_char(sock, LOG_REQUEST);
        recvData(sock, username);
        recvData(sock, password);
        for (i = 0; i < users_num; i++) {
            if ((strcmp(accounts[i]->username, username) == 0) && (strcmp(accounts[i]->password, password) == 0)) {
                sendToClientPrint(sock, WELCOME_MSG);
                sendHalt(sock); // login successful, wait for input from user
                return accounts[i];
            }
        }

        sendToClientPrint(sock, "LOGIN ATTEMPT FAILED\n");
        send_char(sock, LOG_REQUEST);
        auth_attempts += 1;
        if (auth_attempts >= AUTH_ATTEMPTS) {
            sendToClientPrint(sock, "LOGIN FAILED - GOT KILL FROM SERVER\n");
            send_char(sock, LOG_KILL);
            return NULL;
        }
    }
}

void serverLoop(int sock, Account* currentAccount) {
    // when here, after sock establishment and user auth. keep listening for ops
    //todo - matan - don't forget to validate mail id!!!
    while (true) {
        switch (recv_char(sock)) {
            case OP_SHOWINBOX:
                printf("got show request\n");

                show_inbox_operation(sock, currentAccount);
                break;
            case OP_GETMAIL:
                printf("got get request\n");

                get_mail_operation(sock, currentAccount);
                break;
            case OP_DELETEMAIL:

                printf("got del request\n");

                delete_mail_operation(sock, currentAccount);
                break;
            case OP_QUIT:

                printf("got quit request\n");

                quit_operation(sock);
                return;
            case OP_COMPOSE:

                printf("got compose request\n");

                compose_operation(sock, currentAccount);
                break;
            default:;
        }
    }
}

void quit_operation(int sock) {

}

int isInt(char *str) {
    /* tests if a string is just numbers*/
    if (*str == 0)
        return 0;
    while (*str != 0) {
        if (!isdigit(*str))
            return 0;
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