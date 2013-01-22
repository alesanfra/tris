#include "tris_lib.h"

int connectToServer(client* opponent, char** username, char* ip, char* port);
void runCommand(int server, client* opponent, char* status, char map[]);
void replyToServer(int server, client* opponent, char* status, char map[]);
void playTurn(int server, client* opponent, char* status, char map[]);
void printHelp(int status);
void askWho(int socket);
void hitCell(unsigned char cell, char* map, char* status, client* opponent, int server);
int connectUDP(client* opponent, uint32_t ip, uint16_t port);
void disconnect(int server, client* opponent, char* status, char flag);
void printMap(char map[], char status);
bool checkMap(char* map);

//Lista dei descrittori da controllare con la select()
fd_set masterset;
int fdmax = 0;
struct timeval timeout = ONE_MINUTE;

int main(int argc, char* argv[])
{
	//dichiarazioni delle variabili
	client opponent;
	int server;
	const int des_stdin = fileno(stdin);
	char status=IDLE, map[10], *username = NULL;
	
	//controllo numero argomenti passati
	if(argc != 3)
	{
		printf("Usage: tris_client <host remoto> <porta>\n");
		exit(EXIT_FAILURE);
	}
	
	//Connessione al server e invio username e porta di ascolto
	server = connectToServer(&opponent,&username,argv[1],argv[2]);
	
	//stampo i comandi disponibili
	printHelp(IDLE);
	
	//Azzero la mappa di gioco
	memset(map,' ',10);
	
	//Setto i bit relativi ai descrittori da controllare con la select	
	FD_ZERO(&masterset);
	FD_SET(server, &masterset);
	FD_SET(des_stdin, &masterset);
	
	//Cerco il massimo descrittore da controllare
	if(server < des_stdin)
		fdmax = des_stdin;
	else
		fdmax = server;
	
	//ciclo in cui il client chiama la select ed esegue le azioni richieste
	for(;;)
	{
		struct timeval* timer;
		int ret = 0, ready = 0;
		fd_set readset;
		
		FD_ZERO(&readset);
		readset = masterset; //Copio il masterset perché la select lo modifica
		
		if(status == IDLE || status == WAIT)
		{
			//se il client non è in una partita stampo '>' e metto il timer a NULL
			printf("> ");
			timer = NULL;
		}
		else
		{
			//se il client non è in una partita stampo '#' e metto il timer a 60 sec
			printf("# ");
			timer = &timeout;
		}	
		fflush(stdout);
		
		//eseguo la select()
		ret = select(fdmax+1, &readset, NULL, NULL, timer);
		if(ret == -1)
		{
			perror("Errore nell'esecuzione della select()");
			exit(EXIT_FAILURE);
		}
		else if(ret == 0)
		{
			//Se la select restituisce 0 il timer è scaduto
			printf("\nSono passati 60 secondi!\n");
			
			//Stampo l'esito della partita
			if(status == MYTURN)
				printf("Hai perso!\n");
			else if(status == HISTURN)
				printf("Hai vinto!\n");
			else
				printf("Tempo scaduto!\n");
				
			//Disconnessione
			disconnect(server,&opponent,&status,DONTSIGNAL);
		}
		
		//ciclo in cui scorro i descrittori e gestisco quelli pronti
		for(ready = 0; ready <= fdmax; ready++)
		{
			//controllo se ready è un descrittore pronto in lettura
			if(FD_ISSET(ready,&readset))
			{				
				//se il descrittore pronto lo STDIN eseguo il comando
				if(ready == des_stdin)
					runCommand(server,&opponent,&status,map);
				
				//altrimenti ricevo un pacchetto dal server
				else if(ready == server)
					replyToServer(server,&opponent,&status,map);
				
				//oppure ancora sono in una partita
				else if(ready == opponent.socket)
					playTurn(server,&opponent,&status,map);
			}
		}
	}
	return 0;
}


/* Esegue le operazioni di inizializzazione del gioco: crea un socket TCP,
 * connette il socket al server remoto, chiede all'utente username e
 * porta UDP, li invia al server e associa la porta UDP ad un socket
 * UDP che salva nella struttura opponenent. Se tutto va a buon fine 
 * restituisce il socket TCP corrispondente al server */

int connectToServer(client* opponent, char** username, char* ip, char* port)
{
	packet buffer_in, buffer_out;
	struct sockaddr_in server_addr, opponent_addr;
	int server;
	uint16_t UDPport;
	
	//inizializzazione opponent
	opponent->name = NULL;

	//creazione del socket TCP
	if((server = socket(PF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("Errore nella creazione del socket TCP");
		exit(EXIT_FAILURE);
	}
	
	//Azzeramento della struttura dati sockaddr_in
	memset(&server_addr, 0, sizeof(struct sockaddr_in));
	
	//Inserimento in sockaddr_in dei paramentri
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(atoi(port));
	inet_pton(AF_INET, ip, &server_addr.sin_addr.s_addr);
	
	//connessione al server
	if(connect(server, (struct sockaddr *) &server_addr, sizeof(struct sockaddr)) == -1)
	{
		perror("errore nella connessione al server");
		exit(EXIT_FAILURE);
	}
	
	//connessione avvenuta
	printf("Connessione al server %s (porta %s) effettuata con successo\n", ip, port);
		
	//inserimento name
	*username = (char *) calloc(256,sizeof(char));
	printf("Inserisci il tuo nome: ");
	scanf("%s",*username);
	flush();
	
	//inserimento della porta UDP di ascolto
	printf("Inserisci la porta UDP di ascolto: ");
	scanf("%hu",&UDPport);
	flush();
	
	//creazione del socket UDP
	if((opponent->socket = socket(AF_INET, SOCK_DGRAM, 0)) == -1){
		perror("Errore nella creazione del socket UDP");
		exit(EXIT_FAILURE);
	}
	
	//inizializzazione della struttura dati
	memset(&opponent_addr, 0, sizeof(struct sockaddr_in));
	opponent_addr.sin_family = AF_INET;
	opponent_addr.sin_port = htons(UDPport);
	opponent_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	
	//controllo che la porta UDP scelta sia disponibile
	while(bind(opponent->socket, (struct sockaddr*) &opponent_addr, sizeof(struct sockaddr_in)) == -1)
	{
		switch(errno)
		{
			case EADDRINUSE: //porta già in uso
				printf("La porta scelta è occupata, sceglierne un'altra: ");
				scanf("%hu", &UDPport);
				flush();
				opponent_addr.sin_port = htons(UDPport);
				break;
				
			case EACCES:
				printf("Non hai i permessi per quella porta, prova una porta superiore a 1023: ");
				scanf("%hu", &UDPport);
				flush();
				opponent_addr.sin_port = htons(UDPport);
				break;
				
			default:
				perror("errore non gestito nel bind del socket udp");
				exit(EXIT_FAILURE);
		}
	}
	
	//Invio al server il name e la porta UDP di ascolto
	buffer_out.type = SETUSER;
	buffer_out.length = strlen(*username)+3; //lunghezza stringa + \0 + 2 byte porta
	buffer_out.payload = (char *) malloc(buffer_out.length*sizeof(char));
	*((int16_t *) buffer_out.payload) = htons(UDPport);
	strcpy(&(buffer_out.payload[2]),*username);
	sendPacket(server,&buffer_out,"Errore invio name e porta UDP");
	
	//Ricevo la conferma dal server
	if(recvPacket(server,&buffer_in,"Errore ricezione pacchetto dal client") < 1)
	{
		printf("Disconnesso dal server \n");
		exit(EXIT_FAILURE);
	}

	//Controllo che il name scelto sia libero
	while(buffer_in.type != NAMEFREE)
	{
		printf("Il nome \"%s\" non è disponibile, scegline un'altro: ",*username);
		memset(*username,' ',33);
		scanf("%s", *username);
		flush();
		buffer_out.length = strlen(*username)+3; //lunghezza stringa + \0 + 2 byte porta
		free(buffer_out.payload);
		buffer_out.payload = (char *) malloc(buffer_out.length*sizeof(char));
		*((int16_t *) buffer_out.payload) = htons(UDPport);
		strcpy(&(buffer_out.payload[2]),*username);
		sendPacket(server,&buffer_out,"Errore invio name e porta UDP");
		
		//Ricevo la conferma dal server
		if(recvPacket(server,&buffer_in,"Errore ricezione pacchetto dal client") < 1)
		{
			printf("Disconnesso dal server \n");
			exit(EXIT_FAILURE);
		}
	}
	
	//Libero lo spazio non usato da username
	*username = (char *) realloc(*username,strlen(*username)+1);
	
	//Dealloco il payload del pacchetto inviato e azzero le strutture
	free(buffer_out.payload);

	//Restituisco il socket del server
	return server;
}


/* Legge un comando dallo stdin ed esegue le operazioni corrispondenti.
 * In caso di comando sconosciuto stampa i comandi disponibili */

void runCommand(int server, client* opponent, char* status, char map[])
{
	char* command = calloc(255,sizeof(char));
	memset(command,'\0',255);
	
	//leggo dallo stdin la prima stringa
	scanf("%s",command);
	command[254] = '\0';
	
	//per ogni comando riconosciuto eseguo le azioni corrispondenti
	if(strcmp(command,"!help") == 0)
	{
		printHelp(*status);
	}
	else if(strcmp(command,"!who") == 0)
	{
		askWho(server);
	}
	else if(strcmp(command,"!connect") == 0)
	{
		packet buffer;
		if(*status == IDLE)
		{
			opponent->name = (char *) calloc(255,sizeof(char));
			scanf("%s",opponent->name);
			opponent->name[254] = '\0';
			opponent->name = realloc(opponent->name,strlen(opponent->name) + 1);
			buffer.type = CONNECT;
			buffer.length = strlen(opponent->name)+1;
			buffer.payload = opponent->name;
			sendPacket(server,&buffer,"Errore invio richiesta CONNECT");
			*status = WAIT;
			printf("Richiesta di connessione inviata a %s\n",opponent->name);
		}
		else
		{
			printf("Stai già giocando una partita!\n");
		}
		
	}
	else if(strcmp(command,"!quit") == 0)
	{
		packet buffer = {QUIT,0,NULL,NULL};
		
		if(*status == MYTURN || *status == HISTURN)
		{
			printf("Disconnessione avvenuta con successo: TI SEI ARRESO\n");
			disconnect(server,opponent,status,SIGNAL);
		}
		
		sendPacket(server,&buffer,"Errore nella disconnessione del server");
		printf("Disconnessione dal server in corso...\n");
		close(server);
		close(opponent->socket);
		exit(EXIT_SUCCESS);	
	}
	//Interrompo la partita e mi disconnetto dall'avversario
	else if(strcmp(command,"!disconnect") == 0)
	{
		if(*status == IDLE)
			printf("Non stai giocando nessuna partita!\n");
		else
		{
			if(*status != WAIT)
				printf("Disconnessione avvenuta con successo: TI SEI ARRESO\n");
			else
				printf("Richiesta di conessione a %s annullata\n",opponent->name);
			
			disconnect(server,opponent,status,SIGNAL);
		}
			
	}
	//Se sono in una partita stampo la mappa
	else if(strcmp(command,"!show_map") == 0)
	{
		if(*status != HISTURN && *status != MYTURN)
			printf("Non sei in una partita!\n");
		else 
			printMap(map,*status);
	}
	//Se sono in una partita colpisco una cella
	else if(strcmp(command,"!hit") == 0)
	{
		unsigned char num_cell = 0;
		scanf("%hhd",&num_cell);
		hitCell(num_cell,map,status,opponent,server);
	}
	//Comando non riconosciuto
	else
	{	
		printf("Comando non riconosciuto!\n");
		printHelp(*status);
	}
	//Dealloco il comando
	free(command);
	flush();
}


/* Riceve un pacchetto dal server ed esegue le operazioni corrispondenti
 * al tipo di richiesta. */

void replyToServer(int server, client* opponent, char* status, char map[])
{
	packet buffer;
	unsigned char action;
	
	if(recvPacket(server,&buffer,"Errore ricezione pacchetto dal server") < 1)
	{
		printf("Disconnesso dal server\n");
		exit(EXIT_FAILURE);
	}

	action = buffer.type;

	if(action == PLAYREQ)
	{
		char answer;
		//Se il client è occupato rifiuto la partita
		if(*status != IDLE)
			answer = 'n';
		else
		{	//altrimenti chiedo all'utente
			printf("\n%s vuole inziare una partita con te. Accetti? (s/n): ",buffer.payload);
			scanf("%c",&answer);
			flush();
		}

		if(answer == 's')
		{
			opponent->name = buffer.payload;//salvo il nome dell'avversario
			buffer.type = MATCHACCEPTED;
			//preparo la partita
			memset(map,' ',10);//azzero la mappa
			map[0] = 'O';//Metto come mio simbolo O
		}
		else
			buffer.type = MATCHREFUSED;
		
		sendPacket(server,&buffer,"Risposta alla richiesta di gioco");

		if(buffer.type == MATCHACCEPTED)
		{
			//Ricevo l'indirizzo
			if(recvPacket(server,&buffer,"Errore ricezione pacchetto dal server") < 1)
			{
				printf("Disconnesso dal server\n");
				exit(EXIT_FAILURE);
			}
			
			if(buffer.type == USERADDR)
			{	//Se il pacchetto ricevuto è USERADDR connetto il socket UDP
				client_addr opp;
				memcpy(&opp,buffer.payload,6);
				if(connectUDP(opponent,opp.ip,opp.port) != -1)
				{
					//Metto opponent fra i socket da controllare 
					FD_SET(opponent->socket,&masterset);
					if(opponent->socket > fdmax)
						fdmax = opponent->socket;
						
					*status = HISTURN;
					printf("Partita avviata con %s\n",opponent->name);
					printf("Il tuo simbolo è: %c\n",map[0]);
					printf("E' il turno di %s:\n",opponent->name);
					timeout = ONE_MINUTE;
				}
			}
			else
			{	//Altrimenti 
				printf("%s non esiste o ha ritirato la richiesta di connessione.\n",opponent->name);
				fflush(stdout);
				*status = IDLE;
			}
			
		}
		free(buffer.payload);
		return;
	}
	
	else if(action == NOTFOUND)
	{
		printf("\nImpossibile connettersi a %s: utente inesistente.\n",opponent->name);
		free(opponent->name);
		opponent->name = NULL;
		*status = IDLE;
		return;
	}
	else if(action == YOURSELF)
	{
		printf("\nImpossibile connettersi a %s: non puoi giocare con te stesso.\n",opponent->name);
		free(opponent->name);
		opponent->name = NULL;
		*status = IDLE;
		return;
	}
	else if(action == BUSY)
	{
		printf("\nImpossibile connettersi a %s: l'utente e' occupato.\n",opponent->name);
		free(opponent->name);
		opponent->name = NULL;
		*status = IDLE;
		return;
	}
	else if(action == PENDING_REQ)
	{
		printf("\nImpossibile connettersi a %s: l'utente ha gia' un'altra richiesta in sospeso\n",opponent->name);
		free(opponent->name);
		opponent->name = NULL;
		*status = IDLE;
		return;
	}
	else if(action == MATCHREFUSED)
	{
		printf("\nImpossibile connettersi a %s: l'utente ha rifiutato la partita.\n",opponent->name);
		free(opponent->name);
		opponent->name = NULL;
		*status = IDLE;
		return;
	}
	else if(action == MATCHACCEPTED)
	{
		printf("\n%s ha accettato la partita\n",opponent->name);
		//preparo la partita
		memset(map,' ',10);//azzero la mappa
		map[0] = 'X';//Metto come mio simbolo O
		*status = MYTURN;//Il primo turno spetta a me
		timeout = ONE_MINUTE;//Inizializzo il timer
		FD_SET(opponent->socket,&masterset);//Metto opponent fra i socket da controllare 
		if(opponent->socket > fdmax)
			fdmax = opponent->socket;
		
		if(recvPacket(server,&buffer,"Errore ricezione ip e porta") < 1)
		{
			printf("Disconnesso dal server\n");
			exit(EXIT_FAILURE);
		}
		
		if(buffer.type == USERADDR)
		{
			client_addr* address = (client_addr *) buffer.payload;
			connectUDP(opponent,address->ip,address->port);
		}
		else
		{
			printf("Errore ricezione IP e porta");
			exit(EXIT_FAILURE);
		}
		
		if(buffer.length > 0)
			free(buffer.payload);
			
		printf("Partita avviata con %s\n",opponent->name);
		printf("Il tuo simbolo è: %c\nE' il tuo turno:\n",map[0]);
	}

}


/* Riceve un pacchetto dall'avversario e esegue le azioni corrispondenti */

void playTurn(int server, client* opponent, char* status, char map[])
{
	packet buffer;
	int ret = 0;
	
	ret = recvPacket(opponent->socket,&buffer,"Errore ricezione messaggio dall'avversario");
	if(ret < 1)
		return;
	//~ else
		//~ printf("Ricevuti %i byte: %hhu - %hhu\n",ret,buffer.type,buffer.length);
	
	if(buffer.type == DISCONNECT)
	{
		printf("\nL'avversario si è disconnesso. Hai vinto la partita!\n");
		disconnect(server,opponent,status,DONTSIGNAL);
	}
	
	if(buffer.type == HIT && *status == HISTURN)
	{
		int num_caselle = 0, i = 0;
		char opponent_symbol = ' ';
		unsigned char cell = buffer.payload[0];
		
		if(map[0] == 'O')
			opponent_symbol = 'X';
		else
			opponent_symbol = 'O';
			
		//printf("L'avversario ha inviato la cella %hhu ed ha simbolo %c\n",cell,opponent_symbol);
			
		if(map[cell] == ' ')
		{
			map[cell] = opponent_symbol;
			printf("\nL'avversario ha segnato la casella %hhi\n",cell);
			printMap(map,*status);
			*status = MYTURN;
		}
		else
		{
			printf("\nL'avversario ha cercato di segnare una casella già piena\n");
			return;
		}
			
		//Conto le caselle segnate per vedere se la mappa è piena
		for(i=1; i<10; i++)
			if(map[i] == 'X' || map[i] == 'O')
				num_caselle++;
		
		//Controllo se la partita è finita
		if(checkMap(map))
		{
			printf("L'avversario ha vinto la partita!\n");
			disconnect(server,opponent,status,DONTSIGNAL);
		}
		else if(num_caselle == 9)
		{
			printf("Nessuna ulteriore mossa disponibile, partita terminata.\n");
			disconnect(server,opponent,status,DONTSIGNAL);
		}
		else
		{
			printf("E' il tuo turno:\n");
			timeout = ONE_MINUTE;
		}
	}
	
	//Dealloco il payload
	if(buffer.length > 0)
		free(buffer.payload);
}


/* Stampa l'elenco dei comandi disponibili */

void printHelp(int status)
{
	printf("\nSono disponibili i seguenti comandi:\n");
	printf(" * !help --> mostra l'elenco dei comandi disponibili\n");
	printf(" * !who --> mostra l'elenco dei client connessi al server\n");
	if(status == IDLE)
	{
		printf(" * !connect nome_client --> avvia una partita con l'utente nome_client\n");
	}
	if(status != IDLE)
	{
		printf(" * !disconnect --> disconnette il client dall'attuale partita\n");
		printf(" * !show_map --> mostra la mappa del gioco\n");
		printf(" * !hit num_cell --> marca la casella num_cell (valido quando e' il proprio turno)\n");
	}
	printf(" * !quit --> disconnette il client dal server\n\n");
}


/* Chiede al server la lista dei client connessi */

void askWho(int socket)
{
	packet buffer;
	uint16_t num = 0;
	
	buffer.type = WHO;
	buffer.length = 0;
	buffer.payload = NULL;
	sendPacket(socket,&buffer,"Errore invio richiesta WHO");
	
	//Ricevo numero giocatori connessi
	if(recvPacket(socket,&buffer,"Errore ricezione numero giocatori") < 1)
	{
		printf("Disconnesso dal server\n");
		exit(EXIT_FAILURE);
	}
	num = *((uint16_t *) buffer.payload);
	num = ntohs(num);
	printf("Numero giocatori connessi al server: %i\n",num);
	free(buffer.payload);
	
	//Ricevo la lista dei giocatori connessi
	for(;num > 0; num--)
	{
		if(recvPacket(socket,&buffer,"Errore ricezione lista giocatori") < 1)
		{
			printf("Disconnesso dal server\n");
			exit(EXIT_FAILURE);
		}
		printf("%s ",buffer.payload+1);
		
		if(buffer.payload[0] == IDLE)
			printf("(libero)\n");
		else
			printf("(occupato)\n");
			
		free(buffer.payload);
	}
	printf("\n");
}

/* Segna una casella durante una partita e la notifica all'avversario */

void hitCell(unsigned char cell, char* map, char* status, client* opponent, int server)
{
	packet buffer;
	int i = 0, num_caselle = 0, ret = 0;
	
	//Se non è il turno del client mostro un messaggio di errore
	if(*status == IDLE)
	{
		printf("Non sei in una partita!\n");
		return;
	}	
	if(*status != MYTURN)
	{
		printf("Non è il tuo turno, non puoi segnare una casella!\n");
		return;
	}
	
	//Se cell non è compreso tra 1 e 9 mostro un messaggio di errore
	if(cell < 1 || cell > 9)
	{
		printf("Il numero digitato non è una coordinata valida\n");
		return;
	}
	
	//Se la casella è già segnata mostro un messaggio di errore
	if(map[cell] == 'X' || map[cell] == 'O')
	{
		printf("La casella scelta è già segnata, sceglierne un altra!\n");
		return;
	}
	
	//Segno la casella
	map[cell] = map[0];
	printf("Hai segnato la casella: %hhd\n",cell);
	printMap(map,*status);
	
	//Invio all'avversario il numero [1-9] della casella segnata
	memset(&buffer,0,sizeof(buffer));
	buffer.type = HIT;
	buffer.length = 1;
	buffer.payload =  (char *) &cell;
	//printf("lunghezza %hhu, buffer c'è: %hhu\n",buffer.length,buffer.payload[0]);
	ret = sendPacket(opponent->socket,&buffer,"Errore invio coordinata");
	//printf("lunghezza %hhu, buffer c'è: %hhu, inviati %i\n",buffer.length,buffer.payload[0],ret);
	
	//Conto le caselle segnate per vedere se la mappa è piena
	for(i=1; i<10; i++)
		if(map[i] == 'X' || map[i] == 'O')
			num_caselle++;
	
	//Controllo se la partita è finita
	if(checkMap(map))
	{
		printf("Hai vinto la partita!\n");
		disconnect(server,opponent,status,DONTSIGNAL);
	}
	else if(num_caselle == 9)
	{
		printf("Nessuna ulteriore mossa disponibile, partita terminata.\n");
		disconnect(server,opponent,status,DONTSIGNAL);
	}
	else
	{
		*status = HISTURN;
		printf("E' il turno di %s:\n",opponent->name);
		timeout = ONE_MINUTE;
	}
}

/* Controlla se sulla mappa c'è una sequenza vincente */

bool checkMap(char* map)
{
	int i = 0;
	
	//controllo se la mossa è vincente o se la mappa è piena
	for(i = 0; i < 3; i++)
	{
		//controllo righe
		if((map[(i*3)+1] == 'X' || map[(i*3)+1] == 'O') && map[(i*3)+1] == map[(i*3)+2] && map[(i*3)+1] == map[(i*3)+3])
			return true;
		
		//controllo colonne
		if((map[i+1] == 'X' || map[i+1] == 'O') && map[i+1] == map[i+4] && map[i+1] == map[i+7])
			return true;
	}
	
	//controllo diagonale principale
	if((map[7] == 'X' || map[7] == 'O') && map[7] == map[5] && map[5] == map[3])
		return true;
		
	//controllo diagonale secondaria
	if((map[1] == 'X' || map[1] == 'O') && map[1] == map[5] && map[5] == map[9])
		return true;
	
	return false;
}


/* Connette il client all'avversario tramite socket udp */

int connectUDP(client* opponent, uint32_t ip, uint16_t port)
{
	struct sockaddr_in address;
	char ipstring[16];
	memset(&address,0,sizeof(address));
	address.sin_family = AF_INET;
	address.sin_port = port;
	address.sin_addr.s_addr = ip;
	inet_ntop(AF_INET,&address.sin_addr.s_addr,ipstring,16);
	port = ntohs(port);
	
	//Pulizia del socket
	cleanSocket(opponent->socket);
	
	//connessione al server
	if(connect(opponent->socket, (struct sockaddr *) &address, sizeof(struct sockaddr)) == -1)
	{
		perror("Errore nella connessione al peer");
		return -1;
	}
	else
		printf("\nConnessione a %s (%s:%hu) effettuata\n",opponent->name,ipstring,port);

	

	return 0;
}


/* disconnette il client dall'avversario e lo notfica al server */

void disconnect(int server, client* opponent, char* status, char flag)
{
	struct sockaddr_in address;
	
	//Invio DISCONNECT al peer
	packet buffer = {DISCONNECT,0,NULL,NULL};
	
	if(flag != DONTSIGNAL && *status != WAIT)
		sendPacket(opponent->socket,&buffer,"Errore disconnessione dal peer");
	
	//Dico al server che sono libero
	buffer.type = SETFREE;
	sendPacket(server,&buffer,"Errore invio stato al server");
	
	//Elimino il nome dell'avversario
	if(opponent->name != NULL)
	{
		free(opponent->name);
		opponent->name = NULL;
	}
	
	//Elimino il socket dal readset
	FD_CLR(opponent->socket,&masterset);
	if(opponent->socket == fdmax)
		for(; fdmax > 0; fdmax--)
			if(FD_ISSET(fdmax, &masterset))
				break;
	
	memset(&address,0,sizeof(address));	
	address.sin_family = AF_UNSPEC;
	
	//disconnessione dal peer
	if(connect(opponent->socket, (struct sockaddr *) &address, sizeof(struct sockaddr)) == -1)
	{
		perror("Errore nella disonnessione dal peer");
		exit(EXIT_FAILURE);
	}
	
	//metto lo status del client a libero (IDLE)
	*status = IDLE;
}


/* Stampa a video la mappa di gioco */

void printMap(char map[], char status)
{
	int i, j;
	char* line = "+ - + - + - +\n";
		
	printf("\n");
	for(i = 2; i >= 0; i--)
	{
		printf("%s",line);
		for(j = 1; j < 4; j++)
			printf("| %c ",map[(i*3)+j]);
		printf("|\n");
	}
	printf("%s\n",line);
}
