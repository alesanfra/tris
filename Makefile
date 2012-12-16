CC = gcc
CFLAGS = -Wall -g


all: tris_server tris_client clear_objects

tris_server: tris_server.o tris_lib.o
	$(CC) $(CFLAGS) -o tris_server tris_server.o tris_lib.o
	
tris_client: tris_client.o tris_lib.o
	$(CC) $(CFLAGS) -o tris_client tris_client.o tris_lib.o
	
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<
	
clear_objects:
	rm -rf *.o

clear:
	rm -rf *.o tris_server tris_client
	
