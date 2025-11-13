#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "myassert.h"

#include "master_worker.h"

/* include ajouté */
  #include <assert.h>
  #include <fcntl.h>
  #include <sys/wait.h>
  #include <unistd.h>
/*****************/



/************************************************************************
 * Données persistantes d'un worker
 ************************************************************************/

// on peut ici définir une structure stockant tout ce dont le worker
// a besoin : le nombre premier dont il a la charge, ...


/************************************************************************
 * Usage et analyse des arguments passés en ligne de commande
 ************************************************************************/

static void usage(const char *exeName, const char *message){
    fprintf(stderr, "usage : %s <n> <fdIn> <fdToMaster>\n", exeName);
    fprintf(stderr, "   <n> : nombre premier géré par le worker\n");
    fprintf(stderr, "   <fdIn> : canal d'entrée pour tester un nombre\n");
    fprintf(stderr, "   <fdToMaster> : canal de sortie pour indiquer si un nombre est premier ou non\n");
    if (message != NULL)
        fprintf(stderr, "message : %s\n", message);
    exit(EXIT_FAILURE);
}

static void parseArgs(int argc, char * argv[] /*, structure à remplir*/){
    if (argc != 4)
        usage(argv[0], "Nombre d'arguments incorrect");

    // remplir la structure
}

/************************************************************************
 * Boucle principale de traitement
 ************************************************************************/

void loop(int fdRead,int hasNext, int nextPipe[2], int fdWriteMaster, int myPrime, pid_t nextPid) {
     /*boucle infinie :
        attendre l'arrivée d'un nombre à tester
        si ordre d'arrêt
           si il y a un worker suivant, transmettre l'ordre et attendre sa fin
           sortir de la boucle
        sinon c'est un nombre à tester, 4 possibilités :
               - le nombre est premier
               - le nombre n'est pas premier
               - s'il y a un worker suivant lui transmettre le nombre
               - s'il n'y a pas de worker suivant, le créer */
               
    while (1) {
    int n;
    int r = read(fdRead, &n, sizeof(int));
    if (r == 0) break;  // fin du pipeline (EOF)

    // cas d'arret
    if (n == -1) {
      if (hasNext) {
        // propager l'arret au worker suivant
        write(nextPipe[1], &n, sizeof(int));  // propager l'arret
        close(nextPipe[1]);
        waitpid(nextPid, NULL, 0);
      }
      printf("[WORKER %d] reçoit l'ordre d'arrêt, fin.\n", myPrime);
      break;
    }

    if (n % myPrime == 0) {
      // pas premier : on ignore
    } else {
      if (!hasNext) {
        // ce nombre est un nouveau premier -> on crée un nouveau worker
        hasNext = 1;
        assert(pipe(nextPipe) == 0);
        nextPid = fork();
        assert(nextPid != -1);

        if (nextPid == 0) { // processus du worker suivant
          // fermer les descripteurs inutiles
          close(nextPipe[1]);
          char fdReadStr[10], fdWriteStr[10], primeStr[10]; // écriture vers le master
          snprintf(fdReadStr, sizeof(fdReadStr), "%d", nextPipe[0]); // lecture du master
          snprintf(fdWriteStr, sizeof(fdWriteStr), "%d", fdWriteMaster); // écriture vers le master
          snprintf(primeStr, sizeof(primeStr), "%d", n); // nouveau premier
          char *args[] = {"worker", fdReadStr, fdWriteStr, primeStr, NULL}; // exécuter le worker
          execv("./worker", args);
          perror("execv");
          exit(EXIT_FAILURE);
        }

        // processus du worker courant
        close(nextPipe[0]);
        write(fdWriteMaster, &n, sizeof(int));
        printf("[WORKER %d] : a créé worker %d (pid=%d)\n", myPrime, n, nextPid);
      } else { // transmettre au worker suivant
        write(nextPipe[1], &n, sizeof(int));
      }
    }
  }


}

/************************************************************************
 * Programme principal
 ************************************************************************/


int main(int argc, char *argv[]) {
  assert(argc == 4);

  // récupérer les arguments
  int fdRead = atoi(argv[1]);
  int fdWriteMaster = atoi(argv[2]);
  int myPrime = atoi(argv[3]);

  printf("[WORKER] (pid=%d) : gère %d\n", getpid(), myPrime);

  // pipe vers le worker suivant
  int nextPipe[2] = {-1, -1};
  pid_t nextPid = -1;
  int hasNext = 0;

  // boucle principale du worker
  loop(fdRead, hasNext, nextPipe, fdWriteMaster, myPrime, nextPid);
  // nettoyage
  if (hasNext) {
    close(nextPipe[1]);
    waitpid(nextPid, NULL, 0);
  }

  printf("[WORKER %d] (pid=%d) : terminé\n", myPrime, getpid());
  return EXIT_SUCCESS;
}
