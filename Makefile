CC = gcc
CFLAGS = -Wall


all: tris_server tris_client

tris_server: tris_server.o tris_lib.o
	$(CC) $(CFLAGS) -o tris_server tris_server.o tris_lib.o
	
tris_client: tris_client.o tris_lib.o
	$(CC) $(CFLAGS) -o tris_client tris_client.o tris_lib.o
	
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<
	
clean_objects:
	rm -rf *.o
	
clear: clean

clean:
	rm -rf *.o tris_server tris_client

	
