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

#define MAILS_COUNT 32000
#define NUM_OF_CLIENTS 20
#define MAX_USERNAME 50
#define MAX_PASSWORD 50
#define MAX_SUBJECT 100
#define MAX_CONTENT 2000







#define ASK_USER 1
#define ASK_PASS 2
#define LOGIN_SUCCESS 3
#define LOGIN_FAILURE 4
#define LOGIN_KILL 5




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
    char mail_subject[MAX_SUBJECT];
    char mail_body[MAX_CONTENT];
    unsigned int mail_id;
} Mail;

typedef enum _eOpCode {
    OPCODE_ERROR = 0,
    OPCODE_SHOW_INBOX = 1,
    OPCODE_GET_MAIL = 2,
    OPCODE_DELETE_MAIL = 3,
    OPCODE_COMPOSE = 4,
    OPCODE_QUIT = 5,
    OPCODE_PRINT = 6
} eOpCode;

// TODO complete rellevant masks
/*
typedef enum _eMasks
{
    MASK_GET_OPCODE 0xE00000,
    MASK_GET_CLIENT_ID 0xFFFF
} eMasks;
*/

account accounts[NUM_OF_CLIENTS];
mail *mails[MAILS_COUNT];
unsigned int accounts_num;
unsigned int mails_num;
account *curr_account;

int isInt(char *str);

int sendall(int sock, char *buf, int *len);

void shutdownSocket(int sock);

void tryClose(int sock);

int recvall(int sock, char *buf, int *len);

void read_file(char *path);

bool permit_user(char *user, char *pass);

void parseMessage(int sock, unsigned char type, unsigned int nums);

void serverLoop(int sock);

bool lookForMailInInbox(account *acc, unsigned int mail_id);

bool addMailToInbox(account *recipient, unsigned int mail_id);

bool
composeNewMail(account *sender, account **recipients, unsigned int recipients_num, char *mail_subject, char *mail_body);

bool deleteMailFromInbox(account *acc, unsigned int mail_id);

void show_inbox_operation(int sock, Account* account);

void quit_operation(int sock);

void compose_operation(int sock, Account* account);

void get_mail_operation(int sock, Account* account);

void delete_mail_operation(int sock, Account* account);

// fuction taken from  http://stackoverflow.com/questions/9210528/split-string-with-delimiters-in-c

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

void fillBufferUnknownSize(int sock, char* buff) {
    recvall_imm(sock, buff, recieveTwoBytesAndCastToShort(sock));
}

void compose_operation(int sock, Account* account) {
    char subject[1024], targets[1024], body[1024];
    fillBufferUnknownSize(sock, subject);
    fillBufferUnknownSize(sock, targets);
    fillBufferUnknownSize(sock, body);
    Mail* currentMail = &mails[mails_num];
    currentMail->mail_subject = subject;
    currentMail->mail_body = body;
    currentMail->recipients_num = 0;
    tokens = str_split(targets, ',');
    Account* tempAccount;

    if (tokens)
    {
        int i;
        for (i = 0; *(tokens + i); i++)
        {
            tempAccount = getAccountByUsername(*(tokens + i));
            currentMail->recipients[currentMail->recipients_num] = tempAccount;
            currentMail->recipients_num++;
            tempAccount->inbox_mail_indices[tempAccount->inbox_size] = mails_num;
            tempAccount->inbox_size++;
            free(*(tokens + i));
        }
        printf("\n");
        free(tokens);
    }
    mails_num++;
    send_to_print(sock, "Mail sent\n");
    send_imm1(sock, 'H');
}


void show_inbox_operation(int sock, Account* account) {
    int i;
    char mail_msg[1024];
    for (i = 0; i < currentAccount->inbox_size; i++) {
        if (account->inbox_mail_indices[i] == MAILS_COUNT) { continue; } // mail idx 32000 is marked as deleted
        memset(mail_msg, 0);
        itoa(i, mail_msg, 10);
        strcat(mail_msg, " ");
        strcat(mail_msg, mails[account->inbox_mail_indices[i]]->sender->username);
        strcat(mail_msg, " ~");
        strcat(mail_msg, mails[account->inbox_mail_indices[i]]->mail_subject);
        strcat(mail_msg, " ~");
        strcat(mail_msg, "*\n");
        send_to_print(sock, mail_msg);
    }
    send_imm1(sock, 'H'); // operation halted, now waiting for more input from user
}

short reciveTwoBytesAndCastToShort(int sock) {
    char twoBytesFromServer[2];
    short* result;
    recvall_imm(sock, twoBytesFromServer, 2);
    result = (short*)twoBytesFromServer;
    return *result;
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
    mail* currentMail = mails[mail_idx];
    char mail_msg[4096];
    memset(mail_msg, 0);
    if (canRead(mail, account)) {
        strcat(mail_msg, "From: ");
        strcat(mail_msg, mail->sender->username);
        strcat(mail_msg, "\nTo: ");
        for (i = 0; i < mail->recipients_num; i++) {
            if (i > 0) { strcat(mail_msg, ","); }
            strcat(mail_msg, mail->recipients[i]->username);
        }
        strcat(mail_msg, "\nSubject: ");
        strcat(mail_msg, mail->mail_subject);
        strcat(mail_msg, "\nText: ");
        strcat(mail_msg, mail->mail_body);
        strcat(mail_msg, "\n");
        send_to_print(sock, mail_msg);
    }
    else {
        send_to_print(sock, "Failed to get mail\n");
    }
    send_imm1(sock, 'H');
}

void delete_mail_operation(int sock, Account* account) {
    short mail_idx = recieveTwoBytesAndCastToShort(sock);
    int i;
    if (i < account->inbox_size || account->inbox_mail_indices[i] == MAILS_COUNT) {
        send_to_print(sock, "Invalid selection\n");
    }
    else {
        account->inbox_mail_indices[i] == MAILS_COUNT;
    }
    send_imm1(sock, 'H');
}

bool send_to_print(int sock, char* msg) {
    send_imm1(sock, OPCODE_PRINT, 1);
    sendall_imm(sock, strlen(msg), 2);
    sendall_imm(sock, msg, strlen(msg));
}

int main(int argc, char *argv[]) {
    mails_num = 0;
    char *port;
    char *path;
    int sock, new_sock;
    socklen_t sin_size;
    struct sockaddr_in myaddr, their_addr;
    accunt* currentAccount;
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


account* loginToAccount(int sock) {
    int i, auth_attempts = 0;
    char username[1024] = {0};
    char password[1024] = {0};
    char user_len;
    char password_len;
    while (true) {
        send_imm1(sock, ASK_USER);
        recvall_imm(sock, &user_len, 1);
        recvall(sock, username, user_len);
        send_imm1(sock, ASK_PASS);
        recvall_imm(sock, &password_len, 1);
        recvall(sock, password, password_len);

        for (i = 0; i < users_num; i++) {
            if ((strcmp(accounts[i].username, username) == 0) && (strcmp(accounts[i].password, password) == 0)) {
                send_imm1(sock, LOGIN_SUCCESS);
                return &accounts[i];
            }
        }

        send_imm1(sock, LOGIN_FAILURE);
        auth_attempts += 1;
        if (auth_attempts == 3) {
            send_imm1(sock, LOGIN_KILL);
            perror("Error receiving data from client");
            return NULL;
        }
    }
}

void serverLoop(int sock, account* currentAccount) {
    // when here, after sock establishment and user auth. keep listening for ops

    char op;

    while (true) {
        op = recv_imm1(sock);
        switch (op) {
            /* SHOW_INBOX operation*/
            case eOpCode.OPCODE_SHOW_INBOX:
                show_inbox_operation(sock, currentAccount);
                break;
                /* GET_MAIL operation*/
            case eOpCode.OPCODE_GET_MAIL:
                get_mail_operation(sock, currentAccount);
                break;
                /* DELETE_MAIL operation*/
            case eOpCode.OPCODE_DELETE_MAIL:
                delete_mail_operation(sock, currentAccount);
                break;
                /* QUIT operation*/
            case eOpCode.OPCODE_QUIT:
                quit_operation(sock);
                return;
                /* COMPOSE operation*/
            case eOpCode.OPCODE_COMPOSE:
                compose_operation(sock, currentAccount);
                break;

        }
    }
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

int sendall(int sock, char *buf, int *len) {
    /* sendall code as seen in recitation*/
    int total = 0; /* how many bytes we've sent */
    int bytesleft = *len; /* how many we have left to send */
    int n;
    while (total < *len) {
        n = send(sock, buf + total, bytesleft, 0);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
    }
    *len = total; /* return number actually sent here */
    return n == -1 ? -1 : 0; /*-1 on failure, 0 on success */
}

int recvall(int sock, char *buf, int *len) {
    int total = 0; /* how many bytes we've read */
    int bytesleft = *len; /* how many we have left to read */
    int n;
    while (total < *len) {
        n = recv(sock, buf + total, bytesleft, 0);
        if (n == -1) { break; }
        if (n == 0) {
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

int sendall_imm(int sock, char* buf, int len) {
    int temp = len;
    return sendall(sock, buf, &temp);
}

int send_imm1(int sock, char msg) {
    sendall_imm(sock, &msg, 1);
}

int recvall_imm(int sock, char* buf, int len) {
    int temp = len;
    return recvall(sock, buf, &temp);
}

char recv_imm1(int sock) {
    char temp;
    recvall_imm(sock, &temp, 1);
    return temp;
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

int lookForMailInInbox(account *acc, unsigned int mail_id) {
    int i;
    if (mail_id > acc->inbox_size) {
        // personal mail ouot of  bounds
        return MAILS_COUNT;
    }
    return acc->inbox_mail_indices[mail_id - 1]; // if user asked for mail 1, we will return element 0 in mail array
}

bool addMailToInbox(account *recipient, unsigned int mail_id) {
    recipient->inbox_size = recipient->inbox_size + 1;
    recipient->inbox_mail_indices = (unsigned short *) realloc(inbox_size * sizeof(unsigned short));
    if (recipient->inbox_mail_indices == NULL)
        return false;
    recipient->inbox_mail_indices[inbox_size - 1] = mail_id;
    return true;
}

bool composeNewMail(account *sender, account **recipients, unsigned int recipients_num, const char *mail_subject,
                    const char *mail_body) {
    int success = 1;
    mail *new_mail = (mail *) malloc(sizeof(mail));

    if (new_mail == NULL)
        return false;

    mails_num++;

    new_mail->sender = sender;
    new_mail->recipients = recipients;
    new_mail->recipients_num = recipients_num;
    strcpy(new_mail->mail_subject, mail_subject);
    strcpy(new_mail->mail_body, mail_body);
    new_mail->mail_id = mails_num - 1;
    mails[mails_num - 1] = new_mail;
    int i;
    for (i = 0; i < recipients_num; i++) {
        success ^= addMailToInbox(recipients[i], mails_num - 1);
    }
    return success;
}

bool deleteMailFromInbox(account *acc, unsigned int mail_id) {
    int i = lookForMailInInbox(acc, mail_id);
    // use MAILS_COUNT as index of unfound mail
    if (i == MAILS_COUNT) { return false; }
    acc->inbox_mail_indices[i] = MAILS_COUNT;   // we mark an invalid mail by setting him to point to mail MAILS_COUNT,
                                                // because it is out of bounds (0 : MAILS_COUNT - 1)
    return true;
}