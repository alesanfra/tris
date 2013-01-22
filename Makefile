CC = gcc
CFLAGS = -Wall


all: tris_server tris_client udp_bomber

tris_server: tris_server.o tris_lib.o
	$(CC) $(CFLAGS) -o tris_server tris_server.o tris_lib.o
	
tris_client: tris_client.o tris_lib.o
	$(CC) $(CFLAGS) -o tris_client tris_client.o tris_lib.o
	
udp_bomber: udp_bomber.o tris_lib.o
	$(CC) $(CFLAGS) -o udp_bomber udp_bomber.o tris_lib.o
	
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<
	
clear_objects:
	rm -rf *.o

clear:
	rm -rf *.o tris_server tris_client udp_bomber
	
