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

int isInt(char *);
int sendall(int sock, char* buf, int*len);
void playGame(int sock, int a, int b, int c);
int sendState(int sock,int initial, int accept, int winLose, int a, int b, int c);
void doServerTurn(int sock, int* a, int* b, int* c);
int testWin(int a, int b, int c);
void shutdownSocket(int sock);
void tryClose(int sock);
int recvall(int sock, char* buf, int*len);

int main(int argc, char* argv[]) {
	char* port;
	int a,b,c;
	int sock, new_sock;
	socklen_t sin_size;
	struct sockaddr_in myaddr, their_addr;
	if(argc < 4 || argc > 5)
	{
		printf("Incorrect Argument Count");
		exit(1);
	}
	if(!isInt(argv[1]) || !isInt(argv[2]) || !isInt(argv[3]))
	{
		printf("One or more of the parameters is not a valid number");
		exit(1);
	}
	/* test range of argument numbers*/
	a = strtol(argv[1], NULL, 10);
	b = strtol(argv[2], NULL, 10);
	c = strtol(argv[3], NULL, 10);
	if(a < 0 || a > 1000 || b < 0 || b > 1000 || c < 0 || c > 1000)
	{
		printf("One or more of the parameters is an illegal heap size");
	}
	if(argc == 5)
	{
		if(!isInt(argv[4]))
		{
			printf("Please enter a valid number as the port number");
			exit(1);
		}
		port = argv[4];
	}
	else
	{
		port = "6444";
	}

	/* open IPv4 TCP socket*/
	if((sock = socket(PF_INET, SOCK_STREAM, 0))<0)
	{
		perror("Could not open socket");
		exit(errno);
	}
	/* IPv4 with given port. We dont care what address*/
	myaddr.sin_family = AF_INET;
	myaddr.sin_port = htons((short)strtol(port, NULL, 10));
	myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	/* try to bind*/
	if(bind(sock, (struct sockaddr*)&(myaddr), sizeof(myaddr))<0)
	{
		perror("Could not bind socket");
		exit(errno);
	}
	/* try to listen to the ether*/
	if(listen(sock, 1)<0)
	{
		perror("Could not listen to socket");
		exit(errno);
	}
	sin_size = sizeof(struct sockaddr_in);
	/* accept the connection*/
	if((new_sock = accept(sock, (struct sockaddr*)&their_addr, &sin_size))<0)
	{
		perror("Could not accept connection");
		exit(errno);
	}
	/* by this point we have a connection. play the game*/
	/* we can close the listening socket and play with the active socket*/
	tryClose(sock);
	playGame(new_sock,a , b, c);
	return 0;
}
void playGame(int sock, int a, int b, int c)
{
	unsigned short buff = 0;
	int* choice = 0;
	int len = 2;
	/* start by sending the initial state of the game*/
	sendState(sock, 1, 0, 0, a, b, c);
	/* get client reply in a short*/
	while(recvall(sock, ((char*)&buff), &len) == 0)
	{
		int accepted = 1;
		unsigned short letter;
		unsigned short num;
		if(len != 2)
		{
			perror("Error receiving data from client");
			tryClose(sock);
			exit(errno);
		}
		/* split the two parts of the client message*/
		letter = buff & 0xC000;
		num = buff & 0x3FFF;
		/* if letter == 0 then there was some error in the client. reject the move*/
		if(letter == 0)
		{
			accepted = 0;
		}
		else
		{
			/* choose the choice based on letter, as per protocol specification*/
			switch(letter)
			{
			case 0x4000:
				choice = &a;
				break;
			case 0x8000:
				choice = &b;
				break;
			case 0xC000:
				choice = &c;
				break;
			}
			/* if out of range, reject*/
			if(num > *choice || num > 1000 || num <= 0)
			{
				accepted = 0;
			}
			else
			{
				/* if in range, remove from choice*/
				*choice -= num;
				/* test if user won. if yes, send win message and shutdown the sockets*/
				if(testWin(a,b,c))
				{
					sendState(sock, 0, accepted, 1, a, b, c);
					shutdownSocket(sock);
					break;
				}
			}
		}
		/* if the client didnt win, do a server turn*/
		doServerTurn(sock, &a, &b, &c);
		/* test if the server won. if yes, send message and shutdown the socket*/
		if(testWin(a,b,c))
		{
			sendState(sock, 0, accepted, 2, a, b, c);
			shutdownSocket(sock);
			break;
		}
		else
		{
			/* otherwise just send a status message*/
			sendState(sock, 0, accepted, 0, a, b, c);
		}
	}
	if(len != 0 && len != 2)
	{
		perror("Error receiving data from client");
	}
	tryClose(sock);
	exit(0);
}
int isInt(char *str)
{
	/* tests if a string is just numbers*/
	if(*str == 0)
		return 0;
	while(*str != 0)
	{
		if(!isdigit(*str))
			return 0;
		str++;
	}
	return 1;
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
int sendState(int sock,int initial, int accept, int winLose, int a, int b, int c)
{
	/* sets the type char bits, as per protocol specification*/
	unsigned char type =
			type = (initial << 7) + (accept << 6) + (winLose << 4);
	/* puts all 3 heap sizes in one uint as per protocol specification*/
	unsigned int nums =
			(a << 20) + (b<<10) + c;
	int len1 = sizeof(unsigned char), len2 = sizeof(unsigned int);
	/* send char*/
	if(sendall(sock, ((char*)&type), &len1) < 0)
	{
		perror("Error sending message");
		tryClose(sock);
		exit(errno);
	}
	/* send nums*/
	if(sendall(sock, (char*)&nums, &len2) < 0)
	{
		perror("Error sending message");
		tryClose(sock);
		exit(errno);
	}
	return 0;
}
int testWin(int a, int b, int c)
{
	/* tests if this is a winning setup*/
	return a == 0 && b == 0 && c == 0;
}
void doServerTurn(int sock, int* a, int* b, int* c)
{
	/* removes one from the highest heap, or the first lexicographically if there are ties*/
	if(*a >= *b && *a >= *c)
	{
		(*a)--;
	}
	else if(*b >= *a && *b >= *c)
	{
		(*b)--;
	}
	else
	{
		(*c)--;
	}
}

void tryClose(int sock)
{
	if(close(sock) <0)
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
	while(1)
	{
		/* read until the end of the stream */
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
