#include "tris_lib.h"

void flush()
{
	while(getchar()!='\n');
}

void sendPacket(int socket, packet* buffer, const char* message)
{
	char* send_buffer;
	send_buffer = (char *) malloc((buffer->length+2)*sizeof(char));
	
	//costruzione del pacchetto da inviare
	send_buffer[0] = buffer->type;
	send_buffer[1] = buffer->length;
	strcpy(&send_buffer[2], buffer->payload);
	
	if(send(socket, (void *) send_buffer, buffer->length+2, 0) == -1)
	{
		perror(message);
		exit(EXIT_FAILURE);
	}
}

packet recvPacket(int socket, const char* message)
{
	packet recv_buffer;
	
	//ricezione dell'header del pacchetto
	if(recv(socket, (void *) &recv_buffer, 2, 0) == -1)
	{
		perror(message);
		exit(EXIT_FAILURE);
	}
	
	//allocazione dello spazio necessario a conetenere il payload
	recv_buffer.payload = (char *) malloc((recv_buffer.length)*sizeof(char));
	
	if(recv(socket, (void *) recv_buffer.payload, 2, 0) == -1)
	{
		perror(message);
		exit(EXIT_FAILURE);
	}
	
	return recv_buffer;
}

