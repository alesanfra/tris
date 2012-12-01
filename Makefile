CC = gcc
CFLAGS = -Wall -g


all: server client

client: client.o
	$(CC) -o client client.o
	
server: server.o
	$(CC) -o server server.o

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

clear:
	rm -rf *.o server client
	
