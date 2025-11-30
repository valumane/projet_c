#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#define _XOPEN_SOURCE

#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/stat.h>  // <-- pour mkfifo()
#include <unistd.h>    // <-- pour close() et unlink()

#include "master_client.h"

// sema
void P(int semid) {
  struct sembuf op = {0, -1, 0};
  if (semop(semid, &op, 1) == -1) {
    perror("semop P");
    assert(0);
  }
}

void V(int semid) {
  struct sembuf op = {0, +1, 0};
  if (semop(semid, &op, 1) == -1) {
    perror("semop V");
    assert(0);
  }
}

void resetSemaphore(int sem1, int sem2) {
  semctl(sem1, 0, IPC_RMID);
  semctl(sem2, 0, IPC_RMID);
}

// fonctions éventuelles internes au fichier
void createFifos() {
  mkfifo(FIFO_CLIENT_TO_MASTER, 0666);
  mkfifo(FIFO_MASTER_TO_CLIENT, 0666);
}

void closePipes(int pipe1, int pipe2) {
  close(pipe1);
  close(pipe2);
}

void unlinkPipes() {
  unlink(FIFO_CLIENT_TO_MASTER);  // supprimer la FIFO client->master
  unlink(FIFO_MASTER_TO_CLIENT);  // supprimer la FIFO master->client
}

/* client */
void clientInterpretOrder(int order, int number, int resultat) {
  switch (order) {
    case ORDER_COMPUTE_PRIME:
      if (resultat)
        printf("[CLIENT] %d est premier\n", number);
      else
        printf("[CLIENT] %d n'est pas premier\n", number);
      break;

    case ORDER_HOW_MANY_PRIME:
      printf("[CLIENT] %d nombres premiers ont été trouvés\n", resultat);
      break;

    case ORDER_HIGHEST_PRIME:
      printf("[CLIENT] Le plus grand nombre premier trouvé est %d\n", resultat);
      break;

    case ORDER_STOP:
      printf("[CLIENT] Master arrêté (code retour %d)\n", resultat);
      break;

    default:
      printf("[CLIENT] Ordre inconnu (%d), résultat brut = %d\n", order,
             resultat);
      break;
  }
}

void clientSendOrder(int fdOut, int order, int number) {
  // Envoi ordre
  if (write(fdOut, &order, sizeof(order)) != sizeof(order)) {
    perror("[CLIENT] write ordre");
    return;
  }

  // Envoi paramètre si nécessaire
  if (order == ORDER_COMPUTE_PRIME) {
    if (write(fdOut, &number, sizeof(number)) != sizeof(number)) {
      perror("[CLIENT] write number");
      return;
    }
    printf("[CLIENT] Envoi de l'ordre COMPUTE pour %d au master\n", number);
  }

  else if (order == ORDER_STOP) {
    printf("[CLIENT] Envoi de l'ordre STOP au master\n");
  }

  else if (order == ORDER_HOW_MANY_PRIME) {
    printf("[CLIENT] Envoi HOW_MANY au master\n");
  }

  else if (order == ORDER_HIGHEST_PRIME) {
    printf("[CLIENT] Envoi HIGHEST au master\n");
  }
}

int mysqrt(int n) {
  if (n <= 0) return 0;
  int x = n;
  int y = (x + 1) / 2;

  while (y < x) {
    x = y;
    y = (x + n / x) / 2;
  }
  return x;  // valeur entière de sqrt(n)
}

typedef struct {
  bool *tab;
  int limit;
  int div;
  pthread_mutex_t *mutex;
} ThreadArgs;

void *threadMark(void *arg) {
  ThreadArgs *a = (ThreadArgs *)arg;
  int d = a->div;
  int N = a->limit;

  for (int k = 2 * d; k <= N; k += d) {
    pthread_mutex_lock(a->mutex);
    a->tab[k] = false;
    pthread_mutex_unlock(a->mutex);
  }

  return NULL;
}

void mode_local(int number) {
  // =======================
  // 1) tab booléens
  // =======================

  bool *tab = malloc(sizeof(bool) * (number + 1));

  for (int i = 2; i <= number; i++) {
    tab[i] = true;
  }

  // =======================
  // 2) preparer threads
  // =======================

  pthread_mutex_t mutex;
  pthread_mutex_init(&mutex, NULL);

  int max = mysqrt(number);
  int nbThreads = max - 1;  // pour diviseurs = 2...max

  pthread_t *tids = malloc(sizeof(pthread_t) * nbThreads);
  ThreadArgs *args = malloc(sizeof(ThreadArgs) * nbThreads);

  // =======================
  // 3) create threads
  // =======================

  for (int i = 0; i < nbThreads; i++) {
    args[i].tab = tab;
    args[i].limit = number;
    args[i].div = i + 2;  // diviseur = 2, 3, 4...
    args[i].mutex = &mutex;

    pthread_create(&tids[i], NULL, threadMark, &args[i]);
  }

  // =======================
  // 4) join les threads
  // =======================

  for (int i = 0; i < nbThreads; i++) {
    pthread_join(tids[i], NULL);
  }

  pthread_mutex_destroy(&mutex);

  // =======================
  // 5) print nombres premiers
  // =======================

  printf("Premiers trouvés jusqu'à %d :\n", number);
  for (int i = 2; i <= number; i++) {
    if (tab[i]) {
      printf("%d ", i);
    }
  }

  printf("\n");

  free(tab);
  free(tids);
  free(args);
}

void show_worker(int argc, char *argv[]) {
  /* ======= MODE SPÉCIAL : ./client showworker ======= */
  if (argc == 2 && strcmp(argv[1], "showworker") == 0) {
    fprintf(stdout, "ps -C worker.o -o stat,cmd | grep -E 'R|S|D|I'\n");
    system("ps -C worker.o -o stat,cmd | grep -E 'R|S|D|I'");
  }
  /* ================================================== */
}