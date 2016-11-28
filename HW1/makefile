all: mail_client mail-server 

clean:
	-rm mail_client.o mail_server.o mail_common.o mail_client mail_server

mail_client: mail_client.o mail_common.o
	gcc -o mail_client mail_client.o mail_common.o -lm -pedantic-errors -Wall -g

mail_client.o: mail_client.c mail_common.h
	gcc -pedantic-errors -c -Wall -g -lm mail_client.c

mail_server: mail_server.o mail_common.o
	gcc -o mail_server mail_server.o mail_common.o -lm -pedantic-errors -Wall -g

mail_server.o: mail_server.c mail_common.h
	gcc -pedantic-errors -c -Wall -g -lm mail_server.c

mail_common.o: mail_common.c mail_common.h
	gcc -pedantic-errors -c -Wall -g -lm mail_common.c