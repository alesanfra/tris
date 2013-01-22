#include "tris_lib.h"

int main(int argc, char* argv[])
{
	int sk;
	struct sockaddr_in address;
	char buffer;
	
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

	do{
		printf("Digita un numero: ");
		scanf("%hhi",&buffer);
		sendto(sk,&buffer,1,0,(struct sockaddr*) &address,sizeof(address));
	} while(buffer != 0);
	
	close(sk);
	
	return 0;
}
