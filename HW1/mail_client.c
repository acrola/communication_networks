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
int sendMessage(int sock, char letter, unsigned short amount);
void parseMessage(int sock, unsigned char type, unsigned int nums);
int connectToHostname(int sock, char* hostname, char* port);
void tryClose(int sock);
int recvall(int sock, char* buf, int*len);

int main(int argc, char* argv[]){
	int sock;
	char* hostname;
	char* port;
	/* buff is byte 1 (status byte) in the protocol*/
	char buff;
	int len1 = 1;
	int len2 = 4;

	/* nums is bytes 2-5 (heap size bytes) in the protocol*/
		unsigned int nums;

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
	if((sock = socket(PF_INET, SOCK_STREAM, 0))<0)
	{
		perror("Could not open socket");
		exit(errno);
	}
	/* connect to server*/
	connectToHostname(sock, hostname, port);

	/* read into the buffers*/
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
		/* parse and print message*/
		parseMessage(sock, buff, nums);

		printf("Your turn:\n");

		/* get correct input from user*/
		while(!inputDone)
		{
			scanf("%c", &c);
			if(!isalpha(c))
			{
				fprintf(stderr, "Error. Please enter a letter\n");
				continue;
			}
			if(c == 'Q')
			{

				shutdownSocket(sock);
				exit(0);
			}
			scanf(" %i", &num);
			getchar();
			inputDone = 1;
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

int sendMessage(int sock, char letter, unsigned short amount)
{
	int len = 2;/* size of short*/
	if(letter == 'A')
	{
		/* 01xxxxaa aaaaaaaa*/
		amount = amount | 0x4000;
	}
	else if (letter == 'B')
	{
		/* 10xxxxaa aaaaaaaa*/
		amount = amount | 0x8000;
	}
	else if (letter == 'C')
	{
		/* 11xxxxaa aaaaaaaa*/
		amount = amount | 0xC000;
	}
	else
	{
		/* 00xxxxaa aaaaaaaa*/
		amount = amount & 0x3FFF;
	}
	return sendall(sock, ((char*)&amount), &len);
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
void parseMessage(int sock, unsigned char type, unsigned int nums)
{
	unsigned short a,b,c;
	if(!(type & 0x80))
	{
		/* not initial*/
		if(type & 0x40)
		{
			printf("Move accepted\n");
		}
		else
		{
			printf("Illegal move\n");
		}
	}
	
	a = (nums >> 20) & 0x3FF;
	b = (nums >> 10) & 0x3FF;
	c = nums & 0x3FF;
	printf("Heap A: %i\nHeap B: %i\nHeap C: %i\n", a, b, c);
	if(type & 0x20)
	{
		printf("Server win!\n");
		tryClose(sock);
		exit(0);
	}
	else if(type & 0x10)
	{
		printf("You win!\n");
		tryClose(sock);
		exit(0);
	}
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
