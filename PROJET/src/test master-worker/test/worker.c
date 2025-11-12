#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  assert(argc == 4);

  int fdRead = atoi(argv[1]);
  int fdWriteMaster = atoi(argv[2]);
  int myPrime = atoi(argv[3]);

  printf("[WORKER] (pid=%d) : gère %d\n", getpid(), myPrime);

  int nextPipe[2] = {-1, -1};
  pid_t nextPid = -1;
  int hasNext = 0;

  while (1) {
    int n;
    ssize_t r = read(fdRead, &n, sizeof(int));
    if (r == 0) break;  // fin du pipeline (EOF)

    // cas d'arret
    if (n == -1) {
      if (hasNext) {
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

        if (nextPid == 0) {
          close(nextPipe[1]);
          char fdReadStr[10], fdWriteStr[10], primeStr[10];
          snprintf(fdReadStr, sizeof(fdReadStr), "%d", nextPipe[0]);
          snprintf(fdWriteStr, sizeof(fdWriteStr), "%d", fdWriteMaster);
          snprintf(primeStr, sizeof(primeStr), "%d", n);
          char *args[] = {"worker", fdReadStr, fdWriteStr, primeStr, NULL};
          execv("./worker", args);
          perror("execv");
          exit(EXIT_FAILURE);
        }

        close(nextPipe[0]);
        write(fdWriteMaster, &n, sizeof(int));
        printf("[WORKER %d] : a créé worker %d (pid=%d)\n", myPrime, n,
               nextPid);
      } else {
        write(nextPipe[1], &n, sizeof(int));
      }
    }
  }

  if (hasNext) {
    close(nextPipe[1]);
    waitpid(nextPid, NULL, 0);
  }

  printf("[WORKER %d] (pid=%d) : terminé\n", myPrime, getpid());
  return EXIT_SUCCESS;
}
