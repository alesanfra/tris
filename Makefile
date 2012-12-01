CC = gcc
CFLAGS = -Wall -g


all: tris_server tris_client

tris_client: tris_client.o
	$(CC) -o tris_client tris_client.o
	
tris_server: tris_server.o
	$(CC) -o tris_server tris_server.o

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

clear:
	rm -rf *.o tris_server tris_client
	
