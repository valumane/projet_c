#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <unistd.h>

#include "master_client.h"
#include "master_worker.h"
#include "myassert.h"

// fonctions éventuelles internes au fichier

// fonctions éventuelles proposées dans le .h
void init_semaphores(int *sem_mutex, int *sem_sync) {
  int key_mutex = ftok("master.c", 'M');
  int key_sync = ftok("master.c", 'S');

  printf("key_mutex = 0x%x\n", key_mutex);
  printf("key_sync  = 0x%x\n", key_sync);

  myassert(key_mutex != -1, "ftok(\"master.c\", 'M') a échoué");
  myassert(key_sync != -1, "ftok(\"master.c\", 'S') a échoué");

  *sem_mutex = semget(key_mutex, 1, IPC_CREAT | 0641);
  *sem_sync = semget(key_sync, 1, IPC_CREAT | 0641);

  myassert(*sem_mutex != -1, "semget(sem_mutex) a échoué");
  myassert(*sem_sync != -1, "semget(sem_sync) a échoué");

  myassert(semctl(*sem_mutex, 0, SETVAL, 1) != -1,
           "semctl(sem_mutex, SETVAL) a échoué");
  myassert(semctl(*sem_sync, 0, SETVAL, 0) != -1,
           "semctl(sem_sync, SETVAL) a échoué");
}

// ...

void launch_worker(int prime, int pipeMW[2], int pipeWM[2]) {
  // On est dans le *processus fils* ici

  myassert(close(pipeMW[1]) != -1, "close(pipe1) a échoué");
  myassert(close(pipeWM[0]) != -1, "close(pipe2) a échoué");

  char rStr[16], wStr[16], pStr[16];

  int n = snprintf(rStr, sizeof(rStr), "%d", pipeMW[0]);
  myassert(n > 0 && n < (int)sizeof(rStr), "snprintf rStr a échoué");

  n = snprintf(wStr, sizeof(wStr), "%d", pipeWM[1]);
  myassert(n > 0 && n < (int)sizeof(wStr), "snprintf wStr a échoué");

  n = snprintf(pStr, sizeof(pStr), "%d", prime);
  myassert(n > 0 && n < (int)sizeof(pStr), "snprintf pStr a échoué");

  char *args[] = {"worker", rStr, wStr, pStr, NULL};

  execv("./worker", args);
  // Si on arrive ici, execv a échoué
  perror("execv worker");
  exit(EXIT_FAILURE);
}
