#include "tris_lib.h"

//Dichiarazioni delle funzioni definite dopo il main()
void printHelp(int status);
int readCommand();
bool executeCommand(int socket, char status);
void replyToServer(int socket);
void playTurn(int socket, char status);
void askWho(int socket);

int main(int argc, char* argv[])
{
	//dichiarazioni delle variabili
	struct sockaddr_in server_addr, opponent_addr;
	struct timeval timeout = {60,0};
	packet buffer_in, buffer_out;
	int server, udp, fdmax, ready;
	uint16_t UDPport;
	char name[33], status=IDLE;
	
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
	memset(&opponent_addr, 0, sizeof(struct sockaddr_in));
	opponent_addr.sin_family = AF_INET;
	opponent_addr.sin_port = htons(UDPport);
	opponent_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	
	//controllo che la porta UDP scelta sia disponibile
	while(bind(udp, (struct sockaddr*) &opponent_addr, sizeof(struct sockaddr_in)) == -1)
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
	buffer_out.length = strlen(name)+3; //lunghezza stringa + \0 + 2 byte porta
	buffer_out.payload = (char *) malloc(buffer_out.length*sizeof(char));
	*((int16_t *) buffer_out.payload) = htons(UDPport);
	strcpy(&(buffer_out.payload[2]),name);
	sendPacket(server,&buffer_out,"Errore invio name e porta UDP");
	
	//Ricevo la conferma dal server
	recvPacket(server,&buffer_in,"Errore ricezione controllo name");

	//Controllo che il name scelto sia libero
	while(buffer_in.payload[0] != true)
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
		free(buffer_in.payload);
		recvPacket(server,&buffer_in,"Errore ricezione controllo name");
	}
	
	//Dealloco il payload del pacchetto ricevuto e azzero la struttura
	free(buffer_in.payload);
	memset(&buffer_in,0,sizeof(buffer_in));
	
	/*Da questo punto in poi il client è pronto a giocare*/
	
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
		if(select(fdmax+1, &readset, NULL, NULL, timer) == -1)
		{
			perror("Errore nell'esecuzione della select()");
			exit(EXIT_FAILURE);
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
					replyToServer(ready);
				
				//oppure ancora sono in una partita
				else if(status == MYTURN || status == HISTURN)
					playTurn(ready,status);
			}
		} //fine ciclo in cui scorro i descrittori
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

//legge un comando dallo STDIN e ne restituisce il codice numerico
int readCommand()
{
	char command[12];
	
	//leggo dallo stdin la prima stringa
	scanf("%s",command);
	flush();
	
	//ricavo il codice numerico associato
	
	if(strcmp(command,"!help") == 0)
		return HELP;
		
	else if(strcmp(command,"!who") == 0)
		return WHO;
	
	else if(strcmp(command,"!quit") == 0)
		return QUIT;
	
	else if(strcmp(command,"!connect") == 0)
		return CONNECT;
		
	else if(strcmp(command,"!disconnect") == 0)
		return DISCONNECT;
	
	else if(strcmp(command,"!show_map") == 0)
		return SHOWMAP;
	
	else
		return -1;
}

//esegue un comando
bool executeCommand(int socket, char status)
{
	char* user;
	int cmd = readCommand();
	int length = 0;
	
	//Comando non riconosciuto
	if(cmd == -1)
	{
		printf("Comando non riconosciuto!\n");
		printHelp(status);
		return false;
	}
	
	//se il comando è CONNECT prelevo dallo stdin il nome 
	if(cmd == CONNECT)
	{
		user = (char *) calloc(33,sizeof(char));
		scanf("%s",user);
		flush();
		length = strlen(user) + 1;
		user = realloc(user,length);
	}
	
	switch(cmd)
	{
		case HELP:
			printHelp(status);
			break;
			
		case WHO:
			askWho(socket);
			break;
			
		case CONNECT:
			//connectToUser(char* name);
			break;
			
		case DISCONNECT:
			break;
			
		case QUIT:
			break;
		
		case SHOWMAP:
			break;
			
		case HIT:
			break;
			
		default:
			return false;
	}
	
	return true;
}

void replyToServer(int socket)
{
	
}

void playTurn(int socket,char status)
{
	

}

void askWho(int socket)
{
	packet buffer;
	uint16_t num = 0;
	
	buffer.type = WHO;
	buffer.length = 0;
	buffer.payload = NULL;
	sendPacket(socket,&buffer,"Errore invio richiesta WHO");
	printf("Richiesta inviata\n");
	fflush(stdout);
	recvPacket(socket,&buffer,"Errore ricezione numero giocatori");
	printf("Numero client ricevuti\n");
	fflush(stdout);
	num = *((uint16_t *) buffer.payload);
	printf("Numero giocatori connessi al server: %i\n",num);
	
	for(;num > 0; num--)
	{
		recvPacket(socket,&buffer,"Errore ricezione lista giocatori");
		printf("%s\n",buffer.payload);
		free(buffer.payload);
	}	
}

