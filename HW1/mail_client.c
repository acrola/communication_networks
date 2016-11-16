#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>

void shutdownSocket(int sock);
int sendall(int sock, char* buf, int*len);
int sendMessage(int sock, unsigned int operation, unsigned short mail_id);
int connectToHostname(int sock, char* hostname, char* port);
void tryClose(int sock);
int recvall(int sock, char* buf, int*len);

int main(int argc, char* argv[]){
	int sock;
	char* hostname;
	char* port;
	/* place default port if required*/
	if(argc < 3)
	{
		port = "6423";
	}
	else
	{	
		port = argv[2];
	}
	/* place default hostname if required*/
	if(argc == 1)
	{
		hostname = "localhost";
	}
	else
	{
		hostname = argv[1];
	}

	/* open TCP socket with IPv4*/
	if((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("Could not open socket");
		exit(errno);
	}
	
	/* get correct input from user*/
	int inputDone = 0;
	char input;
	while(!inputDone)
	{
		/* TODO - get from user, username and password correctly, the best way i thought of it is to read char by char*/
		scanf("%c", &input);
		//if (...)
		  //inputDone = 1;
		/* TODO - if authentication was uncorrect/the input wan't valid- disconnect from the server*/
	}

	/* connect to server*/
	connectToHostname(sock, hostname, port);

	/* TODO - after connection was established succesfully, get input operations from user, parse correctly and use "sendMessage" function to send to server*/
	/* TODO - complete reading something from server*/
	while(((recvall(sock,(char*)&buff,&len1)) == 0) &&
			((recvall(sock,(char*)&nums,&len2)) == 0))
	{
		int num;
		int inputDone = 0;
		char c;
		unsigned short n;
		if(len1 != 1 || len2 != 4)
		{
			perror("Error receiving data from server");
			tryClose(sock);
			exit(errno);
		}

		
		/* if number is not in range, send 1023 to make move invalid*/
		n = (num > 0 && num <= 1000)? (unsigned short)num : 0x3FF;
		/* try to send the message*/
		if(sendMessage(sock, c, n) < 0)
		{
			perror("Error sending message");
			break;
		}
		len1 = 1;
		len2 = 4;
	}
	/* if we had a read error, display it*/
	if(len1 !=0 && len2!= 0)
	{
		if(len1 != 1 || len2 != 4)
		{
			perror("Error receiving message");
		}
	}
	tryClose(sock);
	return 0;
	}

int sendMessage(int sock, unsigned int operation, unsigned short mail_id)
{
	/* TODO - implement with protocol I did in the server*/
}
int sendall(int sock, char* buf, int*len)
{
	/* sendall code as seen in recitation*/
	int total = 0; /* how many bytes we've sent */
	int bytesleft = *len; /* how many we have left to send */
	int n;
	while(total < *len) {
		n = send(sock, buf+total, bytesleft, 0);
		if (n == -1) { break; }
		total += n;
		bytesleft -= n;
	}
	*len = total; /* return number actually sent here */
	return n == -1 ? -1:0; /*-1 on failure, 0 on success */
}

int connectToHostname(int sock, char* hostname, char* port)
{
	const char separator[2] = " ";
	char *token;
	
	char username_line[256];
	char* username;
	if (!fgets(user_line, sizeof(user_line), stdin)) {
	      printf("Error reading username, probably too long.\n");
	      exit(1);
	}
	token = strtok(user_line, separator);
	username = strtok(NULL, separator);

	char password_line[256];
	char* password;
	if (!fgets(password_line, sizeof(password_line), stdin)) {
	      printf("Error reading password, probably too long.\n");
	      exit(1);
	}
	token = strtok(password_line, separator);
	password = strtok(NULL, separator);
	
	int rv;
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_in *h;
	struct sockaddr_in dest_addr;

	/* server addr is IPv4*/
	dest_addr.sin_family = AF_INET;
	/* write server port*/
	dest_addr.sin_port = htons((short)strtol(port, NULL,10));
	/* zero out hints (since garbage is bad)*/
	memset(&hints, 0, sizeof(hints));
	/* we want IPv4 address and TCP*/
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	/* try get address info*/
	if((rv = getaddrinfo(hostname, port, &hints, &servinfo)) != 0)
	{
		fprintf(stderr, "Error getting address info: %s\n", gai_strerror(rv));
		exit(1);
	}
	p = servinfo;
	while(p!= NULL)
	{
		/* put last address into sent pointer*/
		h = (struct sockaddr_in *)p->ai_addr;
		dest_addr.sin_addr = h->sin_addr;
		if(connect(sock, (struct sockaddr*)(&dest_addr), sizeof(struct sockaddr)) == 0)
				break;/* if connection succesfull*/
		p = p->ai_next;
	}
	freeaddrinfo(servinfo);
	if(p == NULL)
	{
		/*We didnt find a host*/
		tryClose(sock);
		perror("Could not connect to server");
		exit(errno);
	}
	printf("Connected to server\n");
	return 0;
}
void tryClose(int sock)
{
	if(close(sock) <0)
	{
		perror("Could not close socket");
		exit(errno);
	}
}
int recvall(int sock, char* buf, int*len)
{
	int total = 0; /* how many bytes we've read */
	int bytesleft = *len; /* how many we have left to read */
	int n;
	while(total < *len) {
		n = recv(sock, buf+total, bytesleft, 0);
		if (n == -1) { break; }
		if(n == 0){*len = 0; n=-1; break;}
		total += n;
		bytesleft -= n;
	}
	*len = total; /* return number actually sent here */
	return n == -1 ? -1:0; /*-1 on failure, 0 on success */
}
void shutdownSocket(int sock)
{
	int res = 0;
	char buff[10];
	/* send shtdwn message, and stop writing*/
	shutdown(sock, SHUT_WR);
	while(1)
	{
		/* read until the end of the stream*/
		res = recv(sock, buff, 10, 0);
		if(res < 0)
		{
			perror("Shutdown error");
			exit(errno);
		}
		if(res == 0)
			break;
	}
	tryClose(sock);
	exit(0);
}
