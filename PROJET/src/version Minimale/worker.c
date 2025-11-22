#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "master_worker.h"

/************************************************************************
 * Données persistantes d'un worker
 ************************************************************************/

// on peut ici définir une structure stockant tout ce dont le worker
// a besoin : le nombre premier dont il a la charge, ...

/************************************************************************
 * Fonctions utilitaires
 ************************************************************************/

static void usage(const char *exeName, const char *message) {
  fprintf(stderr, "usage : %s <n> <fdIn> <fdToMaster>\n", exeName);
  fprintf(stderr, "   <n> : nombre premier géré par le worker\n");
  fprintf(stderr, "   <fdIn> : canal d'entrée pour tester un nombre\n");
  fprintf(stderr,
          "   <fdToMaster> : canal de sortie pour indiquer si un nombre est "
          "premier ou non\n");
  if (message != NULL) {
    fprintf(stderr, "message : %s\n", message);
  }
  exit(EXIT_FAILURE);
}

static void parseArgs(int argc, char *argv[] /*, structure à remplir*/) {
  if (argc != 4) {
    usage(argv[0], "Nombre d'arguments incorrect");
  }

  // remplir la structure
}

/************************************************************************
 * Boucle principale du worker (crible de Hoare)
 ************************************************************************/

void loop(int fdRead, int *hasNext, int nextPipe[2], int fdWriteMaster,
          int myPrime, pid_t *nextPid) {
  while (1) {
    int n;
    int r = read(fdRead, &n, sizeof(int));

    if (r == 0) break;       // EOF = fin du pipeline
    if (r != sizeof(int)) {  // lecture invalide
      perror("[WORKER] read");
      break;
    }

    // ---- CAS STOP ----
    if (n == -1) {
      if (*hasNext) {
        write(nextPipe[1], &n, sizeof(int));
        close(nextPipe[1]);
        waitpid(*nextPid, NULL, 0);
      }
      printf("[WORKER %d] reçoit STOP\n", myPrime);
      break;
    }

    // ---- CAS (N == P) -> SUCCÈS ----
    if (n == myPrime) {
      write(fdWriteMaster, &n, sizeof(int));  // succès
      continue;
    }

    // ---- CAS (P divise N) -> ÉCHEC ----
    if (n % myPrime == 0) {
      int fail = 0;  // 0 = NON PREMIER
      write(fdWriteMaster, &fail, sizeof(int));
      continue;
    }

    // ---- CAS N NON DIVISIBLE -> envoyer au suivant ----
    if (!(*hasNext)) {
      // ---- Créer un nouveau worker (prime = N) ----
      *hasNext = 1;
      assert(pipe(nextPipe) == 0);

      *nextPid = fork();
      assert(*nextPid != -1);

      if (*nextPid == 0) {
        // ---- processus enfant = nouveau worker ----

        close(nextPipe[1]);  // ferme écriture parent

        char fdReadStr[10], fdWriteStr[10], primeStr[10];
        snprintf(fdReadStr, sizeof(fdReadStr), "%d", nextPipe[0]);
        snprintf(fdWriteStr, sizeof(fdWriteStr), "%d", fdWriteMaster);
        snprintf(primeStr, sizeof(primeStr), "%d", n);

        char *args[] = {"worker.o", fdReadStr, fdWriteStr, primeStr, NULL};
        execv("./worker.o", args);
        perror("execv");
        exit(EXIT_FAILURE);
      }

      // ---- processus parent ----
      close(nextPipe[0]);

      // IMPORTANT :
      // Le nouveau worker va automatiquement envoyer N au master
      // (donc ici le parent NE DOIT RIEN envoyer au master)
      printf("[WORKER %d] a créé worker %d (pid=%d)\n", myPrime, n, *nextPid);

    } else {
      // ---- Transmettre au worker suivant ----
      write(nextPipe[1], &n, sizeof(int));
    }
  }
}

/************************************************************************
 * Programme principal
 ************************************************************************/

int main(int argc, char *argv[]) {
  parseArgs(argc, argv);

  int fdRead = atoi(argv[1]);
  int fdWriteMaster = atoi(argv[2]);
  int myPrime = atoi(argv[3]);

  printf("[WORKER] (pid=%d) : gère %d\n", getpid(), myPrime);

  // IMPORTANT :
  // Un worker qui démarre DOIT envoyer son prime au master : SUCCESS
  write(fdWriteMaster, &myPrime, sizeof(myPrime));

  int nextPipe[2] = {-1, -1};
  pid_t nextPid = -1;
  int hasNext = 0;

  loop(fdRead, &hasNext, nextPipe, fdWriteMaster, myPrime, &nextPid);

  if (hasNext) {
    close(nextPipe[1]);
    waitpid(nextPid, NULL, 0);
  }

  printf("[WORKER %d] (pid=%d) : terminé\n", myPrime, getpid());
  return EXIT_SUCCESS;
}
