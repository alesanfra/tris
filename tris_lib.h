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
#define CHAT 12 //invia un messaggio di chat
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
#define BUSY 45
#define PENDING_REQ 46

//Risposte alla CONNECT
#define NOTFOUND 60
#define YOURSELF 61
#define REFUSE 62
#define ACCEPT 63
#define UNAVAILABLE 64

//Flag
#define SIGNAL 70
#define DONTSIGNAL 71
#define KILL 72
#define KEEP_ALIVE 73

//timer
#define ONE_MINUTE (struct timeval){60,0}

//Booleani
typedef char bool;
#define true 1
#define false 0

//Strutture dati
typedef struct _packet
{
	unsigned char type; //Tipo del payload
	unsigned char length; //Lunghezza del payload
	char* payload;
	struct _packet* next;
} packet;

typedef struct _player
{
	char* name; //Nome del giocatore
	int socket; //Socket del giocatore
	struct _packet* packets_tail; //Coda di pacchetti in attesa
	struct _player* opponent; //Puntatore all'avversario
	struct _player* next;
	struct sockaddr_in address; //Indirizzo del giocatore
	uint16_t UDPport; //Porta UDP
	char status; //Stato del client
} player;

typedef struct
{
	char* name;
	int socket;
	struct sockaddr_in address;
} client;

typedef struct
{
	uint32_t ip;
	uint16_t port;
} client_addr;


//Dichiarazioni delle funzioni definite in tris_lib.c

void flush();
void cleanSocket(int socket);
void sendPacket(int socket, packet* buffer, const char* error_message);
int recvPacket(int socket, packet* buffer, char flag, const char* error_message);
int recvPacketFrom(client* peer, packet* buffer, const char* error_message);
char* readLine(int max);
