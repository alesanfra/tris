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
void setFree(player* pl);
void addPacket(player* pl, unsigned char type, unsigned char length, char* payload);
void sendToClient(int socket);

//Lista dei client connessi al server
player* clients = NULL;

//Lista dei descrittori da controllare con la select()
fd_set masterreadset, masterwriteset;
int fdmax=0;

int main(int argc, char* argv[])
{
	//Allocazione delle strutture dati necessarie
	struct sockaddr_in my_addr;
	int listener, ready, flag = 1;
	fd_set readset, writeset;

	//Controllo numero argomenti passati
	if(argc != 3)
	{
		printf("Usage: tris_client <host remoto> <porta>\n");
		exit(EXIT_FAILURE);
	}
	
	//Creazione del socket di ascolto	
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

	//Inizializzazione della sockaddr_in con ip e porta
	memset(&my_addr, 0, sizeof(my_addr));
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(atoi(argv[2]));
	inet_pton(AF_INET, argv[1], &my_addr.sin_addr.s_addr);
	
	//Bind del socket	
	if(bind(listener, (struct sockaddr *) &my_addr, sizeof(my_addr)) == -1)
	{
		perror("Errore nell'esecuzione della bind del socket");
		exit(EXIT_FAILURE);
	}
	
	//Messa in ascolto del server sul socket
	if(listen(listener, 20) == -1)
	{
		perror("Errore nella messa in ascolto del server");
		exit(EXIT_FAILURE);
	}
	
	//Inizializzazione delle strutture dati utilizzate nella select()
	FD_ZERO(&masterreadset);
	FD_ZERO(&readset);
	FD_ZERO(&masterwriteset);
	FD_ZERO(&writeset);
	FD_SET(listener, &masterreadset);
	fdmax = listener;

	printf("Avvio di Tris Server\n\n");

	//Ciclo in cui il server chiama la select() ed esegue le azioni richieste
	for(;;)
	{
		//Copio readset e writeset perché vengono modificati dalla select()
		readset = masterreadset;
		writeset = masterwriteset;
		
		//Chiamo la select
		if(select(fdmax+1, &readset, &writeset, NULL, NULL) == -1)
		{
			perror("Errore nella select()");
			exit(EXIT_FAILURE);
		}
		
		//Ciclo in cui scorro i descrittori e gestisco quelli pronti
		for(ready = 0; ready <= fdmax; ready++)
		{
			//Controllo se ready è un descrittore pronto in lettura
			if(FD_ISSET(ready,&readset))
			{
				//Se il descrittore pronto e' listener accetto il client
				if(ready == listener)
					acceptPlayer(ready);
				//Altrimenti gestisco la richiesta di un client
				else 
					handleRequest(ready);
			}
			
			//Controllo se ready è un descrittore pronto in scrittura
			if(FD_ISSET(ready,&writeset))
				sendToClient(ready);
		}
	}
	
	//Questo codice non verrà mai eseguito dato che si trova
	//dopo un ciclo infinito
	return 0;	
}

/* 
 * Restituisce il puntatore alla struttura dati "player"
 * corrispondente al socket passato per argomento
 * 		Parametri:
 * 			int socket: socket associato al giocatore
 */

player* getBySocket(int socket)
{
	player* pl = clients;
	
	while((pl != NULL) && (pl->socket != socket))
		pl = pl->next;
		
	return pl; //se non trova il player restituisce NULL
}

/*
 * Restituisce il puntatore alla struttura dati "player"
 * corrispondente al nome passato per argomento 
 * 		Parametri:
 * 			char* name: nome del giocatore che si sta cercando 
 */

player* getByName(char* name)
{
	player* pl = clients;
	
	while((pl != NULL) && (strcmp(name,pl->name) != 0))
		pl = pl->next;
		
	return pl; //se non trova il player restituisce NULL
}

/* 
 * Accetta il client che ha fatto richiesta di connessione sul
 * socket passato per argomento, crea la strutture dati "player",
 * la inizializza e la inserisce nella lista client 
 * 		Parametri:
 * 			int socket: socket di ascolto sul quale il client ha fatto
 * 						richiesta di connessione
 */
 
void acceptPlayer(int socket)
{
	//Alloco le strutture dati necessarie
	int len = sizeof(struct sockaddr_in);	
	player* new = (player *) malloc(sizeof(player));
	memset(new, 0, sizeof(player));
	
	//Accetto la connessione
	new->socket = accept(socket, (struct sockaddr *) &(new->address),(socklen_t *) &len);
	
	if(new->socket == -1)
	{
		perror("Errore nell'accettazione di una richiesta");
		free(new);
		return;
	}
	
	printf("Connessione stabilita con il client\n");
	fflush(stdout);
	
	//Inserisco il client nel master readset
	FD_SET(new->socket, &masterreadset);
	if(new->socket > fdmax)
		fdmax = new->socket;
		
	//Inizializzo i campi del player
	new->name = (char *) malloc(sizeof(char));
	*(new->name) = '\0';
	new->packets_tail = NULL;
	new->opponent = NULL;
	new->status = IDLE;
	
	//Inserisco il client nella lista
	new->next = clients;
	clients  = new;
}

/* 
 * Chiude il socket del giocatore passato per argomento, lo toglie 
 * dalla lista e dealloca la struttura dati 
 * 		Parametri:
 * 			player* pl: puntatore al giocatore da rimuovere
 */

void removePlayer(player* pl)
{
	player** temp = &clients;
	
	if(pl == NULL)
		return;
	
	//Chiudo il socket
	close(pl->socket);
	
	//Tolgo il client nel masterreadset e nel masterwriteset
	FD_CLR(pl->socket, &masterreadset);
	FD_CLR(pl->socket, &masterwriteset);
	
	//Se pl->socket era il max cerco il nuovo fd massimo
	if(pl->socket == fdmax)
		for(; fdmax > 0; fdmax--)
			if(FD_ISSET(fdmax, &masterreadset))
				break;
	
	//Cerco il puntatore al giocatore da rimuovere
	while(*temp != pl)
		temp = &((*temp)->next);
	
	//Rimuovo l'oggetto dalla lista
	*temp = (*temp)->next;
	
	//Stampo il messaggio a video
	if(pl->name[0] == '\0')
		printf("Client sconosciuto si e' disconnesso\n");
	else
		printf("%s si e' disconnesso\n", pl->name);
	
	//Se il giocatore aveva richieste pendenti segno come libero
	//il richiedente e gli invio un'avviso
	if(pl->status == PENDING_REQ && pl->opponent != NULL)
	{
		addPacket(pl->opponent,UNAVAILABLE,0,NULL);
		setFree(pl->opponent);
	}
	
	//Se il giocatore aveva dei messaggi in coda li elimino
	while(pl->packets_tail != NULL)
	{
		packet* pkt = pl->packets_tail;
		pl->packets_tail = pl->packets_tail->next;
		if(pkt->length > 0)
			free(pkt->payload);
		free(pkt);
	}
	
	//Dealloco l'oggetto
	free(pl->name);
	free(pl);
}

/* 
 * Riceve un pacchetto dal socket passato come argomento, legge
 * il tipo e chiama la funzione di gestione della richiesta
 * 		Parametri:
 * 			int socket: socket da cui ricevere la richiesta da gestire
 */

void handleRequest(int socket)
{
	packet buffer;
	
	//Ricevo il pacchetto dal client
	if(recvPacket(socket,&buffer,KEEP_ALIVE,"Errore ricezione pacchetto dal client") < 1)
	{
		removePlayer(getBySocket(socket));	
		return;
	}
	
	//In base al tipo del pacchetto gestisco la richiesta
	switch(buffer.type)
	{
		case SETUSER:
			setUser(socket,&(buffer.payload[2]),*((uint16_t *) buffer.payload));
			break;
		
		case WHO:
			sendUserList(socket);
			break;
			
		case CONNECT:
			askToPlay(socket,buffer.payload);
			break;
		
		case MATCHACCEPTED:
		case MATCHREFUSED:
			replyToPlayRequest(socket,buffer.type,buffer.payload);
			break;
			
		case QUIT:
			removePlayer(getBySocket(socket));
			break;
			
		case SETFREE:
			setFree(getBySocket(socket));
			break;
		
		default:
			printf("Ricevuto pacchetto con tipo non riconosciuto\n");
			break;
	}
	
	//Dealloco il payload
	if(buffer.length > 0)
		free(buffer.payload);
}

/* 
 * Imposta al giocatore corrispondente al socket passato per argomento
 * nome utente e porta anch'essi passati come argomenti. Se il nome 
 * scelto è già occupato manda la client un messaggio di errore
 * 		Parametri:
 * 			int socket: socket sul quale è arrivata la richiesta
 * 			char* user: nome utente inviato dal client
 * 			uint16_t UDPport: porta udp inviata dal client
 */

void setUser(int socket, char* user, uint16_t UDPport)
{
	if(getByName(user) == NULL)
	{
		//Salvo nome e porta nel descrittore del client
		player* pl = getBySocket(socket);
		
		if(pl != NULL)
		{
			free(pl->name);
			pl->name = (char *) calloc(strlen(user)+1,sizeof(char));
			strcpy(pl->name,user);
			pl->UDPport = ntohs(UDPport);
			printf("%s si è connesso\n",pl->name);
			printf("%s è libero\n",pl->name);
			
			//Dico al client che il nome richiesto è libero
			addPacket(getBySocket(socket),NAMEFREE,0,NULL);
		}
	}
	else
		//Altrimenti dico al client che il nome è occupato
		addPacket(getBySocket(socket),NAMEBUSY,0,NULL);
}

/* 
 * Invia sul socket passato per argomento la lista dei client connessi
 * al server in quel momento
 * 		Parametri:
 * 			int socket: socket sul quale inviare la lista degli utenti
 */

void sendUserList(int socket)
{
	player* list = clients, *pl = getBySocket(socket);
	uint16_t num = 0;
	
	if(pl == NULL)
		return;
	
	printf("%s ha richiesto la lista degli utenti connessi\n",pl->name);
	
	//Conto i client connessi
	while(list != NULL)
	{
		if(list->name[0] != '\0')
			num++;
		list = list->next;
	}
	
	//Metto il numero in formato di rete
	num = htons(num);
	
	//Aggiungo il pacchetto alla lista di pacchetti da inviare al client
	addPacket(pl,REPLYWHO,2,(char *) &num);
	
	//Invio i nomi
	list = clients;
	while(list != NULL)
	{
		if(list->name[0] != '\0')
		{
			char* username = calloc(strlen(list->name)+2,sizeof(char));
			
			if(list->status == IDLE)
				username[0] = IDLE;
			else
				username[0] = BUSY;
				
			strcpy(username+1,list->name);
			addPacket(pl,USERLIST,strlen(list->name)+2,username);
			free(username);
		}
		list = list->next;
	}
}

/* 
 * Invia al giocatore corrispondente al nome passato per argomento
 * una richiesta di gioco. In caso non sia possibile recapitare la
 * richiesta invia un messaggio di errore al richiedente 
 * 		Parametri:
 * 			int socket: socket del richiedente
 * 			char* name: nome del giocatore a cui inoltrare la richiesta
 */

void askToPlay(int socket, char* name)
{
	player *source, *target;
	
	source = getBySocket(socket);
	target = getByName(name);
	
	if(source == NULL)
	{
		close(socket);
		return;
	}
	
	if(target == NULL)
	{	//se il giocatore richiesto non esiste invio NOTFOUND
		addPacket(source,NOTFOUND,0,NULL);
	}
	else if(target == source)
	{	//se il giocatore richiesto è il richiedente invio YOURSELF
		addPacket(source,YOURSELF,0,NULL);
	}
	else if(target->status == PENDING_REQ)
	{	//se il giocatore richiesto ha già un'altra richiesta invio PENDING_REQ
		addPacket(source,PENDING_REQ,0,NULL);
	}
	else if(target->status != IDLE)
	{	//se il giocatore richiesto è occupato invio BUSY
		addPacket(source,BUSY,0,NULL);
	}
	else
	{
		//invio al giocatore target una richiesta di gioco
		addPacket(target,PLAYREQ,strlen(source->name)+1,source->name);
		printf("%s ha richiesto di giocare con %s\n",source->name,target->name);
		
		//salvo nel descrittore del richiedente il target
		source->opponent = target;
		source->status = WAIT;
		
		//blocco il target in modo che nessuno si inserisca
		target->opponent = source;
		target->status = PENDING_REQ;
	}
}

/* 
 * Gestisce la risposta ad una richiesta di gioco, e in caso di
 * risposta positiva invia ai giocatori l'indirizzo ip e la porta UDP
 * dei rispettivi avversari 
 * 		Parametri:
 * 			int socket: socket del player che ha ricevuto la richiesta
 * 			unsigned char type: esito della richiesta
 * 			char* name: nome del richiedente
 */

void replyToPlayRequest(int socket, unsigned char type, char* name)
{
	player *source, *target;
	
	source = getBySocket(socket);//Ha risposto alla richiesta
	target  = getByName(name);//Ha fatto la richiesta
	
	if(source == NULL)
	{
		close(socket);
		return;
	}
	
	if(target == NULL || target->opponent != source || source->opponent != target)
	{
		//Il richiedente non esiste o non ha richiesto di giocare
		addPacket(source,NOTVALID,0,NULL);
		setFree(source);
		return;
	}
	
	//Invio al target il responso della richiesta
	addPacket(target,type,0,NULL);
	
	if(type == MATCHACCEPTED)
	{
		client_addr source_addr, target_addr;
		
		//Salvo il client richiedente nel source
		source->opponent = target;
		source->status = BUSY;
		
		//Metto il richiedente a occupato
		target->status = BUSY;
		
		//Invio porta e indirizzo ip dell'avversario al target
		source_addr.ip = source->address.sin_addr.s_addr;
		source_addr.port = htons(source->UDPport);
		addPacket(target,USERADDR,sizeof(source_addr),(char *) &source_addr);
		
		//Invio porta e indirizzo ip del richiedente al source
		target_addr.ip = target->address.sin_addr.s_addr;
		target_addr.port = htons(target->UDPport);
		addPacket(source,USERADDR,sizeof(target_addr),(char *) &target_addr);
		
		printf("%s ha accettato di giocare con %s\n",source->name,target->name);
	}
	else if(type == MATCHREFUSED)
	{
		//Segno il target e il source come liberi
		printf("%s ha rifiutato la proposta di gioco di %s\n",source->name,target->name);
		setFree(source);
		setFree(target);
	}
}

/* 
 * Imposta lo stato del giocatore passato per argomento al "libero"
 * 		Parametri:
 * 			player* pl: puntatore al descrittore del giocatore
 */

void setFree(player* pl)
{	
	if(pl != NULL)
	{
		pl->opponent = NULL;
		pl->status = IDLE;
		printf("%s e' libero\n",pl->name);
	}
}

/* 
 * Aggiunge un pacchetto alla coda dei pacchetti da inviare al client
 * passato per argomento 
 * 		Parametri:
 * 			player* pl: giocatore destinatario del pacchetto
 * 			unsigned char type: tipo del pacchetto
 * 			unsigned char length: lunghezza del payload
 * 			char* payload: puntatore al payload
 */

void addPacket(player* pl, unsigned char type, unsigned char length, char* payload)
{
	packet **list, *new;
	
	if(pl == NULL)
		return;
	
	//Creo il pacchetto da inviare
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
	
	//Aggiungo il pacchetto in fondo alla lista dei pacchetti da inviare
	list = &(pl->packets_tail);
	while(*list != NULL)
		list = &((*list)->next);
	*list = new;
	
	//Metto il socket nel set di quelli da controllare in scrittura
	FD_SET(pl->socket,&masterwriteset);
}

/* 
 * Invia al client corrispondente al socket passato per argomento
 * il primo pacchetto in coda
 * 		Parametri:
 * 			int socket: socket pronto in scrittura a cui inviare
 * 						un pacchetto
 */

void sendToClient(int socket)
{
	player* pl = getBySocket(socket);
	packet* sending = NULL;
	
	//Se il player non è in lista o non ha pacchetti da spedire
	//lo tolgo dal write set
	if(pl == NULL || pl->packets_tail == NULL)
	{
		FD_CLR(socket,&masterwriteset);
		return;
	}
	
	//Prendo il primo pacchetto in coda e lo invio
	sending = pl->packets_tail;
	sendPacket(socket,sending,"Errore invio pacchetto al client");
	
	//Tolgo il pacchetto dalla coda
	pl->packets_tail = pl->packets_tail->next;
	
	//Se non ci sono più pacchetti in coda tolgo il client dal writeset
	if(pl->packets_tail == NULL)
		FD_CLR(socket,&masterwriteset);
	
	//Dealloco il payload
	if(sending->length > 0)
		free(sending->payload);
		
	//Dealloco la struttura dati
	free(sending);
}
