#include "tris_lib.h"

int main(int argc,char* argv[])
{
	//allocazione delle strutture dati necessarie
	struct sockaddr_in server_addr, my_addr, cl_addr;
	int len, sk, cn_sk;
	char ip[16];
	
	//controllo numero argomenti passati
	if(argc != 3){
		printf("Usage: tris_client <host remoto> <porta>\n");
		exit(EXIT_FAILURE);
	}
	
	//Verbose
	printf("Tris Server!\n");
	
	//creazione del socket di ascolto
	sk = socket(AF_INET, SOCK_STREAM, 0);
	
	if(sk == -1)
	{
		perror("Errore nella creazione del socket");
		exit(EXIT_FAILURE);
	}
	
	//Copia dell'indirizzo passato nella variabile ip
	if(strcmp(argv[1], "localhost") == 0)
		strcpy(ip, "127.0.0.1");
	else
		strcpy(ip, argv[1]);
	
	//inizializzazione delle strutture dati
	memset(&my_addr, 0, sizeof(my_addr));
	my_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(atoi(argv[2]));
	inet_pton(AF_INET, ip, &server_addr.sin_addr.s_addr);
	
	//bind del socket	
	if(bind(sk, (SA *) &my_addr, sizeof(my_addr)) == -1)
	{
		perror("Errore nell'esecuzione della bind del socket");
		exit(EXIT_FAILURE);
	}
	
	//messa in ascolto del server sul socket
	if(listen(sk, 25) == -1)
	{
		perror("Errore nella messa in ascolto del server");
		exit(EXIT_FAILURE);
	}
	
	//dimensioni della struttura dove viene salvato l'ind del client
	len = sizeof(cl_addr);
	
	//ciclo in cui il server accetta connessioni in ingresso e le gestisce
	for(;;)
	{
		//accettazione delle connessioni ingresso
		cn_sk = accept(sk, (SA *) &cl_addr, (socklen_t *) &len);
		
		if(cn_sk == -1)
		{
			perror("Errore nell'accettazione di una richiesta");
			return 0;
		}
		
		//gestione delle richieste
		//replyToClient(cn_sk);
	}
	
	//Chiusura del socket di ascolto
	//Questo codice non verrà mai eseguito dato che si trova
	//dopo un ciclo infinito
	
	printf("\nChiusura del server\n");
	close(sk);
	return 0;	
}

