#include "tris_lib.h"

/* Elimina dallo stdin una linea di caratteri */

void flush()
{
	while(getchar()!='\n');
}

/* Invia un pacchetto al socket passato per argomento */

int sendPacket(int socket, packet* buffer, const char* error_message)
{
	int ret = 0;
	char* send_buffer;
	send_buffer = (char *) calloc((buffer->length)+2, sizeof(char));
	
	//costruzione del pacchetto da inviare
	send_buffer[0] = buffer->type;
	send_buffer[1] = buffer->length;
	
	if(buffer->length != 0)
		memcpy(&send_buffer[2], buffer->payload, buffer->length);
	
	ret = send(socket, (void *) send_buffer, (buffer->length) + 2, 0); 
	if(ret == -1)
	{
		perror(error_message);
		exit(EXIT_FAILURE);
	}
	
	//dealloco il buffer di invio
	free(send_buffer);
	return ret;
}


/* Riceve un pacchetto dal socket passato per argomento */

int recvPacket(int socket, packet* buffer, const char* error_message)
{
	int ret = 0;
	char* recv_buffer;
	
	//azzero la struttura dati
	memset(buffer,0,sizeof(buffer));
	
	//ricezione dell'header del pacchetto
	ret = recv(socket, (void *) buffer, 2, MSG_PEEK);
	if(ret < 1)
	{
		if(ret < 0)
			perror(error_message);
			
		return ret;
	}
	
	//~ printf("Ricevuto header %hhu, lunghezza: %hhu\n",buffer->type,buffer->length);
	//~ 
	//~ //allocazione dello spazio necessario a contenere il payload
	//~ if(buffer->length == 0)
		//~ return 2;
	//~ else
		//~ buffer->payload = (char *) calloc(buffer->length, sizeof(char));
		
	recv_buffer = (char *) calloc(buffer->length + 2, sizeof(char));
	
	//ricezione del payload del pacchetto
	ret = recv(socket, (void *) recv_buffer, buffer->length + 2, 0);
	if(ret < (buffer->length + 2))
	{
		if(ret != 0)
			perror(error_message);
			
		return ret;
	}
	
	if(buffer->length > 0)
	{	//il payload e dealloco il buffer
		buffer->payload = (char *) calloc(buffer->length, sizeof(char));
		memcpy(buffer->payload,recv_buffer+2,buffer->length);
	}
	free(recv_buffer);
	
	//restituisco il numero di byte letti sul socket
	return ret;
}

void cleanSocket(int socket)
{
	char buff;
	while(recv(socket,(void *)&buff,1,MSG_DONTWAIT) > 0)
	{
		//printf("%c\n",buff);
	}
}
