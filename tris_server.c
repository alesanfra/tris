#include "tris_lib.h"

//Dichiarazioni delle funzioni definite dopo il main()
player* getBySocket(int socket);
player* getByName(char* name);
void acceptPlayer(int socket);
void removePlayer(player* pl);
void handleRequest(int socket);
void setUser(int socket, char* user, uint16_t UDPport);
void sendUserList(int socket);
void askToPlay(int socket, char* name);
void replyToPlayRequest(int socket, unsigned char type, char* name);
void setFree(int socket);
void addPacket(player* pl, unsigned char type, unsigned char length, char* payload);
void sendToClient(int socket);

//lista dei client connessi al server
player* clients = NULL;

//lista dei descrittori da controllare con la select()
fd_set masterreadset, masterwriteset;

//massimo descrittore
int fdmax=0;

int main(int argc, char* argv[])
{
	//allocazione delle strutture dati necessarie
	struct sockaddr_in my_addr;
	int listener, ready, flag = 1;
	fd_set readset, writeset;

	//controllo numero argomenti passati
	if(argc != 3)
	{
		printf("Usage: tris_client <host remoto> <porta>\n");
		exit(EXIT_FAILURE);
	}
	
	//creazione del socket di ascolto	
	if((listener = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("Errore nella creazione del socket");
		exit(EXIT_FAILURE);
	}
	
	//Modifica delle opzioni del socket listener
	if(setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int)) == -1)
	{
		perror("Errore nella modifica delle opzioni del socket listener");
		exit(EXIT_FAILURE);
	}

	//inizializzazione della sockaddr_in con ip e porta
	memset(&my_addr, 0, sizeof(my_addr));
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(atoi(argv[2]));
	inet_pton(AF_INET, argv[1], &my_addr.sin_addr.s_addr);
	
	//bind del socket	
	if(bind(listener, (struct sockaddr *) &my_addr, sizeof(my_addr)) == -1)
	{
		perror("Errore nell'esecuzione della bind del socket");
		exit(EXIT_FAILURE);
	}
	
	//messa in ascolto del server sul socket
	if(listen(listener, 10) == -1)
	{
		perror("Errore nella messa in ascolto del server");
		exit(EXIT_FAILURE);
	}
	
	//inizializzazione delle strutture dati utilizzate nella select()
	FD_ZERO(&masterreadset);
	FD_ZERO(&readset);
	FD_ZERO(&masterwriteset);
	FD_ZERO(&writeset);
	FD_SET(listener, &masterreadset);
	fdmax = listener;

	printf("Avvio di Tris Server\n\n");

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
					acceptPlayer(ready);
				//altrimenti gestisco la richiesta di un client
				else 
					handleRequest(ready);
			}
			
			//controllo se ready è un descrittore pronto in scrittura
			if(FD_ISSET(ready,&writeset))
				sendToClient(ready);
		}
	}
	
	//Questo codice non verrà mai eseguito dato che si trova
	//dopo un ciclo infinito
	return 0;	
}

player* getBySocket(int socket)
{
	player* pl = clients;
	
	while((pl != NULL) && (pl->socket != socket))
		pl = pl->next;
		
	//se non trova il player restituisce NULL
	return pl;
}

player* getByName(char* name)
{
	player* pl = clients;
	
	while((pl != NULL) && (strcmp(name,pl->name) != 0))
		pl = pl->next;
		
	return pl;
}

void acceptPlayer(int socket)
{
	//alloco le strutture dati necessarie
	int len = sizeof(struct sockaddr_in);	
	player* new = (player *) malloc(sizeof(player));
	memset(new, 0, sizeof(player));
	
	//accetto la connessione
	new->socket = accept(socket, (struct sockaddr *) &(new->address),(socklen_t *) &len);
	
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
	new->opponent = FREE;
	new->name = (char *) malloc(sizeof(char));
	*(new->name) = '\0';
	new->UDPport = 0;
	new->tail = NULL;
	
	//inserisco il client nella lista
	new->next = clients;
	clients  = new;
}

void removePlayer(player* pl)
{
	player** temp = &clients;
	player* opponent;
	
	if(pl == NULL)
		return;
	
	//chiudo il socket
	close(pl->socket);
	
	//tolgo il client nel master readset
	FD_CLR(pl->socket, &masterreadset);
	
	//se *temp->socket era il max cerco il nuovo fd massimo
	if(pl->socket == fdmax)
		for(; fdmax > 0; fdmax--)
			if(FD_ISSET(fdmax, &masterreadset))
				break;
	
	while(*temp != pl)
		temp = &((*temp)->next);
	
	//rimuovo l'oggetto dalla lista
	*temp = (*temp)->next;
	
	//Stampo il messaggio a video
	printf("%s si e' disconnesso\n", pl->name);
	
	//Se il giocatore era impegnato in una partita
	//metto a IDLE l'avversario
	if(pl->opponent != FREE)
	{
		opponent = getBySocket(pl->opponent);
		if(opponent != NULL)
			opponent->opponent = FREE;
	}
	
	//dealloco l'oggetto
	free(pl->name);
	free(pl);
}

void handleRequest(int socket)
{
	packet buffer_in;
	
	//ricevo il pacchetto dal client
	if(recvPacket(socket,&buffer_in,"Errore ricezione pacchetto dal client") < 1)
	{
		removePlayer(getBySocket(socket));	
		return;
	}
	
	//in base al tipo del pacchetto gestisco la richiesta
	switch(buffer_in.type)
	{
		case SETUSER:
			setUser(socket,&(buffer_in.payload[2]),*((uint16_t *) buffer_in.payload));
			break;
		
		case WHO:
			sendUserList(socket);
			break;
			
		case CONNECT:
			askToPlay(socket,buffer_in.payload);
			break;
		
		case MATCHACCEPTED:
		case MATCHREFUSED:
			replyToPlayRequest(socket,buffer_in.type,buffer_in.payload);
			break;
			
		case QUIT:
			removePlayer(getBySocket(socket));
			break;
			
		case SETFREE:
			setFree(socket);
			break;
		
		default:
			break;
	}
	
	//dealloco il payload
	if(buffer_in.length > 0)
		free(buffer_in.payload);
}

void setUser(int socket, char* user, uint16_t UDPport)
{
	if(getByName(user) == NULL)
	{
		//salvo nome e porta nel descrittore del client
		player* pl = getBySocket(socket);
		free(pl->name);
		pl->name = (char *) calloc(strlen(user)+1,sizeof(char));
		strcpy(pl->name,user);
		pl->UDPport = ntohs(UDPport);
		printf("%s si è connesso\n",pl->name);
		printf("%s è libero\n",pl->name);
		
		//dico al client che il nome richiesto è libero
		addPacket(getBySocket(socket),NAMEFREE,0,NULL);
	}
	else
		//altrimenti dico al client che il nome è occupato
		addPacket(getBySocket(socket),NAMEBUSY,0,NULL);
}

void sendUserList(int socket)
{
	player* pl = clients, *client = getBySocket(socket);
	uint16_t num = 0;
	
	printf("Richiesta lista client da %s\n",client->name);
	
	//conto i client connessi
	while(pl != NULL)
	{
		if(pl->name[0] != '\0')
			num++;
		pl = pl->next;
	}
	
	//Metto il numero in formato di rete
	num = htons(num);
	
	//Aggiungo il pacchetto alla lista di pacchetti da inviare al client
	addPacket(client,REPLYWHO,2,(char *) &num);
	
	//invio i nomi
	pl = clients;
	while(pl != NULL)
	{
		if(pl->name[0] != '\0')
		{
			char* username = calloc(strlen(pl->name)+2,sizeof(char));
			
			if(pl->opponent == FREE)
				username[0] = FREE;
			else
				username[0] = BUSY;
				
			strcpy(username+1,pl->name);
			addPacket(client,USERLIST,strlen(pl->name)+2,username);
			free(username);
		}
		pl = pl->next;
	}
}

void askToPlay(int socket, char* name)
{
	player *source, *target;
	
	source = getBySocket(socket);
	target  = getByName(name);
	
	if(target == NULL)
	{	//se il giocatore richiesto non esiste invio NOTFOUND
		addPacket(source,NOTFOUND,0,NULL);
	}
	else if(target == source)
	{	//se il giocatore richiesto è il richiedente invio YOURSELF
		addPacket(source,YOURSELF,0,NULL);
	}
	else if(target->opponent != FREE)
	{	//se il giocatore richiesto è occupato invio BUSY
		addPacket(source,BUSY,0,NULL);
	}
	else
	{
		//invio al giocatore target una richiesta di gioco
		addPacket(target,PLAYREQ,strlen(source->name)+1,source->name);
		printf("%s ha richiesto di giocare con %s\n",source->name,target->name);
		
		//salvo nel descrittore del richiedente il target
		source->opponent = target->socket;
	}
}

void replyToPlayRequest(int socket, unsigned char type, char* name)
{
	player *source, *target;
	
	source = getBySocket(socket);//ha risposto alla richiesta
	target  = getByName(name);//ha fatto la richiesta
	
	
	if(target == NULL || target->opponent != socket)
	{
		//Il richiedente non esiste o non ha richiesto di giocare
		addPacket(source,NOTVALID,0,NULL);
		return;
	}
	
	//invio al target il responso della richiesta
	addPacket(target,type,0,NULL);
	
	if(type == MATCHACCEPTED)
	{
		client_addr source_addr, target_addr;
		
		//salvo il client richiedente nel source
		source->opponent = target->socket;
		
		//invio porta e indirizzo ip dell'avversario al target
		source_addr.ip = source->address.sin_addr.s_addr;
		source_addr.port = htons(source->UDPport);
		//printf("Inviato a %s l'indirizzo %u : %hu\n",target->name,source_addr.ip,source->UDPport);
		addPacket(target,USERADDR,6,(char *) &source_addr);
		
		//invio porta e indirizzo ip del richiedente al source
		target_addr.ip = target->address.sin_addr.s_addr;
		target_addr.port = htons(target->UDPport);
		//printf("Inviato a %s l'indirizzo %u : %hu\n",source->name,target_addr.ip,target->UDPport);
		addPacket(source,USERADDR,6,(char *) &target_addr);
	}
	else if(type == MATCHREFUSED)
	{
		//segno il target come libero
		target->opponent = FREE;
	}
}

void setFree(int socket)
{
	player* pl = getBySocket(socket);
	
	if(pl != NULL)
	{
		pl->opponent = FREE;
		printf("%s e' libero\n",pl->name);
	}
}

void addPacket(player* pl, unsigned char type, unsigned char length, char* payload)
{
	packet **list, *new;
	
	if(pl == NULL)
		return;
	
	//creo il pacchetto da inviare
	new = (packet *) malloc(sizeof(packet));
	new->type = type;
	new->length = length;
	
	if(length != 0)
	{
		new->payload = (char *) malloc(length);
		memcpy(new->payload,payload,length);
	}
	else
		new->payload = NULL;
	
	new->next = NULL;
	
	//aggiungo il pacchetto in fondo alla lista dei pacchetti da inviare
	list = &(pl->tail);
	while(*list != NULL)
		list = &((*list)->next);
	*list = new;
	
	//metto il socket nel set di quelli da controllare in scrittura
	FD_SET(pl->socket,&masterwriteset);
}

void sendToClient(int socket)
{
	player* pl = getBySocket(socket);
	packet* sending = NULL;
	
	//se il player non è in lista o non ha pacchetti da spedire
	//lo tolgo dal write set
	if(pl == NULL || pl->tail == NULL)
	{
		FD_CLR(socket,&masterwriteset);
		return;
	}
	
	//prendo il primo pacchetto in coda e lo invio
	sending = pl->tail;
	sendPacket(socket,sending,"Errore invio pacchetto al client");
	//~ printf("Pacchetto inviato a %s: %hhu - %hhu\n",pl->name,sending->type,sending->length);
	
	//Tolgo il pacchetto dalla coda
	pl->tail = pl->tail->next;
	
	//Se non ci sono più pacchetti in coda tolgo il client dal writeset
	if(pl->tail == NULL)
		FD_CLR(socket,&masterwriteset);
	
	//Dealloco il payload e la struttura dati
	free(sending->payload);
	free(sending);
}
