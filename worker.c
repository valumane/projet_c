#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#include "myassert.h"
#include "master_worker.h"

/************************************************************************
 * Usage et analyse des arguments passés en ligne de commande
 ************************************************************************/

static void usage(const char *exeName, const char *message) {
  fprintf(stderr, "usage : %s <n> <fdIn> <fdToMaster>\n", exeName);
  fprintf(stderr, "   <n> : nombre premier géré par le worker\n");
  fprintf(stderr, "   <fdIn> : canal d'entrée pour tester un nombre\n");
  fprintf(stderr,
          "   <fdToMaster> : canal de sortie pour indiquer si un nombre est "
          "premier ou non\n");
  if (message != NULL) fprintf(stderr, "message : %s\n", message);
  exit(EXIT_FAILURE);
}

static void parseArgs(int argc, char *argv[]) {
  if (argc != 4) usage(argv[0], "Nombre d'arguments incorrect");
}

/* =====================================================================
 * Boucle principale d’un worker du crible Hoare
 * ===================================================================== */
void loop(int fdRead, int *hasNext, int nextPipe[2], int fdWriteMaster,
          int myPrime, int *nextPid) {
  while (1) {
    int n = 0;
    int r = read(fdRead, &n, sizeof(int));
    myassert(r != -1, "[WORKER] read(fdRead) a échoué");

    if (r == 0) break;

    /* --- CAS STOP --- */
     if (n == -1) {
      if (*hasNext) {
        int w = write(nextPipe[1], &n, sizeof(int));
        myassert(w == (int)sizeof(int),"[WORKER] write STOP vers le worker suivant a échoué");

        myassert(close(nextPipe[1]) != -1,"[WORKER] close(nextPipe[1]) a échoué");
        waitpid(*nextPid, NULL, 0);
        *hasNext = 0;  // déjà nettoyé, rien à refaire après la boucle
      }

      printf("[WORKER %d] reçoit STOP\n", myPrime);
      break;
    }

    /* --- CAS N == PRIME : SUCCES --- */
    if (n == myPrime) {
      int w = write(fdWriteMaster, &n, sizeof(int));
      myassert(w == (int)sizeof(int),"[WORKER] write(n==myPrime) vers master a échoué");
      continue;
    }

    /* --- CAS divisible : ECHEC --- */
    if (n % myPrime == 0) {
      int fail = 0;
      int w = write(fdWriteMaster, &fail, sizeof(int));
      myassert(w == (int)sizeof(int),"[WORKER] write(fail) vers master a échoué");
      continue;
    }

    /* --- CAS N NON DIVISIBLE : transmettre au suivant --- */
        if (!(*hasNext)) {
      *hasNext = 1;
      myassert(pipe(nextPipe) == 0, "[WORKER] pipe(nextPipe) a échoué");

      *nextPid = fork();
      myassert(*nextPid != -1, "[WORKER] fork() pour le worker suivant a échoué");

      if (*nextPid == 0) {
        myassert(close(nextPipe[1]) != -1, "[WORKER] close(nextPipe[1]) (fils) a échoué");

        char fdReadStr[10], fdWriteStr[10], primeStr[10];
        snprintf(fdReadStr, sizeof(fdReadStr), "%d", nextPipe[0]);
        snprintf(fdWriteStr, sizeof(fdWriteStr), "%d", fdWriteMaster);
        snprintf(primeStr, sizeof(primeStr), "%d", n);

        char *args[] = {"worker", fdReadStr, fdWriteStr, primeStr, NULL};
        execv("./worker", args);
        perror("execv");
        exit(EXIT_FAILURE);
      }

      myassert(close(nextPipe[0]) != -1, "[WORKER] close(nextPipe[0]) (père) a échoué");
      printf("[WORKER %d] a créé worker %d (pid=%d)\n", myPrime, n, *nextPid);

    } else {
      int w = write(nextPipe[1], &n, sizeof(int));
      myassert(w == (int)sizeof(int), "[WORKER] write vers worker suivant a échoué");
    }

  }
}

/* ======================================================================
 *  MAIN
 * ===================================================================== */
int main(int argc, char *argv[]) {
  parseArgs(argc, argv);

  int fdRead = atoi(argv[1]);
  int fdWriteMaster = atoi(argv[2]);
  int myPrime = atoi(argv[3]);

  printf("[WORKER] (pid=%d) : gère %d\n", getpid(), myPrime);

    /* au démarrage, un worker renvoie immédiatement son premier au master */
  int w = write(fdWriteMaster, &myPrime, sizeof(myPrime));
  myassert(w == (int)sizeof(myPrime), "[WORKER] write premier myPrime vers master a échoué");

  int nextPipe[2] = {-1, -1};
  int nextPid = -1;
  int hasNext = 0;

  loop(fdRead, &hasNext, nextPipe, fdWriteMaster, myPrime, &nextPid);

  if (hasNext) {
    myassert(close(nextPipe[1]) != -1, "[WORKER] close(nextPipe[1]) final a échoué");
    waitpid(nextPid, NULL, 0);
  }

  printf("[WORKER %d] (pid=%d) : terminé\n", myPrime, getpid());

  return EXIT_SUCCESS;
}
