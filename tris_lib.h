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

//Tipi dell'header inviati dai client
#define SETUSER 10 //invia name e porta
#define WHO 11 //chiede gli utenti connessi
#define GETPORT 12 //chiede la porta UDP di ascolto di un utente
#define SETFREE 14 //comunica al server che l'utente è libero
#define CONNECT 15 //chiede al server di connettersi con un utente
#define DISCONNECT 17 //chiude la partita con l'avverario
#define QUIT 18 //chiude la connessione con il server
#define HIT 19 //colpisce la casella indicata

//Tipi dell'header inviati dal server
#define NOTVALID 20
#define NAMEFREE 21
#define NAMEBUSY 22
#define USERLIST 23 //invio di un player nel formato <port><name>
#define PLAYREQ 24
#define MATCHACCEPTED 25
#define MATCHREFUSED 26
#define USERADDR 27
#define REPLYWHO 28

//Status del client
#define IDLE 41
#define WAIT 42
#define MYTURN 43
#define HISTURN 44
#define FREE -2

//Risposte alla CONNECT
#define NOTFOUND 60
#define YOURSELF 61
#define REFUSE 62
#define ACCEPT 63

//Booleani
typedef char bool;
#define true 1
#define false 0

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
	uint16_t UDPport;
	struct sockaddr_in address;
	packet* tail;
	struct player_str* next;
} player;

typedef struct client_addr_str
{
	uint32_t ip;
	uint16_t port;
} client_addr;

//Dichiarazioni delle funzioni definite in tris_lib.c

void flush();
void sendPacket(int socket, packet* buffer, const char* error_message);
int recvPacket(int socket, packet* buffer, const char* error_message);
