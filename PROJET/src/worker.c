#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

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

    if (r == 0) break;

    /* --- CAS STOP --- */
    if (n == -1) {
      if (*hasNext) {
        write(nextPipe[1], &n, sizeof(int));
        close(nextPipe[1]);
        waitpid(*nextPid, NULL, 0);
      }

      printf("[WORKER %d] reçoit STOP\n", myPrime);
      break;
    }

    /* --- CAS N == PRIME : SUCCES --- */
    if (n == myPrime) {
      write(fdWriteMaster, &n, sizeof(int));
      continue;
    }

    /* --- CAS divisible : ECHEC --- */
    if (n % myPrime == 0) {
      int fail = 0;
      write(fdWriteMaster, &fail, sizeof(int));
      continue;
    }

    /* --- CAS N NON DIVISIBLE : transmettre au suivant --- */
    if (!(*hasNext)) {
      *hasNext = 1;
      assert(pipe(nextPipe) == 0);

      *nextPid = fork();
      assert(*nextPid != -1);

      if (*nextPid == 0) {
        close(nextPipe[1]);

        char fdReadStr[10], fdWriteStr[10], primeStr[10];
        snprintf(fdReadStr, sizeof(fdReadStr), "%d", nextPipe[0]);
        snprintf(fdWriteStr, sizeof(fdWriteStr), "%d", fdWriteMaster);
        snprintf(primeStr, sizeof(primeStr), "%d", n);

        char *args[] = {"worker.o", fdReadStr, fdWriteStr, primeStr, NULL};
        execv("./worker.o", args);
        perror("execv");
        exit(EXIT_FAILURE);
      }

      close(nextPipe[0]);
      printf("[WORKER %d] a créé worker %d (pid=%d)\n", myPrime, n, *nextPid);

    } else {
      write(nextPipe[1], &n, sizeof(int));
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
  write(fdWriteMaster, &myPrime, sizeof(myPrime));

  int nextPipe[2] = {-1, -1};
  int nextPid = -1;
  int hasNext = 0;

  loop(fdRead, &hasNext, nextPipe, fdWriteMaster, myPrime, &nextPid);

  if (hasNext) {
    close(nextPipe[1]);
    waitpid(nextPid, NULL, 0);
  }

  printf("[WORKER %d] (pid=%d) : terminé\n", myPrime, getpid());

  return EXIT_SUCCESS;
}
