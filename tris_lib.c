#include "tris_lib.h"

void flush()
{
	while(getchar()!='\n');
}

void sendPacket(int socket, packet* buffer, const char* error_message)
{
	char* send_buffer;
	send_buffer = (char *) malloc((buffer->length+2)*sizeof(char));
	
	//costruzione del pacchetto da inviare
	send_buffer[0] = buffer->type;
	send_buffer[1] = buffer->length;
	strcpy(&send_buffer[2], buffer->payload);
	
	if(send(socket, (void *) send_buffer, buffer->length+2, 0) == -1)
	{
		perror(error_message);
		exit(EXIT_FAILURE);
	}
}

void recvPacket(int socket, packet* buffer, const char* error_message)
{
	//azzero la struttura dati
	memset(buffer,0,sizeof(buffer));
	
	//ricezione dell'header del pacchetto
	if(recv(socket, (void *) buffer, 2, MSG_WAITALL) == -1)
	{
		perror(error_message);
		exit(EXIT_FAILURE);
	}
	
	//allocazione dello spazio necessario a conetenere il payload
	buffer->payload = (char *) malloc((buffer->length)*sizeof(char));
	
	if(recv(socket, (void *) buffer->payload, buffer->length, MSG_WAITALL) == -1)
	{
		perror(error_message);
		exit(EXIT_FAILURE);
	}
}

