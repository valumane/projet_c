#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

const char *fifoClientToMaster = "client_to_master.fifo";
const char *fifoMasterToClient = "master_to_client.fifo";

int last_tested = 2;
int highest_prime = 2;

// rend un pipe non bloquant
void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main(void) {
    printf("[MASTER] Démarrage du master\n");

    mkfifo(fifoClientToMaster, 0666);
    mkfifo(fifoMasterToClient, 0666);

    int pipeMW[2];
    int pipeWM[2];
    assert(pipe(pipeMW) == 0);
    assert(pipe(pipeWM) == 0);

    pid_t pid = fork();
    assert(pid != -1);

    if (pid == 0) {
        close(pipeMW[1]);
        close(pipeWM[0]);
        char fdReadStr[10], fdWriteStr[10], primeStr[10];
        snprintf(fdReadStr, sizeof(fdReadStr), "%d", pipeMW[0]);
        snprintf(fdWriteStr, sizeof(fdWriteStr), "%d", pipeWM[1]);
        snprintf(primeStr, sizeof(primeStr), "%d", 2);
        char *args[] = {"worker", fdReadStr, fdWriteStr, primeStr, NULL};
        execv("./worker", args);
        perror("execv");
        exit(EXIT_FAILURE);
    }

    close(pipeMW[0]);
    close(pipeWM[1]);
    set_nonblocking(pipeWM[0]); // lecture non bloquante

    while (1) {
        int fdClient = open(fifoClientToMaster, O_RDONLY);
        if (fdClient == -1) continue;

        int nombre;
        ssize_t n = read(fdClient, &nombre, sizeof(int));
        close(fdClient);

        if (n == 0) continue;
        if (nombre == -1) break;

        printf("[MASTER] Nombre reçu du client : %d\n", nombre);

        // envoi du nombre
        if (nombre > last_tested) {
            for (int i = last_tested + 1; i <= nombre; i++)
                write(pipeMW[1], &i, sizeof(int));
            last_tested = nombre;
        } else {
            write(pipeMW[1], &nombre, sizeof(int));
        }

        // lecture non bloquante des nouveaux premiers
        int prime;
        while (read(pipeWM[0], &prime, sizeof(int)) > 0) {
            highest_prime = prime;
            printf("[MASTER] Nouveau nombre premier trouvé : %d\n", prime);
        }

        // verif si le nombre est premier
        int isPrime = 1;
        for (int i = 2; i * i <= nombre; i++) {
            if (nombre % i == 0) { isPrime = 0; break; }
        }

        // envoi du résultat au client
        int fdRetour = open(fifoMasterToClient, O_WRONLY);
        if (fdRetour != -1) {
            write(fdRetour, &isPrime, sizeof(int));
            close(fdRetour);
        }

        printf("[MASTER] Résultat envoyé au client : %d (%s)\n",
               isPrime, isPrime ? "premier" : "non premier");
    }

    close(pipeMW[1]);
    close(pipeWM[0]);
    unlink(fifoClientToMaster);
    unlink(fifoMasterToClient);
    printf("[MASTER] Fermeture propre du master.\n");
    return EXIT_SUCCESS;
}
