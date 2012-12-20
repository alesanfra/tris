#include "tris_lib.h"

//Dichiarazioni delle funzioni definite dopo il main()
void printHelp(int status);
void executeCommand(int socket, char status);
void replyToServer(int socket);
void playTurn(int socket, char* status, char* map);
void askWho(int socket);
void connectToUser(int socket, char* name);

int main(int argc, char* argv[])
{
	//dichiarazioni delle variabili
	struct sockaddr_in server_addr, myudp_addr;
	struct timeval timeout = {60,0};
	packet buffer_in, buffer_out;
	int server, udp, fdmax, ready;
	uint16_t UDPport;
	char name[33], map[9], status=IDLE;
	
	//lista dei descrittori da controllare con la select()
	fd_set masterset, readset;
	
	//descrittore dello STDIN
	const int des_stdin = fileno(stdin);
	
	//inizializzazione degli fd_set
	FD_ZERO(&masterset);
	FD_ZERO(&readset);
	
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
	if(connect(server, (SA *) &server_addr, sizeof(SA)) == -1)
	{
		perror("errore nella connessione al server");
		exit(EXIT_FAILURE);
	}
		
	//connessione avvenuta
	printf("\nConnessione al server %s:%s effettuata con successo\n\n", argv[1], argv[2]);
	
	//stampo i comandi disponibili
	printHelp(IDLE);
	
	//inserimento name
	printf("Inserisci il tuo name (max 32 caratteri): ");
	scanf("%s",name);
	flush();
	
	//inserimento della porta UDP di ascolto
	printf("Inserisci la porta UDP di ascolto: ");
	scanf("%hu",&UDPport);
	flush();
	
	//creazione del socket UDP
	if((udp = socket(AF_INET, SOCK_DGRAM, 0)) == -1){
		perror("Errore nella creazione del socket UDP");
		exit(EXIT_FAILURE);
	}
	
	//inizializzazione della struttura dati
	memset(&myudp_addr, 0, sizeof(struct sockaddr_in));
	myudp_addr.sin_family = AF_INET;
	myudp_addr.sin_port = htons(UDPport);
	myudp_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	
	//controllo che la porta UDP scelta sia disponibile
	while(bind(udp, (struct sockaddr*) &myudp_addr, sizeof(struct sockaddr_in)) == -1)
	{
		switch(errno)
		{
			case EADDRINUSE: //porta già in uso
				printf("La porta scelta è occupata, sceglierne un'altra: ");
				scanf("%hu", &UDPport);
				flush();
				myudp_addr.sin_port = htons(UDPport);
				break;
				
			case EACCES:
				printf("Non hai i permessi per quella porta, prova una porta superiore a 1023: ");
				scanf("%hu", &UDPport);
				flush();
				myudp_addr.sin_port = htons(UDPport);
				break;
				
			default:
				perror("errore non gestito nel bind del socket udp");
				exit(EXIT_FAILURE);
		}
	}
	
	//Invio al server il name e la porta UDP di ascolto
	buffer_out.type = SETUSER;
	buffer_out.length = strlen(name)+3; //lunghezza stringa + \0 + 2 byte porta
	buffer_out.payload = (char *) malloc(buffer_out.length*sizeof(char));
	*((int16_t *) buffer_out.payload) = htons(UDPport);
	strcpy(&(buffer_out.payload[2]),name);
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
		printf("Il nome \"%s\" non è disponibile, scegline un'altro (max 32 caratteri): ",name);
		memset(name,' ',33);
		scanf("%s", name);
		flush();
		buffer_out.length = strlen(name)+3; //lunghezza stringa + \0 + 2 byte porta
		free(buffer_out.payload);
		buffer_out.payload = (char *) malloc(buffer_out.length*sizeof(char));
		*((int16_t *) buffer_out.payload) = htons(UDPport);
		strcpy(&(buffer_out.payload[2]),name);
		sendPacket(server,&buffer_out,"Errore invio name e porta UDP");
		
		//Ricevo la conferma dal server
		if(recvPacket(server,&buffer_in,"Errore ricezione pacchetto dal client") < 1)
		{
			printf("Disconnesso dal server \n");
			exit(EXIT_FAILURE);
		}
	}
	
	//Dealloco il payload del pacchetto ricevuto e azzero la struttura
	free(buffer_in.payload);
	memset(&buffer_in,0,sizeof(buffer_in));
	
	/* Da questo punto in poi il client è pronto a giocare */
	
	//Setto i bit relativi ai descrittori da controllare con la select
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
		int ret = 0;
		
		if(status == IDLE)
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
		
		//Copio il masterset perché la select lo modifica
		readset = masterset;
		
		//eseguo la select()
		ret = select(fdmax+1, &readset, NULL, NULL, timer);
		if(ret == -1)
		{
			perror("Errore nell'esecuzione della select()");
			exit(EXIT_FAILURE);
		}
		else if(ret == 0)
		{
			//SE la select restituisce 0 il timer è scaduto
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
			memset(map,0,9);
		}
		
		//ciclo in cui scorro i descrittori e gestisco quelli pronti
		for(ready = 0; ready <= fdmax; ready++)
		{
			//controllo se ready è un descrittore pronto in lettura
			if(FD_ISSET(ready,&readset))
			{				
				//se il descrittore pronto lo STDIN eseguo il comando
				if(ready == des_stdin)
					executeCommand(server,status);
				
				//altrimenti ricevo un pacchetto dal server
				else if(ready == server) 
					replyToServer(ready,&status,&masterset,&fdmax,udp);
				
				//oppure ancora sono in una partita
				else if((status == MYTURN || status == HISTURN) && ready == udp)
					playTurn(ready,&status,map);
			}
		}
	}
	return 0;
}

//Stampa l'elenco dei comandi disponibili
void printHelp(int status)
{
	printf("\nSono disponibili i seguenti comandi:\n");
	printf(" * !help --> mostra l'elenco dei comandi disponibili\n");
	if(status == IDLE)
	{
		printf(" * !who --> mostra l'elenco dei client connessi al server\n");
		printf(" * !connect nome_client --> avvia una partita con l'utente nome_client\n");
	}
	printf(" * !disconnect --> disconnette il client dall'attuale partita\n");
	printf(" * !quit --> disconnette il client dal server\n");
	printf(" * !show_map --> mostra la mappa del gioco\n");
	printf(" * !hit num_cell --> marca la casella num_cell (valido quando e' il proprio turno)\n\n");
}

//esegue un comando
void executeCommand(int socket, char status)
{
	char command[12];
	
	//leggo dallo stdin la prima stringa
	scanf("%s",command);
	
	//per ogni comando riconosciuto eseguo le azioni corrispondenti
	if(strcmp(command,"!help") == 0)
	{
		printHelp(status);
	}
	else if(strcmp(command,"!who") == 0)
	{
		askWho(socket);
	}
	else if(strcmp(command,"!connect") == 0)
	{
		char* user = (char *) calloc(33,sizeof(char));
		scanf("%s",user);
		user[32] = '\0';
		user = realloc(user,strlen(user) + 1);
		connectToUser(socket,user);
	}
	else if(strcmp(command,"!quit") == 0)
	{
		quit();
	}
	else if(strcmp(command,"!disconnect") == 0)
	{
		
	}
	else if(strcmp(command,"!show_map") == 0)
	{
	
	}
	else if(strcmp(command,"!hit") == 0)
	{
		unsigned char num_cell = 0;
		scanf("%d",num_cell);
	}
	else
	{	//Comando non riconosciuto
		printf("Comando non riconosciuto!\n");
		printHelp(status);
	}
	
	flush();
}

void replyToServer(int socket)
{
	packet buffer;
	char answer = 0;
	
	if(recvPacket(socket,&buffer,"Errore ricezione numero giocatori") < 1)
	{
		printf("Disconnesso dal server\n");
		exit(EXIT_FAILURE);
	}

	if(buffer->type == PLAYREQ)
	{
		if(status != IDLE)
			return;
		
		printf("\n%s vuole inziare una partita con te. Accetti? (s/n)",buffer->payload);
		scanf("%c",&answer);
		fflush(stdin);
		
		if(answer == 's')
			buffer->type = MATCHACCEPTED;
		else
			buffer->type = MATCHREFUSED;
			
		sendPacket(socket,&buffer,"Risposta alla richiesta di gioco");
		
	//operazioni per l'avvio del gioco	
		
	}

}


void playTurn(int socket, char* status, char* map)
{
	packet buffer;
	
	if(recvPacket(socket,&buffer,"Errore ricezione messaggio dall'avversario") < 1)
		return;
	
	if(buffer.type == DISCONNECT)
	{
		printf("L'avversario si è disconnesso. Hai vinto la partita!");
		
		
		
	}
	
	if(buffer.type == HIT && *status == HISTURN && buffer.payload[0] > 0 && buffer.payload[0] < 10)
	{
		if(map[(int)buffer.payload[0]] == ' ')
			map[(int)buffer.payload[0]] = opponent_symbol;
		else
			printf("L'avversario ha cercato di marcare una casella già piena\n");
	}
	

}


void askWho(int socket)
{
	packet buffer;
	uint16_t num = 0;
	
	buffer.type = WHO;
	buffer.length = 0;
	buffer.payload = NULL;
	sendPacket(socket,&buffer,"Errore invio richiesta WHO");
	if(recvPacket(socket,&buffer,"Errore ricezione numero giocatori") < 1)
	{
		printf("Disconnesso dal server\n");
		exit(EXIT_FAILURE);
	}
	printf("Numero client ricevuti\n");
	fflush(stdout);
	num = *((uint16_t *) buffer.payload);
	printf("Numero giocatori connessi al server: %i\n",num);
	
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
}

void connectToUser(int socket, char* name)
{
	packet buffer;
	
	buffer.type = CONNECT;
	buffer.length = strlen(name)+1;
	buffer.payload = name;
	
	sendPacket(socket,&buffer,"Errore invio richiesta CONNECT");
	if(recvPacket(socket,&buffer,"Errore ricezione numero giocatori") < 1)
	{
		printf("Disconnesso dal server\n");
		exit(EXIT_FAILURE);
	}
	
	if(buffer.payload[0] == NOTFOUND)
	{
		printf("Impossibile connettersi a %s: utente inesistente.\n",name);
		return;
	}
	if(buffer.payload[0] == YOURSELF)
	{
		printf("Impossibile connettersi a %s: non puoi giocare con te stesso.\n",name);
		return;
	}
	if(buffer.payload[0] == REFUSE)
	{
		printf("Impossibile connettersi a %s: l'utente ha rifiutato la partita.\n",name);
		return;
	}
	if(buffer.payload[0] != ACCEPT)
	{
		printf("Impossibile connettersi a %s: errore del server.\n",name);
		return;
	}
}

void showMap(char status, char* map)
{
	char* line = "+---+---+---+\n\n";
	
	if(status != HISTURN && status != MYTURN)
	{
		printf("Non sei in una partita!\n");
		return;
	}
	
	for(i = 0; i < 3; i++)
	{
		printf("%s| ",line);
		for(j = 0; j < 3; j++)
			printf("%c |",&map[(i*3)+j]);
		printf("\n\n");
	}
	prinf("%s",line);
}

void quit(int socket)
{
	packet buffer;
	
	buffer->type = QUIT;
	buffer->length = 0;
	buffer->payload = NULL;
	sendPacket(socket,&buffer,"Errore nella disconnessione del server");
	printf("Disconnessione dal server in corso...\n");
	close(socket);
	exit(EXIT_SUCCESS);	
}
	
