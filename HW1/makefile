all: mail_client mail-server 

clean:
	-rm mail_client.o mail_server.o mail_client mail_server

mail_client: mail_client.o
	gcc -o mail_client mail_client.o -lm -pedantic-errors -Wall -g

nim.o: nim.c
	gcc -pedantic-errors -c -Wall -g -lm nim.c

mail_server: mail_server.o
	gcc -o mail_server mail_server.o -Wall -lm -pedantic-errors -g

mail_server.o: mail_server.c
	gcc -pedantic-errors -c -Wall -g -lm mail_server.c