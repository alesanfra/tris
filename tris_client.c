#include "tris_lib.h"

//Dichiarazioni delle funzioni definite dopo il main()
void printHelp(int match);

int main(int argc, char* argv[])
{
	//dichiarazioni delle variabili
	struct sockaddr_in server_addr, opponent_addr;
	struct timeval timeout = {60,0};
	packet buffer, server_msg;
	int server, udp, fdmax, ready;
	uint16_t UDPport;
	char name[33], status=IDLE, cmd = 0;
	
	//lista dei descrittori da controllare con la select()
	fd_set masterset, readset;
	
	//descrittore dello STDIN
	const int des_stdin = fileno(stdin);
	
	//inizializzazione degli fd_set
	FD_ZERO(&masterset);
	FD_ZERO(&readset);
	
	//controllo numero argomenti passati
	if(argc != 3){
		printf("Usage: tris_client <host remoto> <porta>\n");
		exit(EXIT_FAILURE);
	}

	//creazione del socket TCP
	if(server = socket(PF_INET, SOCK_STREAM, 0) == -1){
		perror("Errore nella creazione del socket TCP");
		exit(EXIT_FAILURE);
	}

	/*
	//Copia dell'indirizzo passato nella variabile ip
	if(strcmp(argv[1], "localhost") == 0)
		strcpy(ip, "127.0.0.1");
	else
		strcpy(ip, argv[1]);
	*/
	
	//Azzeramento della struttura dati sockaddr_in
	memset(&server_addr, 0, sizeof(struct sockaddr_in));
	
	//Inserimento in sockaddr_in dei paramentri
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(atoi(argv[2]));
	inet_pton(AF_INET, argv[1], &server_addr.sin_addr.s_addr);
	
	//connessione al server
	if(connect(server, (SA *) &server_addr, sizeof(SA)) == -1){
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
	if(udp = socket(AF_INET, SOCK_DGRAM, 0) == -1){
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
	buffer.type = SETUSER;
	buffer.length = strlen(name)+3; //lunghezza stringa + \0 + 2 byte porta
	buffer.payload = (char *) malloc(buffer.length*sizeof(char));
	*((int16_t *) buffer.payload) = htons(UDPport);
	strcpy(&(buffer.payload[2]),name);
	sendPacket(server,&buffer,"Errore invio name e porta UDP");
	
	//Ricevo la conferma dal server
	recvPacket(server,&server_msg,"Errore ricezione controllo name");

	//Controllo che il name scelto sia libero
	while(server_msg.payload[0] != true)
	{
		printf("Il nome \"%s\" non è disponibile, scegline un'altro (max 32 caratteri): ",name);
		memset(name,' ',33);
		scanf("%s", name);
		flush();
		buffer.length = strlen(name)+3; //lunghezza stringa + \0 + 2 byte porta
		free(buffer.payload);
		buffer.payload = (char *) malloc(buffer.length*sizeof(char));
		*((int16_t *) buffer.payload) = htons(UDPport);
		strcpy(&(buffer.payload[2]),name);
		sendPacket(server,&buffer,"Errore invio name e porta UDP");
		
		//Ricevo la conferma dal server
		free(server_msg.payload);
		recvPacket(server,&server_msg,"Errore ricezione controllo name");
	}
	
	//Dealloco il payload del pacchetto ricevuto e azzero la struttura
	free(server_msg.payload);
	memset(&server_msg,0,sizeof(server_msg));
	
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
				{
					cmd = readCommand();
					if(cmd == -1)
					{
						//Comando non riconosciuto
						printf("Comando non riconosciuto!\n");
						printHelp(status);
						continue;
					}
					else
					{
						//Comando riconosciuto
						executeCommand(cmd);
					}
				}
				else //altrimenti ricevo un pacchetto dal socket pronto
				{
					handleRequest(ready);
				}
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
		printf(" * !who --> mostra l'elenco dei client connessi al server\n");
	printf(" * !connect nome_client --> avvia una partita con l'utente nome_client\n");
	printf(" * !disconnect --> disconnette il client dall'attuale partita\n");
	printf(" * !quit --> disconnette il client dal server\n");
	printf(" * !show_map --> mostra la mappa del gioco\n");
	printf(" * !hit num_cell --> marca la casella num_cell (valido quando e' il proprio turno)\n\n");
}


	
