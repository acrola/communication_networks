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

typedef struct account
{
    char *id;
    char *password;
    unsigned short *inbox_mail_indices;
    unsigned short inbox_size;
} account;

typedef struct mail
{
    account *sender;
    account **recipients;
    unsigned int recipients_num;
    char *mail_subject;
    char *mail_body;
    unsigned int mail_id;
} mail;

account *accounts;
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

bool composeNewMail(account *sender, account **recipients, unsigned int recipients_num, char *mail_subject, char *mail_body);

bool deleteMailFromInbox(account *acc, unsigned int mail_id);

void show_inbox_operation(int sock);

void quit_operation(int sock);

void compose_operation(int sock);

void get_mail_operation(int sock, int mail_id);

void delete_mail_operation(int sock, int mail_id);

int main(int argc, char *argv[])
{
    mails_num = 0;
    char *port;
    char *path;
    int sock, new_sock;
    socklen_t sin_size;
    struct sockaddr_in myaddr, their_addr;
    if (argc > 3)
    {
        printf("Incorrect Argument Count");
        exit(1);
    }

    path = argv[1];

    if (argc == 3)
    {
        if (!isInt(argv[2]))
        {
            printf("Please enter a valid number as the port number");
            exit(1);
        }
        port = argv[2];
    } else
    {
        port = "6423";
    }

    read_file(path);

    /* open IPv4 TCP socket*/
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Could not open socket");
        exit(errno);
    }
    /* IPv4 with given port. We dont care what address*/
    myaddr.sin_family = AF_INET;
    myaddr.sin_port = htons((short) strtol(port, NULL, 10));
    myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    /* try to bind*/
    if (bind(sock, (struct sockaddr *) &(myaddr), sizeof(myaddr)) < 0)
    {
        perror("Could not bind socket");
        exit(errno);
    }
    /* try to listen to the ether*/
    if (listen(sock, 1) < 0)
    {
        perror("Could not listen to socket");
        exit(errno);
    }
    sin_size = sizeof(struct sockaddr_in);
    /* accept the connection*/
    if ((new_sock = accept(sock, (struct sockaddr *) &their_addr, &sin_size)) < 0)
    {
        perror("Could not accept connection");
        exit(errno);
    }
    /* by this point we have a connection. play the game*/
    /* we can close the listening socket and play with the active socket*/
    tryClose(sock);
    /* send welcome message to the client*/
    char connection_msg[] = "Welcome! I am simple-mail-server.\n";
    sendall(new_sock, connection_msg, sizeof(char) * strlen(connection_msg));
    /* server's logic*/
    serverLoop(new_sock);
    return 0;
}

bool read_file(char* path)
{
    FILE *fp;
    users_num = 0;   // count how many lines are in the file
    int current_character;
    fp = fopen(path, "r");
    while (!feof(fp))
    {
        current_character = fgetc(fp);
        if (current_character == '\n')
            users_num++;
    }

    accounts = (account*)malloc(users_num * sizeof(account));
    if (accounts == NULL)
	return false;

    int i;
    // read each line and put into accounts
    for (i = 0; i < users_num; i++)
    {
        fscanf(fp, "%s\t%s", accounts[i].id, accounts[i].password);
    }
    return true;
}

bool permit_user(char *user, char *pass)
{
    int i = 0;
    for (i = 0; i < users_num; ++i)
    {
        if ((strcmp(accounts[i].id, user) == 0) && (strcmp(accounts[i].password, pass) == 0))
            return true;
    }

    return false;
}

void serverLoop(int sock)
{
    /* TODO - complete client's user authentication*/
    long buff = 0;
    int len = 4;
    /* get client reply*/
    while (recvall(sock, ((long *) &buff), &len) == 0)
    {
        int accepted = 1;
        unsigned long op;
        unsigned long num;
        if (len != 4)
        {
            perror("Error receiving data from client");
            tryClose(sock);
            exit(errno);
        }
        /* split the two parts of the client message*/
        op = buff & 0xE00000;
        num = buff & 0xFFFF;
        /* if op == 0 then there was some error in the client. reject the operation*/
        if (op == 0)
        {
            accepted = 0;
        } else
        {
            /* if out of range, reject*/
            if (num > MAILS_COUNT || num <= 0)
            {
                accepted = 0;

            } else
            {
                /* choose the choice based on op, as per protocol specification*/
                switch (op)
                {
                    /* SHOW_INBOX operation*/
                    case 0x200000:
                        show_inbox_operation(sock);
                        break;
                        /* GET_MAIL operation*/
                    case 0x400000:
                        get_mail_operation(sock, (unsigned short) num - 1);
                        break;
                        /* DELETE_MAIL operation*/
                    case 0x600000:
                        delete_mail_operation(sock, (unsigned short) num - 1);
                        break;
                        /* QUIT operation*/
                    case 0x800000:
                        quit_operation(sock);
                        break;
                        /* COMPOSE operation*/
                    case 0xA00000:
                        compose_operation(sock);
                        break;
                }
            }
        }
    }
    if (len != 0 && len != 4)
    {
        perror("Error receiving data from client");
    }
    tryClose(sock);
    exit(0);
}

int isInt(char *str)
{
    /* tests if a string is just numbers*/
    if (*str == 0)
        return 0;
    while (*str != 0)
    {
        if (!isdigit(*str))
            return 0;
        str++;
    }
    return 1;
}

int sendall(int sock, char *buf, int *len)
{
    /* sendall code as seen in recitation*/
    int total = 0; /* how many bytes we've sent */
    int bytesleft = *len; /* how many we have left to send */
    int n;
    while (total < *len)
    {
        n = send(sock, buf + total, bytesleft, 0);
        if (n == -1)
        { break; }
        total += n;
        bytesleft -= n;
    }
    *len = total; /* return number actually sent here */
    return n == -1 ? -1 : 0; /*-1 on failure, 0 on success */
}

int recvall(int sock, char *buf, int *len)
{
    int total = 0; /* how many bytes we've read */
    int bytesleft = *len; /* how many we have left to read */
    int n;
    while (total < *len)
    {
        n = recv(sock, buf + total, bytesleft, 0);
        if (n == -1)
        { break; }
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

void tryClose(int sock)
{
    if (close(sock) < 0)
    {
        perror("Could not close socket");
        exit(errno);
    }
}

void shutdownSocket(int sock)
{
    int res = 0;
    char buff[10];
    /* send shtdwn message, and stop writing*/
    shutdown(sock, SHUT_WR);
    while (1)
    {
        /* read until the end of the stream */
        res = recv(sock, buff, 10, 0);
        if (res < 0)
        {
            perror("Shutdown error");
            exit(errno);
        }
        if (res == 0)
            break;
    }

    tryClose(sock);
    exit(0);
}

int lookForMailInInbox(account *acc, unsigned int mail_id)
{
    int i;
    for (i = 0; i < acc->inbox_size; i++)
    {
        if (acc->inbox_mail_indices[i] != NULL)
            if (acc->inbox_mail_indices[i] == mail_id)
                return i;
    }
    return 32000;
}

<<<<<<< HEAD
bool addMailToInbox(account* recipient, unsigned int mail_id){
	recipient->inbox_size = recipient->inbox_size + 1;
	recipient->inbox_mail_indices = (unsigned short *)realloc(inbox_size*sizeof(unsigned short));
	if (recipient->inbox_mail_indices == NULL)
		return false;
	recipient->inbox_mail_indices[inbox_size - 1] = mail_id;
	return true;
}

bool composeNewMail(account* sender, account** recipients, unsigned int recipients_num, char* mail_subject, char* mail_body){
	int success = 1;
	mail* new_mail = (mail *)malloc(sizeof(mail));
	
	if (new_mail == NULL)
		  return false;
	
	mails_num++;
	
	new_mail->sender = sender;
	new_mail->recipients = recipients;
	new_mail->recipients_num = recipients_num;
	new_mail->mail_subject = mail_subject;
	new_mail->mail_body = mail_body;
	new_mail->mail_id = mails_num - 1;
	mails[mails_num - 1] = new_mail;
	int i;
	for (i = 0; i < recipients_num; i++){
		success *= addMailToInbox(recipients[i], new_mail->mail_id);
	}
	return success;
}

bool addMailToInbox(account *recipient, unsigned int mail_id)
{
    recipient->inbox_size = recipient->inbox_size + 1;
    acc->inbox_mail_indices = (unsigned short *) realloc(inbox_size);
    acc->inbox_mail_indices[inbox_size - 1] = mail_id;
}

bool composeNewMail(account *sender, account **recipients, unsigned int recipients_num, char *mail_subject, char *mail_body)
{
    int success = 1;
    mails_num++;
    mail *new_mail = (mail *) malloc(sizeof(mail));
    new_mail->sender = sender;
    new_mail->recipients = recipients;
    new_mail->recipients_num = recipients_num;
    new_mail->mail_subject = mail_subject;
    new_mail->mail_body = mail_body;
    new_mail->mail_id = mails_num - 1;
    mails[mails_num - 1] = new_mail;
    int i;
    for (i = 0; i < recipients_num; i++)
    {
        success *= addMailToInbox(recipients[i], new_mail->mail_id);
    }
    return success;
}

bool deleteMailFromInbox(account *acc, unsigned int mail_id)
{
    int i = lookForMailInInbox(acc, mail_id);
    if (i == 32000)
        return false;
    acc->inbox_mail_indices[i] = NULL;
    return true;
}
