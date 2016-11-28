#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <stdbool.h>
#include mail_common.c

typedef struct Account {
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];
    unsigned short *inbox_mail_indices;
    unsigned short inbox_size;
} Account;

typedef struct Mail {
    account *sender;
    account **recipients;
    unsigned int recipients_num;
    char subject[MAX_SUBJECT];
    char content[MAX_CONTENT];
} Mail;

Account accounts[NUM_OF_CLIENTS];
Mail mails[MAILS_COUNT];

unsigned int accounts_num; // value will be init'ed in read_file() ...
unsigned int mails_num = 0;

int isInt(char *str);

void shutdownSocket(int sock);

void tryClose(int sock);

void read_file(char *path);

void parseMessage(int sock, unsigned char type, unsigned int nums);

void serverLoop(int sock);

void show_inbox_operation(int sock, Account* account);

void quit_operation(int sock);

void compose_operation(int sock, Account* account);

void get_mail_operation(int sock, Account* account);

void delete_mail_operation(int sock, Account* account);

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

short twoBytesToShort(char arr[]) {
    short* shortPtr = (short*)arr;
    return *arr;
}

Account* getAccountByUsername(char* username) {
    int i;
    for (i = 0; i < accounts_num; i++) {
        if (strcmp(accounts[i]->username, username)) { return &accounts[i]; }
    }
    return NULL;
}


void compose_operation(int sock, Account* account) {
    char subject[1024], targets[1024], content[1024];
    getData(sock, subject);
    getData(sock, targets);
    getData(sock, content);
    Mail* currentMail = &mails[mails_num];
    currentMail->subject = subject;
    currentMail->content = content;
    currentMail->recipients_num = 0;
    tokens = str_split(targets, ',');
    Account* tempAccount;

    // code taken from stack overflow
    if (tokens)
    {
        int i;
        for (i = 0; *(tokens + i); i++)
        {
            tempAccount = getAccountByUsername(*(tokens + i));
            currentMail->recipients[currentMail->recipients_num] = tempAccount;
            currentMail->recipients_num++;
            tempAccount->inbox_mail_indices = (unsigned short *)realloc((inbox_size + 1) * sizeof(unsigned short));
            if (tempAccount->inbox_mail_indices == NULL) {
                perror("Failed allocating memory");
                exit(EXIT_FAILURE);
            }
            tempAccount->inbox_mail_indices[tempAccount->inbox_size] = mails_num;
            tempAccount->inbox_size++;
            free(*(tokens + i));
        }
        free(tokens);
    }
    mails_num++;
    sendToClientPrint(sock, "Mail sent\n");
    sendHalt(sock);
}


void show_inbox_operation(int sock, Account* account) {
    int i;
    char mail_msg[MAX_MAIL_MSG];
    for (i = 0; i < currentAccount->inbox_size; i++) {
        if (account->inbox_mail_indices[i] == MAXMAILS) { continue; } // mail idx MAXMAILS is marked as a deleted mail
        memset(mail_msg, 0, MAX_MAIL_MSG);
        itoa(i, mail_msg, 10);
        strcat(mail_msg, " ");
        strcat(mail_msg, mails[account->inbox_mail_indices[i]]->sender->username);
        strcat(mail_msg, " ~");
        strcat(mail_msg, mails[account->inbox_mail_indices[i]]->subject);
        strcat(mail_msg, " ~");
        strcat(mail_msg, "*\n");
        sendToClientPrint(sock, mail_msg);
    }
    sendHalt(sock);
}


bool canRead(Mail* mail, Account* account) {
    int i;
    for (i = 0; i < mail->recipients_num; i++) {
        if (mail->recipients[i] == account) {
            return true;
        }
    }
    return false;
}

void get_mail_operation(int sock, Account* account) {
    short mail_idx = recieveTwoBytesAndCastToShort(sock);
    int i;
    Mail* currentMail = &mails[mail_idx];
    char mail_msg[MAX_MAIL_MSG];
    if (canRead(mail, account)) {
        memset(mail_msg, 0, MAX_MAIL_MSG);
        strcat(mail_msg, "From: ");
        strcat(mail_msg, mail->sender->username);
        strcat(mail_msg, "\nTo: ");
        for (i = 0; i < mail->recipients_num; i++) {
            if (i > 0) { strcat(mail_msg, ","); }
            strcat(mail_msg, mail->recipients[i]->username);
        }
        strcat(mail_msg, "\nSubject: ");
        strcat(mail_msg, mail->subject);
        strcat(mail_msg, "\nText: ");
        strcat(mail_msg, mail->content);
        strcat(mail_msg, "\n");
        sendToClientPrint(sock, mail_msg);
    }
    else {
        sendToClientPrint(sock, "Failed to get mail\n");
    }
    sendHalt(sock);
}

void delete_mail_operation(int sock, Account* account) {
    short mail_idx = recieveTwoBytesAndCastToShort(sock);
    int i;
    if (i < account->inbox_size || account->inbox_mail_indices[i] == MAXMAILS) {
        sendToClientPrint(sock, "Invalid selection\n");
    }
    else {
        account->inbox_mail_indices[i] == MAXMAILS;
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
    Accunt* currentAccount = NULL;
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
        port = "6423";
    }

    read_file(path);

    /* open IPv4 TCP socket*/
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Could not open socket");
        exit(errno);
    }
    /* IPv4 with given port. We dont care what address*/
    myaddr.sin_family = AF_INET;
    myaddr.sin_port = htons((short) strtol(port, NULL, 10));
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
    tryClose(sock);
    /* send welcome message to the client*/
    //send_to_print(new_sock, "Welcome! I am simple-mail-server.\n");
    /* server's logic*/
    while (true) {
        if ((new_sock = accept(sock, (struct sockaddr *) &their_addr, &sin_size)) < 0) {
            perror("Could not accept connection");
            exit(errno);
        }
        // reached here, has connection with client
        if ((currentAccount = loginToAccount(new_sock)) == NULL) { continue; }
        serverLoop(new_sock, currentAccount);
    }

}

bool read_file(char *path) {
    FILE *fp;
    users_num = 0;   // count how many lines are in the file
    int current_character;
    fp = fopen(path, "r");
    while (!feof(fp)) {
        current_character = fgetc(fp);
        if (current_character == '\n')
            users_num++;
    }

    accounts = (account *) malloc(users_num * sizeof(account));
    if (accounts == NULL)
        return false;

    int i;
    // read each line and put into accounts
    for (i = 0; i < users_num; i++) {
        fscanf(fp, "%s\t%s", accounts[i].username, accounts[i].password);
    }
    return true;
}

Account* loginToAccount(int sock) {
    int i, auth_attempts = 0;
    const int AUTH_ATTEMPTS = 3;
    char username[MAX_USERNAME] = {0};
    char password[MAX_PASSWORD] = {0};
    char user_len;
    char password_len;
    while (true) {
        send_char(sock, LOG_REQUEST);
        getData(sock, username);
        getData(sock, password);
        for (i = 0; i < users_num; i++) {
            if ((strcmp(accounts[i].username, username) == 0) && (strcmp(accounts[i].password, password) == 0)) {
                sendHalt(sock); // login successful, wait for input from user
                return &accounts[i];
            }
        }

        send_char(sock, LOG_REQUEST);
        auth_attempts += 1;
        if (auth_attempts >= AUTH_ATTEMPTS) {
            send_char(sock, LOG_KILL);
            return NULL;
        }
    }
}

void serverLoop(int sock, Account* currentAccount) {
    // when here, after sock establishment and user auth. keep listening for ops

    while (true) {
        switch (recv_char(sock)) {
            case OP_SHOWINBOX:
                show_inbox_operation(sock, currentAccount);
                break;
            case OP_GETMAIL:
                get_mail_operation(sock, currentAccount);
                break;
            case OP_DELETEMAIL:
                delete_mail_operation(sock, currentAccount);
                break;
            case OP_QUIT:
                quit_operation(sock);
                return;
            case OP_COMPOSE:
                compose_operation(sock, currentAccount);
                break;
        }
    }
}

int quit_operation(sock) {

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



void tryClose(int sock) {
    if (close(sock) < 0) {
        perror("Could not close socket");
        exit(errno);
    }
}

void shutdownSocket(int sock) {
    int res = 0;
    char buff[10];
    /* send shtdwn message, and stop writing*/
    shutdown(sock, SHUT_WR);
    while (1) {
        /* read until the end of the stream */
        res = recv(sock, buff, 10, 0);
        if (res < 0) {
            perror("Shutdown error");
            exit(errno);
        }
        if (res == 0)
            break;
    }

    tryClose(sock);
    exit(0);
}


