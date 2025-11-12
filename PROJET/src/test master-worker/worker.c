#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// ajout fcntl.h pour les opérations sur les fichiers
#include <fcntl.h>
// ajout unistd.h pour les opérations POSIX (read, write, close,
#include <unistd.h>

int main(int argc, char* argv[]) {
	printf("\n---------------------\n");
	printf("Debut worker.c main\n");

	printf("Worker démarré, pid : %d, pere : %d\n", getpid(), getppid());
	printf("nombre recu en argument : %s\n", argv[1]);

	int NumberToTest = atoi(argv[1]);
	int tag = 2;

	// j'ouvre le tube en ecriture
	int fdWriteToMaster = open("master-worker", O_WRONLY);
	int ifPrime = 1;
	int ifNotPrime = 0;
	int ifElse = 2;

	if (NumberToTest == 2) {
		printf("%d est premier\n", NumberToTest);
		write(fdWriteToMaster,&ifPrime,sizeof(int));
		
	} else if (NumberToTest % 2 == 0) {
		printf("%d pas premier\n", NumberToTest);
		write(fdWriteToMaster,&ifNotPrime,sizeof(int));
	} else {
		printf("on envoie plus loin\n");
		write(fdWriteToMaster,&ifElse,sizeof(int));
	}

	printf("Worker termine\n");
	printf("---------------------\n");

	return EXIT_SUCCESS;
}
