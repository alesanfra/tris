#include "tris_lib.h"

/* 
 * Elimina dallo stdin una linea di caratteri 
 */

void flush()
{
	char buff;
	
	do{
		buff = getchar();
	}
	while(buff != '\n' && buff != EOF);
}



/* 
 * Elimina tutti i byte sul socket
 * 	Parametri:
 * 		int socket: socket da pulire
 */

void cleanSocket(int socket)
{
	char buff;
	while(recv(socket,(void *)&buff,1,MSG_DONTWAIT) > 0)
	{
		//printf("%c ",buff);
	}
}



/* 
 * Invia un pacchetto al socket passato per argomento
 * 	Parametri:
 * 		int socket: socket dell'destinatario
 * 		packet* buffer: pachhetto da inviare
 * 		const char* error_message: messaggio da visualizzare in caso di err
 */

void sendPacket(int socket, packet* buffer, const char* error_message)
{
	int ret = 0;
	char* send_buffer;
	send_buffer = (char *) calloc((buffer->length)+2, sizeof(char));
	
	//Costruzione del pacchetto da inviare
	send_buffer[0] = buffer->type;
	send_buffer[1] = buffer->length;
	
	if(buffer->length != 0)
		memcpy(send_buffer+2, buffer->payload, buffer->length);
	
	ret = send(socket, (void *) send_buffer, (buffer->length) + 2, 0); 
	if(ret == -1)
	{
		perror(error_message);
		exit(EXIT_FAILURE);
	}
	
	//Dealloco il buffer di invio
	free(send_buffer);
}



/* 
 * Riceve un pacchetto dal socket passato per argomento
 * 	Parametri:
 * 		int socket: socket dell'mittente
 * 		packet* buffer: buffer dove salvare il pacchetto in arrivo
 * 		char flag: specifica cosa fare in caso di errore
 * 		const char* error_message: messaggio da visualizzare in caso di err
 */

int recvPacket(int socket, packet* buffer, char flag, const char* error_message)
{
	int ret = 0, header_len;
	
	//Calcolo la lunghezza dell'header del pacchetto
	header_len = sizeof(buffer->type) + sizeof(buffer->length);
	
	//azzero la struttura dati
	memset(buffer,0,sizeof(buffer));
	
	//ricezione dell'header del pacchetto
	ret = recv(socket, (void *) buffer, header_len, MSG_WAITALL);
	
	//Se ricevo meno di due byte stampo un messaggio di errore
	if(ret < header_len)
	{
		if(ret < 0)
			perror(error_message);
		else if(ret > 0)
			printf("\rRicevuto pacchetto corrotto\n");
			
		if(flag == KILL)
		{
			printf("\rConnessione TCP interrotta\n");
			exit(EXIT_FAILURE);
		}
		else
			return ret;
	}
	
	//Allocazione dello spazio necessario a contenere il payload
	if(buffer->length == 0)
		return ret;
	else
		buffer->payload = (char *) calloc(buffer->length, sizeof(char));
	
	//Ricezione del payload del pacchetto
	ret = recv(socket, (void *) buffer->payload, buffer->length, 0);
	
	//Se il byte ricevuti sono meno del previsto stampo un messaggio
	//di errore
	if(ret < buffer->length)
	{
		if(ret < 0)
			perror(error_message);
		else if(ret > 0)
			printf("\rRicevuti meno byte di quelli specificati nell'header\n");
		
		if(flag == KILL)
		{
			printf("\rConnessione TCP interrotta \n");
			exit(EXIT_FAILURE);
		}
		else
			return ret;
	}
	
	//Restituisco il numero di byte letti sul socket
	return ret;
}



/* 
 * Riceve un pacchetto UDP controllando che il mittente sia il client
 * passato per argomento
 * 	Parametri:
 * 		client* peer: nome, socket e indirizzo dell'mittente
 * 		packet* buffer: buffer dove salvare il pacchetto in arrivo
 * 		const char* error_message: messaggio da visualizzare in caso di err
 */

int recvPacketFrom(client* peer, packet* buffer, const char* error_message)
{
	int ret = 0, header_len;
	char* recv_buffer;
	struct sockaddr_in recv_addr;
	socklen_t len = sizeof(peer->address);
	header_len = sizeof(buffer->type) + sizeof(buffer->length);
	
	//Azzero la struttura dati
	memset(buffer,0,sizeof(buffer));
	memset(&recv_addr,0,sizeof(recv_addr));
	
	//Ricezione dell'header del pacchetto (type e length)
	ret = recvfrom(peer->socket, (void *) buffer, header_len, MSG_PEEK,(struct sockaddr*)&recv_addr,&len);
	
	//Se ricevo meno di due byte stampo un messaggio di errore
	if(ret < 2)
	{
		if(ret < 0)
			perror(error_message);
		else if(ret > 0);
			printf("\rRicevuto pacchetto corrotto\n");
		return -1;
	}
	
	//Alloco lo spazio per tutto il pacchetto (type+length+payload)
	recv_buffer = (char *) calloc(buffer->length + 2, sizeof(char)); 
	
	//Ricevo tutto il pacchetto, compresi type e length che non erano
	//stati eliminati dalla precendente recvfrom() causa MSG_PEEK
	ret = recvfrom(peer->socket, (void *) recv_buffer, buffer->length + 2, 0,(struct sockaddr*)&recv_addr,&len);
	
	//Se il byte ricevuti sono meno del previsto stampo un messaggio
	//di errore
	if(ret < (buffer->length + 2))
	{
		if(ret < 0)
			perror(error_message);
		else if(ret > 0)
			printf("\rRicevuti meno byte di quelli specificati nell'header\n");
		return ret;
	}
	
	//Se il pacchetto conetiene un payload alloco lo spazio necessario
	//e ci copio il contenuto del recv_buffer a partire dal terzo byte,
	//saltando type e length
	if(buffer->length > 0)
	{
		buffer->payload = (char *) calloc(buffer->length, sizeof(char));
		memcpy(buffer->payload,recv_buffer+2,buffer->length);
	}
	
	//Dealloco il buffer usato per ricevere tutto il contenuto del segmento
	free(recv_buffer);
	
	//Controllo che il mittente sia l'avversario
	if(recv_addr.sin_addr.s_addr == peer->address.sin_addr.s_addr && recv_addr.sin_port == peer->address.sin_port)
		return ret;//restituisco il numero di byte letti sul socket
	else
	{
		printf("\rRicevuto pacchetto non proveniente dall'avversario (%hhu,%hhu)\n",buffer->type,buffer->length);
		return -1;//restituisco errore
	}
}



/* 
 * Legge una linea dal terminale, la alloca in memoria dinamica e 
 * restitusce il puntatore
 * 	Parametri:
 * 		int max: numero massimo di caratteri nella stringa
 */

char* readLine(int max)
{
	int i=1;
	
	if(max < 2)
		return NULL;
	
	char* line = (char *) malloc(max);
	
	do{
		line[0] = getchar();
		if(line[0] == EOF)
		{
			free(line);
			return NULL;
		}
	} while(line[0] == ' ' || line[0] == '\n');
	
	for(i=1; i<max-1; i++)
	{
		line[i] = getchar();
		if(line[i] == EOF || line[i] == '\n')
			break;
	}
	
	line[i] = '\0';
	line = realloc(line,strlen(line)+1);
	
	if(i==max-1)
		flush();
	
	return line;
}
