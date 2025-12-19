#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include "master_client.h"
#include "master_worker.h"


#include <fcntl.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <unistd.h>
#include "myassert.h"


// ========================
// Données glowrite pipeMWles
// ========================
int last_tested = 1;    // dernier nombre testé
int highest_prime = 0;  // plus grand premier trouvé
int nb_primes = 0;      // combien de nombres premiers trouvés

static void usage(const char *exe, const char *msg) {
  fprintf(stderr, "usage : %s\n", exe);
  if (msg) fprintf(stderr, "message : %s\n", msg);
  exit(EXIT_FAILURE);
}

// ============================================================
// order_compute : envoie les nombres au pipeline et lit les
// résultats de façon BLOQUANTE, proprement.
// ============================================================
int order_compute(int nombre, int pipeMW[2], int pipeWM[2]) {
  int resultat = 0;

  // Cas 1 : on étend la zone de test jusqu'à "nombre"
  if (nombre > last_tested) {
    for (int i = last_tested + 1; i <= nombre; ++i) {
      // envoyer i au pipeline
      if (write(pipeMW[1], &i, sizeof(i)) != sizeof(i)) {
        perror("[MASTER] wwrite pipeMW");
        return 0;
      }

      // lire exactement 1 réponse pour i
      int msg = 0;
      if (read(pipeWM[0], &msg, sizeof(msg)) != sizeof(msg)) {
        perror("[MASTER] read pipeWM");
        return 0;
      }

      // si msg > 0, i est premier
      if (msg > 0) {
        highest_prime = msg;
        nb_primes++;
        printf("[MASTER] Nouveau premier trouvé : %d\n", msg);
      }

      // si c'était le dernier (== nombre), on mémorise son verdict
      if (i == nombre) {
        if (msg > 0) {
          resultat = nombre;  // nombre  premier
        } else {
          resultat = 0;  // nombre pas premier
        }
      }
    }

    last_tested = nombre;
  }
  // Cas 2 : nombre <= last_tested -> on le reteste tout seul
  else {
    if (write(pipeMW[1], &nombre, sizeof(nombre)) != sizeof(nombre)) {
      perror("[MASTER] write pipeMW (retest)");
      return 0;
    }

    int msg = 0;
    if (read(pipeWM[0], &msg, sizeof(msg)) != sizeof(msg)) {
      perror("[MASTER] read pipeWM (retest)");
      return 0;
    }

    if (msg > 0) {
      // évite de gonfler nb_primes si c’est un premier déjà connu
      if (msg > highest_prime) {
        highest_prime = msg;
        nb_primes++;
        printf("[MASTER] Nouveau premier trouvé (retest) : %d\n", msg);
      }
      resultat = nombre;
    } else {
      resultat = 0;
    }
  }

  if (resultat != 0)
    printf("[MASTER] SUCCESS : %d est premier\n", nombre);
  else
    printf("[MASTER] FAIL : %d n'est pas premier\n", nombre);

  return resultat;
}

// ============================================================
// Boucle principale
// ============================================================
void loop(int sem_sync, int pipeMW[2], int pipeWM[2]) {
  while (1) {
    printf("[MASTER] Attente d'un client...\n");

    int order = ORDER_NONE;
    int number = 0;

        // ---- lire la requête du client ----
    int fdIn = open(FIFO_CLIENT_TO_MASTER, O_RDONLY);
    myassert(fdIn != -1, "[MASTER] open(FIFO_CLIENT_TO_MASTER) a échoué");

    int r = read(fdIn, &order, sizeof(order));
    myassert(r == (int)sizeof(order),
             "[MASTER] read ordre depuis le client a échoué");

    if (order == ORDER_COMPUTE_PRIME) {
      r = read(fdIn, &number, sizeof(number));
      if (r != (int)sizeof(number)) {
        perror("[MASTER] read number");
        myassert(close(fdIn) != -1, "[MASTER] close(fdIn) a échoué");
        continue;
      }
      printf("[MASTER] Reçu COMPUTE %d\n", number);
    } else if (order == ORDER_STOP) {
      printf("[MASTER] Reçu STOP\n");
    } else if (order == ORDER_HOW_MANY_PRIME) {
      printf("[MASTER] Reçu HOW_MANY\n");
    } else if (order == ORDER_HIGHEST_PRIME) {
      printf("[MASTER] Reçu HIGHEST\n");
    }

    myassert(close(fdIn) != -1, "[MASTER] close(fdIn) a échoué");


    // ---- traitement ----
    int resultat = 0;

    if (order == ORDER_COMPUTE_PRIME) {
      resultat = order_compute(number, pipeMW, pipeWM);
    } else if (order == ORDER_HIGHEST_PRIME) {
      resultat = highest_prime;
    } else if (order == ORDER_HOW_MANY_PRIME) {
      resultat = nb_primes;
    } else if (order == ORDER_STOP) {
      resultat = 0;  // ACK STOP
    }

    // ---- envoyer la réponse au client ----
    int fdOut = open(FIFO_MASTER_TO_CLIENT, O_WRONLY);
    if (fdOut == -1) {
      perror("[MASTER] open FIFO_MASTER_TO_CLIENT");
    } else {
      if (write(fdOut, &resultat, sizeof(resultat)) != sizeof(resultat)) {
        perror("[MASTER] write resultat");
      }
      close(fdOut);
    }

    // ---- attendre que le client ait fini de lire ----
    P(sem_sync);

    // ---- gestion du STOP ----
    if (order == ORDER_STOP) {
      int stopVal = -1;
      int w = write(pipeMW[1], &stopVal, sizeof(stopVal));
      myassert(w == (int)sizeof(stopVal),"[MASTER] write STOP dans le pipeline a échoué");
      break;
    }
  }
}

// ============================================================
// main
// ============================================================
int main(int argc, char *argv[]) {
  if (argc != 1) usage(argv[0], NULL);

  printf("[MASTER] Démarrage du master (pipeline Hoare stable)\n");

  createFifos();

  // --- sémaphores ---
  int sem_mutex, sem_sync;
  init_semaphores(&sem_mutex, &sem_sync);

  // --- pipes pour le pipeline Hoare ---
  int pipeMW[2], pipeWM[2];

  myassert(pipe(pipeMW) == 0, "pipe(pipeMW) a échoué");
  myassert(pipe(pipeWM) == 0, "pipe(pipeWM) a échoué");

  // --- création du premier worker (prime = 2) ---
  int pid = fork();
  myassert(pid != -1, "fork pour le premier worker a échoué");

  if (pid == 0) {
    // process worker
    launch_worker(2, pipeMW, pipeWM);
  }

  // process master
  closePipes(pipeMW[0], pipeWM[1]);

  // lire le tout premier premier envoyé par le worker 2
  int firstPrime = 0;
  int r = read(pipeWM[0], &firstPrime, sizeof(firstPrime));
  myassert(r == (int)sizeof(firstPrime), "read premier premier depuis le worker a échoué");highest_prime = firstPrime;  // normalement 2
  nb_primes = 1;
  last_tested = firstPrime;
  printf("[MASTER] Premier premier trouvé : %d\n", firstPrime);

  // boucle principale
  loop(sem_sync, pipeMW, pipeWM);

  // nettoyage
  closePipes(pipeMW[1], pipeWM[0]);
  unlinkPipes();
  resetSemaphore(sem_mutex, sem_sync);

  printf("[MASTER] Fermeture propre.\n");
  return 0;
}
