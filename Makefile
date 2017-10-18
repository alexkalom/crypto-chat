default: client server

client: client.c client-server-common.o
	gcc -o client client.c client-server-common.o

server: server.c client-server-common.o
	gcc -o server server.c client-server-common.o

client-server-common.o: client-server-common.c client-server-common.h
	gcc -o client-server-common.o -c client-server-common.c

clean: server client *.o