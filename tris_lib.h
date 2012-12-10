#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>
#include <netinet/in.h>

#define SA struct sockaddr
#define packet struct packet_str
#define player struct player_str

//Tipi dell'header inviati dai client
#define SETUSER 10 //invia nickname e porta
#define WHO 11 //chiede gli utenti connessi
#define GETPORT 12 //chiede la porta UDP di ascolto di un utente
#define SETBUSY 13 //comunica al server che l'utente è occupato
#define SETFREE 14 //comunica al server che l'utente è libero
#define DISCONNECT 15 //chiude la partita con l'avverario
#define QUIT 16 //chiude la connessione con il server

//Tipi dell'header inviati dal server
#define REPLYUSER 20
#define USERLIST 21
#define USERNAME 22
#define USERPORT 23
#define USERSTATUS 24

//Booleani
#define true 1
#define false 0

//Status del client
#define IDLE 1
#define WAIT 2
#define MYTURN 3
#define HISTURN 4


//Strutture dati

struct packet_str
{
	unsigned char type; //tipo del payload
	unsigned char length; //lunghezza del payload
	char* payload;
};

struct player_str
{
	char* name;
	int16_t UDPport;
	struct sockaddr_in address;
	struct player_str *next;
};

//Dichiarazioni delle funzioni definite in tris_lib.c

void flush();
void sendPacket(int socket, packet* buffer, const char* message);
packet recvPacket(int socket, const char* message);
