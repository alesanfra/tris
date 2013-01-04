#include "tris_lib.h"

void runCommand(int server, int opponent, char* status, char map[]);
void replyToServer(int server, int opponent, char* status, char map[]);
void playTurn(int server, int opponent, char* status, char map[]);
void printHelp(int status);
void askWho(int socket);
void hitCell(unsigned char cell, char* map, char* status, int opponent);
int connectUDP(int socket, uint32_t ip, uint16_t port);
void disconnect(int server, int socket, char* status);

bool checkMap(char* map);

//Lista dei descrittori da controllare con la select()
fd_set masterset;
int fdmax = 0;

//Username dell'avversario
char* opponentname = NULL;

int main(int argc, char* argv[])
{
	//dichiarazioni delle variabili
	packet buffer_in, buffer_out;
	struct sockaddr_in server_addr, udp_addr;
	int server, opponent, ready;
	uint16_t UDPport;
	char status=IDLE, map[10];
	char* username = NULL;
	const int des_stdin = fileno(stdin);
	
	//controllo numero argomenti passati
	if(argc != 3)
	{
		printf("Usage: tris_client <host remoto> <porta>\n");
		exit(EXIT_FAILURE);
	}

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
	server_addr.sin_port = htons(atoi(argv[2]));
	inet_pton(AF_INET, argv[1], &server_addr.sin_addr.s_addr);
	
	
	//connessione al server
	if(connect(server, (struct sockaddr *) &server_addr, sizeof(struct sockaddr)) == -1)
	{
		perror("errore nella connessione al server");
		exit(EXIT_FAILURE);
	}
		
	//connessione avvenuta
	printf("\nConnessione al server %s:%s effettuata con successo\n\n", argv[1], argv[2]);
	
	//stampo i comandi disponibili
	printHelp(IDLE);
	
	//inserimento name
	username = (char *) calloc(256,sizeof(char));
	printf("Inserisci il tuo name: ");
	scanf("%s",username);
	flush();
	
	//inserimento della porta UDP di ascolto
	printf("Inserisci la porta UDP di ascolto: ");
	scanf("%hu",&UDPport);
	flush();
	
	//creazione del socket UDP
	if((opponent = socket(AF_INET, SOCK_DGRAM, 0)) == -1){
		perror("Errore nella creazione del socket UDP");
		exit(EXIT_FAILURE);
	}
	
	//inizializzazione della struttura dati
	memset(&udp_addr, 0, sizeof(struct sockaddr_in));
	udp_addr.sin_family = AF_INET;
	udp_addr.sin_port = htons(UDPport);
	udp_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	
	//controllo che la porta UDP scelta sia disponibile
	while(bind(opponent, (struct sockaddr*) &udp_addr, sizeof(struct sockaddr_in)) == -1)
	{
		switch(errno)
		{
			case EADDRINUSE: //porta già in uso
				printf("La porta scelta è occupata, sceglierne un'altra: ");
				scanf("%hu", &UDPport);
				flush();
				udp_addr.sin_port = htons(UDPport);
				break;
				
			case EACCES:
				printf("Non hai i permessi per quella porta, prova una porta superiore a 1023: ");
				scanf("%hu", &UDPport);
				flush();
				udp_addr.sin_port = htons(UDPport);
				break;
				
			default:
				perror("errore non gestito nel bind del socket udp");
				exit(EXIT_FAILURE);
		}
	}
	
	//Invio al server il name e la porta UDP di ascolto
	buffer_out.type = SETUSER;
	buffer_out.length = strlen(username)+3; //lunghezza stringa + \0 + 2 byte porta
	buffer_out.payload = (char *) malloc(buffer_out.length*sizeof(char));
	*((int16_t *) buffer_out.payload) = htons(UDPport);
	strcpy(&(buffer_out.payload[2]),username);
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
		printf("Il nome \"%s\" non è disponibile, scegline un'altro: ",username);
		memset(username,' ',33);
		scanf("%s", username);
		flush();
		buffer_out.length = strlen(username)+3; //lunghezza stringa + \0 + 2 byte porta
		free(buffer_out.payload);
		buffer_out.payload = (char *) malloc(buffer_out.length*sizeof(char));
		*((int16_t *) buffer_out.payload) = htons(UDPport);
		strcpy(&(buffer_out.payload[2]),username);
		sendPacket(server,&buffer_out,"Errore invio name e porta UDP");
		
		//Ricevo la conferma dal server
		if(recvPacket(server,&buffer_in,"Errore ricezione pacchetto dal client") < 1)
		{
			printf("Disconnesso dal server \n");
			exit(EXIT_FAILURE);
		}
	}
	
	//Libero lo spazo non usato da username
	username = (char *) realloc(username,strlen(username)+1);
	
	//Dealloco il payload del pacchetto inviato e azzero le strutture
	free(buffer_out.payload);
	memset(&buffer_out,0,sizeof(buffer_out));
	memset(&buffer_in,0,sizeof(buffer_in));
	
	/* Da questo punto in poi il client è pronto a giocare */
	printf("\n");
	
	//Azzero la mappa di gioco
	memset(map,' ',10);
	
	//Setto i bit relativi ai descrittori da controllare con la select	
	FD_ZERO(&masterset);
	FD_SET(server, &masterset);
	FD_SET(des_stdin, &masterset);
	
	//~ //Cerco il massimo descrittore da controllare
	if(server < des_stdin)
		fdmax = des_stdin;
	else
		fdmax = server;
	
	//ciclo in cui il client chiama la select ed esegue le azioni richieste
	for(;;)
	{
		struct timeval timeout = {60,0};
		struct timeval* timer;
		int ret = 0;
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
			printf("Sono passati 60 secondi!\n");
			
			//Stampo l'esito della partita
			if(status == MYTURN)
				printf("Hai perso!\n");
			else if(status == HISTURN)
				printf("Hai vinto!\n");
			else
				printf("Tempo scaduto!\n");
				
			//Rimetto lo stato a IDLE
			status = IDLE;
			
			//Azzero la mappa
			memset(map,' ',10);
			
			//Azzero il nome dell'avversario
			if(opponentname != NULL)
				free(opponentname);
		}
		
		//ciclo in cui scorro i descrittori e gestisco quelli pronti
		for(ready = 0; ready <= fdmax; ready++)
		{
			//controllo se ready è un descrittore pronto in lettura
			if(FD_ISSET(ready,&readset))
			{				
				//se il descrittore pronto lo STDIN eseguo il comando
				if(ready == des_stdin)
					runCommand(server,opponent,&status,map);
				
				//altrimenti ricevo un pacchetto dal server
				else if(ready == server)
					replyToServer(server,opponent,&status,map);
				
				//oppure ancora sono in una partita
				else if(ready == opponent)
					playTurn(server,opponent,&status,map);
			}
		}
	}
	return 0;
}

//Legge un comando dallo stdin e lo esegue
void runCommand(int server, int opponent, char* status, char map[])
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
			opponentname = (char *) calloc(255,sizeof(char));
			scanf("%s",opponentname);
			opponentname[254] = '\0';
			opponentname = realloc(opponentname,strlen(opponentname) + 1);
			buffer.type = CONNECT;
			buffer.length = strlen(opponentname)+1;
			buffer.payload = opponentname;
			sendPacket(server,&buffer,"Errore invio richiesta CONNECT");
			*status = WAIT;
		}
		else
		{
			printf("Stai già giocando una partita!");
		}
		
	}
	else if(strcmp(command,"!quit") == 0)
	{
		packet buffer = {QUIT,0,NULL,NULL};
		sendPacket(server,&buffer,"Errore nella disconnessione del server");
		printf("Disconnessione dal server in corso...\n");
		close(server);
		close(opponent);
		exit(EXIT_SUCCESS);	
	}
	//Interrompo la partita e mi disconnetto dall'avversario
	else if(strcmp(command,"!disconnect") == 0)
	{
		if(*status != MYTURN && *status != HISTURN)
			printf("Non stai giocando nessuna partita!");
		else
			disconnect(server,opponent,status);
			
	}
	//Se sono in una partita stampo la mappa
	else if(strcmp(command,"!show_map") == 0)
	{
		int i, j;						
		if(*status != HISTURN && *status != MYTURN)
			printf("Non sei in una partita!\n");
		else {
			printf("\n");
			for(i = 2; i >= 0; i--) {
				for(j = 1; j < 4; j++)
					printf("| %c ",map[(i*3)+j]);
				printf("|\n");
			}
			printf("\n");
		}
	}
	//Se sono in una partita colpisco una cella
	else if(strcmp(command,"!hit") == 0)
	{
		unsigned char num_cell = 0;
		scanf("%hhd",&num_cell);
		hitCell(num_cell,map,status,opponent);
	}
	//Comando non riconosciuto
	else
	{	
		printf("Comando non riconosciuto!\n");
		printHelp(*status);
	}
	//Dealloco il comando
	free(command);
	fflush(stdin);
}


//Riceve un pacchetto dal server e gestisce la richiesta
void replyToServer(int server, int opponent, char* status, char map[])
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
			fflush(stdin);
		}

		if(answer == 's')
		{
			opponentname = buffer.payload;//salvo il nome dell'avversario
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
					*status = HISTURN;
					FD_SET(opponent,&masterset);//Metto opponent fra i socket da controllare 
					if(opponent > fdmax)
						fdmax = opponent;
				}
			}
			else
			{	//Altrimenti 
				printf("%s non esiste o non e' più disponibile",opponentname);
				fflush(stdout);
				free(opponentname);
				*status = IDLE;
			}
			
		}
		free(buffer.payload);
		return;
	}
	else if(action == NOTFOUND)
	{
		printf("Impossibile connettersi a %s: utente inesistente.\n",opponentname);
		free(opponentname);
		*status = IDLE;
		return;
	}
	else if(action == YOURSELF)
	{
		printf("Impossibile connettersi a %s: non puoi giocare con te stesso.\n",opponentname);
		free(opponentname);
		*status = IDLE;
		return;
	}
	else if(action == MATCHREFUSED)
	{
		printf("\nImpossibile connettersi a %s: l'utente ha rifiutato la partita.\n",opponentname);
		free(opponentname);
		*status = IDLE;
		return;
	}
	else if(action == MATCHACCEPTED)
	{
		printf("\n%s ha accettato la partita\n",opponentname);
		//preparo la partita
		memset(map,' ',10);//azzero la mappa
		map[0] = 'X';//Metto come mio simbolo O
		*status = MYTURN;//Il primo turno spetta a me
		FD_SET(opponent,&masterset);//Metto opponent fra i socket da controllare 
		if(opponent > fdmax)
			fdmax = opponent;
		
		if(recvPacket(server,&buffer,"Errore ricezione ip e porta") < 1)
		{
			printf("Disconnesso dal server\n");
			exit(EXIT_FAILURE);
		}
		
		if(buffer.type == USERADDR)
		{
			connectUDP(opponent,*((uint32_t *) buffer.payload),*((uint16_t *) &buffer.payload[4]));
		}
		else
		{
			printf("Errore ricezione IP e porta");
			exit(EXIT_FAILURE);
		}
		
		if(buffer.length > 0)
			free(buffer.payload);
	}

}


void playTurn(int server, int opponent, char* status, char map[])
{
	packet buffer;
	
	if(recvPacket(opponent,&buffer,"Errore ricezione messaggio dall'avversario") < 1)
		return;
	
	if(buffer.type == DISCONNECT)
	{
		printf("\nL'avversario si è disconnesso. Hai vinto la partita!");
		disconnect(server,opponent,status);
	}
	
	
	
	if(buffer.type == HIT && *status == HISTURN)
	{
		char opponent_symbol = ' ';
		unsigned char cell = buffer.payload[0];
		
		if(map[0] == 'O')
			opponent_symbol = 'X';
		else
			opponent_symbol = 'O';
		
		if(map[cell] == ' ')
		{
			map[cell] = opponent_symbol;
			printf("L'avversario ha marcato la casella %hhi\n",cell);
		}
		else
			printf("L'avversario ha cercato di marcare una casella già piena\n");
			
		if(checkMap(map))
		{
			printf("L'avversario ha vinto la partita");
			memset(map,' ',10);
			disconnect(server,opponent,status);
		}
	}
	
	//Dealloco il payload
	free(buffer.payload);
}


//Stampa l'elenco dei comandi disponibili
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


//Chiede al server la lista dei client connessi
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
		printf("%s\n",buffer.payload);
		free(buffer.payload);
	}
	printf("\n");
}

//Segna una casella durante una partita
void hitCell(unsigned char cell, char* map, char* status, int opponent)
{
	packet buffer;
	int i = 0, num_caselle = 0;
	
	//Se non è il turno del client mostro un messaggio di errore
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
	
	//Invio all'avversario il numero [1-9] della casella segnata
	buffer.type = HIT;
	buffer.length = 1;
	buffer.payload = (char *) &cell;
	sendPacket(opponent,&buffer,"Errore invio coordinata");
	
	//Conto le caselle segnate per vedere se la mappa è piena
	for(i=1; i<10; i++)
		if(map[i] == 'X' || map[i] == 'O')
			num_caselle++;
	
	//Controllo se la partita è finita
	if(checkMap(map) || num_caselle == 9)
	{
		if(num_caselle == 9)
			printf("Nessuna ulteriore mossa disponibile, partita terminata.");
		else
			printf("Hai vinto la partita!\n");
		
		//Metto lo status a IDLE
		*status = IDLE;
		
		//azzero la mappa e il simbolo
		memset(map,' ',10);
				
		//tolgo il readset
		FD_CLR(opponent,&masterset);
		
		if(opponent == fdmax)
			for(; fdmax > 0; fdmax--)
				if(FD_ISSET(fdmax,&masterset))
					break;			
	}
	else
		*status = HISTURN;
}

//Controlla se sulla mappa c'è una sequenza vincente
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


int connectUDP(int socket, uint32_t ip, uint16_t port)
{
	char ipstring[16];
	struct sockaddr_in address;
	memset(&address,0,sizeof(address));
	address.sin_family = AF_INET;
	address.sin_port = port;
	address.sin_addr.s_addr = ip;
	inet_ntop(AF_INET,&address.sin_addr.s_addr,ipstring,16);
	
	//connessione al server
	if(connect(socket, (struct sockaddr *) &address, sizeof(struct sockaddr)) == -1)
	{
		perror("Errore nella connessione al peer");
		return -1;
	}
	else
		printf("\nConnessione a %s effettuata: %s : %hu\n",opponentname,ipstring,port);

	return 0;
}

void disconnect(int server, int socket, char* status)
{
	struct sockaddr_in address;
	
	//Invio DISCONNECT al peer
	packet buffer = {DISCONNECT,0,NULL,NULL};
	sendPacket(socket,&buffer,"Errore disconnessione dal peer");
	
	//Dico al server che sono libero
	buffer.type = SETFREE;
	sendPacket(server,&buffer,"Errore invio stato al server");
	
	free(opponentname);
	FD_CLR(socket,&masterset);
	if(socket == fdmax)
		for(; fdmax > 0; fdmax--)
			if(FD_ISSET(fdmax, &masterset))
				break;
	
	memset(&address,0,sizeof(address));	
	address.sin_family = AF_UNSPEC;
	
	//disconnessione dal peer
	if(connect(socket, (struct sockaddr *) &address, sizeof(struct sockaddr)) == -1)
	{
		perror("Errore nella disonnessione dal peer");
		exit(EXIT_FAILURE);
	}
	
	//metto lo status del client a libero (IDLE)
	*status = IDLE;
}

