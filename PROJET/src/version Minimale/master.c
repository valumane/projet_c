#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "master_client.h"
#include "master_worker.h"

/* includes ajoutés */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <unistd.h>
/******************/

/************************************************************************
 * Usage et analyse des arguments passés en ligne de commande
 ************************************************************************/
static void usage(const char *exeName, const char *message) {
  fprintf(stderr, "usage : %s\n", exeName);
  if (message != NULL) fprintf(stderr, "message : %s\n", message);
  exit(EXIT_FAILURE);
}

/************************************************************************
 * Données persistantes du master
 ************************************************************************/

int last_tested = 2;
int highest_prime = 2;
int nb_primes = 0;

// Sémaphores IPC
int sem_mutex;  // protège la section critique client-master
int sem_sync;   // synchronisation client -> master

/************************************************************************
 * Fonctions secondaires
 ***********************************************************************/
void set_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// STOP
void order_stop(int pipeMW[2]) {
  int stopVal = -1;
  write(pipeMW[1], &stopVal, sizeof(stopVal));

  int fdRetour = open(FIFO_MASTER_TO_CLIENT, O_WRONLY);
  if (fdRetour != -1) {
    int ack = 0;
    write(fdRetour, &ack, sizeof(ack));
    close(fdRetour);
  }

  printf("[MASTER] Ordre STOP traité.\n");
}

/************************************************************************
 * order_compute — pipeline Hoare
 ************************************************************************/
void order_compute(int nombre, int pipeMW[2], int pipeWM[2]) {
  if (nombre > last_tested) {
    for (int i = last_tested + 1; i <= nombre; i++) {
      if (write(pipeMW[1], &i, sizeof(i)) != sizeof(i)) {
        perror("[MASTER] write pipeMW");
        break;
      }
    }
    last_tested = nombre;
  } else {
    write(pipeMW[1], &nombre, sizeof(nombre));
  }

  int prime;
  while (read(pipeWM[0], &prime, sizeof(prime)) > 0) {
    highest_prime = prime;
    nb_primes++;
    printf("[MASTER] Nouveau premier trouvé : %d\n", prime);
  }

  int resultat;
  ssize_t r2 = read(pipeWM[0], &resultat, sizeof(resultat));
  if (r2 != sizeof(resultat)) {
    perror("[MASTER] read résultat primalité");
    resultat = 0;  // FAIL
  }

  if (resultat != 0) {
    nb_primes++;
    highest_prime = resultat;
    printf("[MASTER] SUCCESS : %d est premier\n", resultat);
  } else {
    printf("[MASTER] FAIL : %d n'est pas premier\n", nombre);
  }

  int fdRetour = open(FIFO_MASTER_TO_CLIENT, O_WRONLY);
  if (fdRetour != -1) {
    write(fdRetour, &resultat, sizeof(resultat));
    close(fdRetour);
  } else
    perror("[MASTER] open retour");

  printf("[MASTER] Résultat envoyé au client : %d\n", resultat);
}

/************************************************************************
 * boucle principale
 ************************************************************************/
void loop(int pipeMW[2], int pipeWM[2]) {
  while (1) {
    printf("[MASTER] Attente d'un client...\n");

    int fdClient = open(FIFO_CLIENT_TO_MASTER, O_RDONLY);
    if (fdClient == -1) {
      perror("[MASTER] open FIFO_CLIENT_TO_MASTER");
      continue;
    }

    int order;
    ssize_t r = read(fdClient, &order, sizeof(order));

    if (r == 0) {
      close(fdClient);
      continue;
    }

    if (r != sizeof(order)) {
      fprintf(stderr, "[MASTER] Ordre incomplet\n");
      close(fdClient);
      continue;
    }

    int nombre = 0;

    int retour = masterInterpretOrder(order, &nombre, fdClient);

    if (retour == -1) {  // ordre inconnu ou erreur
      close(fdClient);
      continue;
    }

    close(fdClient);

    if (order == ORDER_STOP) {
      order_stop(pipeMW);
      break;
    }

    if (order == ORDER_COMPUTE_PRIME) {
      order_compute(nombre, pipeMW, pipeWM);
    }

    else if (order == ORDER_HIGHEST_PRIME) {
      int fdRetour = open(FIFO_MASTER_TO_CLIENT, O_WRONLY);
      if (fdRetour != -1) {
        write(fdRetour, &highest_prime, sizeof(highest_prime));
        close(fdRetour);
      }
    }

    else if (order == ORDER_HOW_MANY_PRIME) {
      int fdRetour = open(FIFO_MASTER_TO_CLIENT, O_WRONLY);
      if (fdRetour != -1) {
        write(fdRetour, &nb_primes, sizeof(nb_primes));
        close(fdRetour);
      }
    }

    P(sem_sync);
  }
}

/************************************************************************
 * main
 ************************************************************************/
int main(void) {
  printf("[MASTER] Démarrage du master\n");

  createFifos();

  key_t key_mutex = ftok("master.c", 'M');
  key_t key_sync = ftok("master.c", 'S');
  if (key_mutex == -1 || key_sync == -1) {
    usage("master", "Erreur ftok");
  }

  sem_mutex = semget(key_mutex, 1, IPC_CREAT | 0666);
  sem_sync = semget(key_sync, 1, IPC_CREAT | 0666);
  if (sem_mutex == -1 || sem_sync == -1) {
    usage("master", "Erreur semget");
  }

  if (semctl(sem_mutex, 0, SETVAL, 1) == -1 ||
      semctl(sem_sync, 0, SETVAL, 0) == -1) {
    usage("master", "Erreur semctl");
  }

  int pipeMW[2], pipeWM[2];
  assert(pipe(pipeMW) == 0);
  assert(pipe(pipeWM) == 0);

  pid_t pid = fork();
  assert(pid != -1);

  if (pid == 0) {
    closePipes(pipeMW[1], pipeWM[0]);

    char fdReadStr[10], fdWriteStr[10], primeStr[10];
    snprintf(fdReadStr, sizeof(fdReadStr), "%d", pipeMW[0]);
    snprintf(fdWriteStr, sizeof(fdWriteStr), "%d", pipeWM[1]);
    snprintf(primeStr, sizeof(primeStr), "%d", 2);

    char *args[] = {"worker.o", fdReadStr, fdWriteStr, primeStr, NULL};
    execv("./worker.o", args);
    perror("execv");
    exit(1);
  }

  closePipes(pipeMW[0], pipeWM[1]);
  set_nonblocking(pipeWM[0]);

  loop(pipeMW, pipeWM);

  closePipes(pipeMW[1], pipeWM[0]);
  unlinkPipes();

  semctl(sem_mutex, 0, IPC_RMID);
  semctl(sem_sync, 0, IPC_RMID);

  printf("[MASTER] Fermeture correcte.\n");
  return EXIT_SUCCESS;
}
