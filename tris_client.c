#include "tris_lib.h"

//Dichiarazioni delle funzioni definite dopo il main()
void printHelp(int match);

int main(int argc, char* argv[])
{
	//dichiarazioni delle variabili
	struct sockaddr_in server_addr, opponent_addr;
	packet buffer, server_msg;
	int server, udp, fdmax, ready_des;
	int16_t UDPport;
	char *nickname, ip[16], status=IDLE;
	
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

	//Copia dell'indirizzo passato nella variabile ip
	if(strcmp(argv[1], "localhost") == 0)
		strcpy(ip, "127.0.0.1");
	else
		strcpy(ip, argv[1]);
	
	//Azzeramento della struttura dati sockaddr_in
	memset(&server_addr, 0, sizeof(struct sockaddr_in));
	
	//Inserimento in sockaddr_in dei paramentri
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(atoi(argv[2]));
	inet_pton(AF_INET, ip, &server_addr.sin_addr.s_addr);
	
	//connessione al server
	if(connect(server, (SA *) &server_addr, sizeof(SA)) == -1){
		perror("errore nella connessione al server");
		exit(EXIT_FAILURE);
	}
	
	//connessione avvenuta, richiesta ed invio di nome e porta
	printf("\nConnessione al server %s:%s effettuata con successo\n\n", argv[1], argv[2]);
	
	//stampo i comandi disponibili
	printHelp(IDLE);
	
	//Inserimento nickname
	printf("Inserisci il tuo nickname (max 32 caratteri): ");
	nickname = (char *) malloc(33*sizeof(char));
	scanf("%s",nickname);
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
	
	//Invio al server il nickname e la porta UDP di ascolto
	buffer.type = SETUSER;
	buffer.length = strlen(nickname)+3; //lunghezza stringa + \0 + 2 byte porta
	buffer.payload = (char *) malloc(buffer.length*sizeof(char));
	*((int16_t *) buffer.payload) = htons(UDPport);
	strcpy(&buffer.payload[2],nickname);
	sendPacket(server,&buffer,"Errore invio nickname e porta UDP");
	
	//Ricevo la conferma dal server
	server_msg = recvPacket(server,"Errore ricezione controllo nickname");

	//Controllo che il nickname scelto sia libero
	while(server_msg.payload[0] != true)
	{
		printf("Il nome \"%s\" non è disponibile, scegline un'altro (max 32 caratteri): ",nickname);
		memset(nickname,' ',33);
		scanf("%s", nickname);
		flush();
		buffer.length = strlen(nickname)+3; //lunghezza stringa + \0 + 2 byte porta
		free(buffer.payload);
		buffer.payload = (char *) malloc(buffer.length*sizeof(char));
		*((int16_t *) buffer.payload) = htons(UDPport);
		strcpy(&buffer.payload[2],nickname);
		sendPacket(server,&buffer,"Errore invio nickname e porta UDP");
		
		//Ricevo la conferma dal server
		server_msg = recvPacket(server,"Errore ricezione controllo nickname");
	}
	
	/*Da questo punto in poi il client è pronto a giocare*/
	
	for(;;)
	{
		if(status == IDLE)
			printf("> ");
		else
			printf("# ");
			
		
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


	
