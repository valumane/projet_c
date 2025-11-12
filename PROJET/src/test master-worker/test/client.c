#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>

const char *fifoClientToMaster = "client_to_master.fifo";
const char *fifoMasterToClient = "master_to_client.fifo";

int main(int argc, char * argv[]){
    assert(argc > 1);

    int fdWrite = open(fifoClientToMaster, O_WRONLY);
    assert(fdWrite != -1);

    int nombre = atoi(argv[1]);
    write(fdWrite, &nombre, sizeof(int));
    fprintf(stdout,"[CLIENT] Envoi du nombre %d au master\n", nombre);
    close(fdWrite);

    // === Réception de la réponse du master ===
    int fdRead = open(fifoMasterToClient, O_RDONLY);
    assert(fdRead != -1);

    int resultat;
    ssize_t n = read(fdRead, &resultat, sizeof(int));
    close(fdRead);

    if (n == sizeof(int)) {
        if (resultat == 1)
            printf("[CLIENT] %d est premier\n", nombre);
        else
            printf("[CLIENT] %d n'est pas premier\n", nombre);
    } else {
        printf("[CLIENT] Aucune réponse du master.\n");
    }

    return EXIT_SUCCESS;
}
