#include "tris_lib.h"

void flush()
{
	while(getchar()!='\n');
}

void sendPacket(int socket, packet* buffer, const char* error_message)
{
	char* send_buffer;
	send_buffer = (char *) calloc((buffer->length)+2, sizeof(char));
	
	//costruzione del pacchetto da inviare
	send_buffer[0] = buffer->type;
	send_buffer[1] = buffer->length;
	
	if(buffer->length != 0)
		memcpy(&send_buffer[2], buffer->payload, buffer->length);
	
	if(send(socket, (void *) send_buffer, (buffer->length) + 2, 0) == -1)
	{
		perror(error_message);
		exit(EXIT_FAILURE);
	}
	
	//dealloco il buffer di invio
	free(send_buffer);
}

int recvPacket(int socket, packet* buffer, const char* error_message)
{
	int ret = 0;
	
	//azzero la struttura dati
	memset(buffer,0,sizeof(buffer));
	
	//ricezione dell'header del pacchetto
	ret = recv(socket, (void *) buffer, 2, MSG_WAITALL);
	if(ret < 1)
	{
		if(ret < 0)
			perror(error_message);
			
		return ret;
	}
	
	//allocazione dello spazio necessario a conetenere il payload
	if(buffer->length == 0)
		return 2;
	else
		buffer->payload = (char *) calloc(buffer->length, sizeof(char));
	
	//ricezione del payload del pacchetto
	ret = recv(socket, (void *) buffer->payload, buffer->length, 0);
	if(ret < buffer->length)
	{
		if(ret != 0)
			perror(error_message);
			
		return ret;
	}
	
	//restituisco il numero di byte letti sul socket
	return ret+2;
}
