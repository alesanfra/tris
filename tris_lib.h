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

typedef char bool;

#define SA struct sockaddr

//Tipi dell'header inviati dai client
#define SETUSER 10 //invia name e porta
#define WHO 11 //chiede gli utenti connessi
#define GETPORT 12 //chiede la porta UDP di ascolto di un utente
#define SETBUSY 13 //comunica al server che l'utente è occupato
#define SETFREE 14 //comunica al server che l'utente è libero
#define CONNECT 15 //chiede al server di connettersi con un utente
#define DISCONNECT 17 //chiude la partita con l'avverario
#define QUIT 18 //chiude la connessione con il server
#define HIT 19 //colpisce la casella indicata

//Tipi dell'header inviati dal server
#define NOTVALID 0
#define REPLYUSER 20 //risposta con un booleano
#define USERLIST 21 //invio di un player nel formato <port><name>
#define PLAYREQ 22
#define USERPORT 23
#define USERSTATUS 24

//Costanti usate all'interno del client
#define HELP 30
#define SHOWMAP 31

//Booleani
#define true 1
#define false 0

//Status del client
#define IDLE 41
#define WAIT 42
#define MYTURN 43
#define HISTURN 44

//Risposte alla CONNECT
#define NOTFOUND 0
#define YOURSELF 1
#define REFUSE 2
#define ACCEPT 3

//Strutture dati

typedef struct packet_str
{
	unsigned char type; //tipo del payload
	unsigned char length; //lunghezza del payload
	char* payload;
	struct packet_str* next;
} packet;

typedef struct player_str
{
	int socket;
	int opponent;
	char* name;
	unsigned char status;
	uint16_t UDPport;
	struct sockaddr_in address;
	packet* tail;
	struct player_str* next;
} player;

//Dichiarazioni delle funzioni definite in tris_lib.c

void flush();
void sendPacket(int socket, packet* buffer, const char* error_message);
int recvPacket(int socket, packet* buffer, const char* error_message);
