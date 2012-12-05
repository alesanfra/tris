#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#define SA struct sockaddr

int main(int argc, char* argv[])
{
	//dichiarazioni delle variabili
	struct sockaddr_in server_addr;
	int ret, server;
	char *request = "GET_TIME";
	char msg[1024], nickname[32];
	char ip[16];
	int porta;
	
	//controllo numero argomenti passati
	if(argc != 3){
		printf("Usage: tris_client <host remoto> <porta>\n");
		exit(EXIT_FAILURE);
	}

	printf("TRIS CLIENT\n\n");

	//Creazione del socket TCP
	if((server = socket(PF_INET, SOCK_STREAM, 0)) == -1){
		perror("Errore nella creazione del socket TCP");
		exit(EXIT_FAILURE);
	}

	//Copia dell'indirizzo passato nella variabile ip
	if(strcmp(argv[1], "localhost") == 0)
		strcpy(ip, "127.0.0.1");
	else
		strcpy(ip, argv[1]);
	
	//Azzeramento della struttura dati sockaddr_in
	memset(&server_addr, 0, sizeof(struct sockaddr_in));
	
	//Inserimento in sockaddr_in dei paramentri
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(atoi(argv[2]));
	inet_pton(AF_INET, ip, &server_addr.sin_addr.s_addr);
	
	//Connessione al server
	if(connect(server, (struct sockaddr*)&server_addr, sizeof(struct sockaddr_in)) == -1){
		perror("errore nella connessione al server");
		exit(EXIT_FAILURE);
	}
	//connessione avvenuta, richiesta ed invio di nome e porta
	printf("Connessione al server %s:%s effettuata con successo\n\n", argv[1], argv[2]);
	
	//Inserimento nickname con controllo
	do {
		printf("Inserisci il tuo nikname (max 31 caratteri): ");
		scanf("%s",nickname);
		if(send())
}
