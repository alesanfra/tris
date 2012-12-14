#include "tris_lib.h"

//lista dei client connessi al server
player *clients = NULL;

//numero dei client connessi al server
int NUM_CLIENTS = 0;

//lista dei descrittori da controllare con la select()
fd_set masterreadset, masterwriteset;

//massimo descrittore
int fdmax=0;

void acceptPlayer(int sk);
player* getBySocket(int socket);
player* getByName(char* name);
void rmPlayer(player* pl);
bool handleRequest(int socket);
bool runAction(int socket);

int main(int argc, char* argv[])
{
	//allocazione delle strutture dati necessarie
	struct sockaddr_in my_addr;
	int listener, ready, yes=1;
	fd_set readset, writeset;

	//inizializzazione degli fd_set
	FD_ZERO(&masterreadset);
	FD_ZERO(&readset);
	FD_ZERO(&masterwriteset);
	FD_ZERO(&writeset);

	//controllo numero argomenti passati
	if(argc != 3){
		printf("Usage: tris_client <host remoto> <porta>\n");
		exit(EXIT_FAILURE);
	}
	
	//creazione del socket di ascolto	
	if((listener = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("Errore nella creazione del socket");
		exit(EXIT_FAILURE);
	}
	
	/*
	//Modifica delle opzioni del socket listener
	if(setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1){
		perror("Errore nella modifica delle opzioni del socket listener");
		exit(EXIT_FAILURE);
	}
	
	/*
	//Copia dell'indirizzo passato nella variabile ip
	if(strcmp(argv[1], "localhost") == 0)
		strcpy(ip, "127.0.0.1");
	else
		strcpy(ip, argv[1]);
	*/
		
	//inizializzazione della sockaddr_in con ip e porta
	memset(&my_addr, 0, sizeof(my_addr));
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(atoi(argv[2]));
	inet_pton(AF_INET, argv[1], &my_addr.sin_addr.s_addr);
	
	//bind del socket	
	if(bind(listener, (SA *) &my_addr, sizeof(my_addr)) == -1)
	{
		perror("Errore nell'esecuzione della bind del socket");
		exit(EXIT_FAILURE);
	}
	
	//messa in ascolto del server sul socket
	if(listen(listener, 25) == -1)
	{
		perror("Errore nella messa in ascolto del server");
		exit(EXIT_FAILURE);
	}
	
	//inizializzazione delle strutture dati utilizzate nella select()
	FD_SET(listener, &masterreadset);
	fdmax = listener;

	//ciclo in cui il server chiama la select() ed esegue le azioni richieste
	for(;;)
	{
		//copio readset e writeset perché vengono modificati dalla select()
		readset = masterreadset;
		writeset = masterwriteset;
		
		//chiamo la select
		if(select(fdmax+1, &readset, &writeset, NULL, NULL) == -1)
		{
			perror("Errore nella select()");
			exit(EXIT_FAILURE);
		}
		
		//ciclo in cui scorro i descrittori e gestisco quelli pronti
		for(ready = 0; ready <= fdmax; ready++)
		{
			//controllo se ready è un descrittore pronto in lettura
			if(FD_ISSET(ready,&readset))
			{
				//se il descrittore pronto e' listener accetto il client
				if(ready == listener)
				{
					printf("prima della accept\n");
					fflush(stdout);
					acceptPlayer(ready);
					printf("dopo la accept\n");
					fflush(stdout);
				}
				else //altrimenti gestisco la richiesta di un client
				{
					handleRequest(ready);
				}
			}
			
			//controllo se ready è un descrittore pronto in scrittura
			if(FD_ISSET(ready,&writeset))
			{
				runAction(ready);
			}
			
		}
	}
	
	//Chiusura del socket di ascolto
	//Questo codice non verrà mai eseguito dato che si trova
	//dopo un ciclo infinito
	
	printf("\nChiusura del server\n");
	close(listener);
	return 0;	
}

void acceptPlayer(int sk)
{
	//alloco le strutture dati necessarie
	int len = sizeof(struct sockaddr_in);	
	player* new = (player *) malloc(sizeof(player));
	memset(new, 0, sizeof(player));
	
	//accetto la connessione
	new->socket = accept(sk, (SA *) &(new->address),(socklen_t *) &len);
	
	if(new->socket == -1)
	{
		perror("Errore nell'accettazione di una richiesta");
		free(new);
		return;
	}
	
	printf("Connessione stabilita con il client\n");
	fflush(stdout);
	
	//inserisco il client nel master readset
	FD_SET(new->socket, &masterreadset);
	if(new->socket > fdmax)
		fdmax = new->socket;
		
	//setto i campi del player
	new->name = (char *) malloc(sizeof(char));
	*(new->name) = '\0'; 
	new->status = IDLE;
	new->UDPport = 0;
	memset(&(new->address), 0, sizeof(struct sockaddr_in));
	new->tail = NULL;
	
	//inserisco il client nella lista
	new->next = clients;
	clients  = new;
}

player* getBySocket(int socket)
{
	player* pl = clients;
	
	while(pl != NULL && pl->socket != socket)
		pl = pl->next;
		
	return pl;
}

player* getByName(char* name)
{
	player* pl = clients;
	
	printf("prima della ricerca\n");
	fflush(stdout);
	
	while(pl != NULL && (strcmp(name,pl->name) != 0))
		pl = pl->next;
		
	printf("trovato player: %i\n",(int) pl);
	fflush(stdout);
		
	return pl;
}

void rmPlayer(player* pl)
{
	player** temp = &clients;
	
	if(pl == NULL)
		return;
		
	//inserisco il client nel master readset
	FD_CLR((*temp)->socket, &masterreadset);
	
	//se temp->socket era il max cerco il nuovo fd massimo
	if((*temp)->socket == fdmax)
		for(; fdmax > 0; fdmax--)
			if(FD_ISSET(fdmax, &masterreadset))
				break;
	
	while(*temp != pl)
		temp = &((*temp)->next);
	
	//rimuovo l'oggetto dalla lista
	*temp = (*temp)->next;
	
	//dealloco l'oggetto
	free(pl->name);
	free(pl);
}

bool runAction(int socket)
{
	
	
	return true;
}

bool handleRequest(int socket)
{
	packet buffer_in, buffer_out;
	char* user;
	
	//ricevo il pacchetto dal client
	recvPacket(socket,&buffer_in,"Errore ricezione pacchetto dal client");
	
	
	switch(buffer_in.type)
	{
		case SETUSER:
			user = &(buffer_in.payload[2]);
			printf("utente: %s\n",user);
			//preparo la risposta
			buffer_out.type = REPLYUSER;
			buffer_out.length = 1;
			buffer_out.payload = (char *) malloc(sizeof(char));
			
			if(getByName(user) == NULL)
				*(buffer_out.payload) = true;
			else
				*(buffer_out.payload) = false;
			
			printf("prima invio pacchetto: %s\n",buffer_out.payload);
			fflush(stdout);
			
			sendPacket(socket,&buffer_out,"Errore invio risposta");
			
			printf("dopo invio pacchetto");
			fflush(stdout);
			break;
		
		default:
			break;
	}
	
	return true;
}

