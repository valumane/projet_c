#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "master_client.h"
#include "master_worker.h"
#include "myassert.h"

/* include ajouté */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
/******************/

/************************************************************************
 * Données persistantes d'un master
 ************************************************************************/

// on peut ici définir une structure stockant tout ce dont le master
// a besoin

const char *fifoClientToMaster = "client_to_master.fifo";
const char *fifoMasterToClient = "master_to_client.fifo";

int last_tested = 2;
int highest_prime = 2;
int nb_primes = 0;  // nombre de nombres premiers trouvés

/************************************************************************
 * Usage et analyse des arguments passés en ligne de commande
 ************************************************************************/

static void usage(const char *exeName, const char *message) {
  fprintf(stderr, "usage : %s\n", exeName);
  if (message != NULL) {
    fprintf(stderr, "message : %s\n", message);
  }
  exit(EXIT_FAILURE);
}

/************************************************************************
 * boucle principale de communication avec le client
 ************************************************************************/
void loop(int pipeMW[2], int pipeWM[2]) {
  while (1) {  // boucle principale du master
    // === Ouverture de la FIFO côté client ===
    int fdClient = open(fifoClientToMaster, O_RDONLY);
    if (fdClient == -1) {
      perror("[MASTER] open fifoClientToMaster");
      continue;
    }

    // On lit d'abord l'ordre
    int order;
    ssize_t r = read(fdClient, &order, sizeof(order));
    if (r == 0) {
      // rien reçu (client a fermé la FIFO)
      close(fdClient);
      continue;
    }
    if (r != (ssize_t)sizeof(order)) {
      fprintf(stderr, "[MASTER] Ordre incomplet reçu (taille=%zd)\n", r);
      close(fdClient);
      continue;
    }

    int nombre = 0;

    if (order == ORDER_COMPUTE_PRIME) {
      // On doit lire le nombre à tester
      ssize_t rr = read(fdClient, &nombre, sizeof(nombre));
      if (rr != (ssize_t)sizeof(nombre)) {
        fprintf(stderr, "[MASTER] Nombre manquant pour ORDER_COMPUTE_PRIME\n");
        close(fdClient);
        continue;
      }
      printf("[MASTER] Reçu ordre COMPUTE pour %d\n", nombre);
    } else if (order == ORDER_STOP) {
      printf("[MASTER] Reçu ordre STOP\n");
    } else if (order == ORDER_HOW_MANY_PRIME) {
      printf("[MASTER] Reçu ordre HOW_MANY\n");
    } else if (order == ORDER_HIGHEST_PRIME) {
      printf("[MASTER] Reçu ordre HIGHEST\n");
    } else {
      printf("[MASTER] Ordre inconnu : %d\n", order);
      close(fdClient);
      continue;
    }

    close(fdClient);

    // === Traitement des ordres ===

    // 1) STOP : on propage -1 aux workers et on répond au client
    if (order == ORDER_STOP) {
      int stopVal = -1;
      if (write(pipeMW[1], &stopVal, sizeof(stopVal)) != sizeof(stopVal)) {
        perror("[MASTER] write stopVal");
      }

      int fdRetour = open(fifoMasterToClient, O_WRONLY);
      if (fdRetour != -1) {
        int ack = 0;
        write(fdRetour, &ack, sizeof(ack));
        close(fdRetour);
      } else {
        perror("[MASTER] open fifoMasterToClient (STOP)");
      }

      printf("[MASTER] Ordre STOP traité, on sort de la boucle.\n");
      break;  // sortie de loop()
    }

    // 2) COMPUTE_PRIME : on garde ton pipeline + test local pour l'instant
    if (order == ORDER_COMPUTE_PRIME) {
      // Envoi du ou des nombres dans le pipeline
      if (nombre > last_tested) {
        for (int i = last_tested + 1; i <= nombre; i++) {
          if (write(pipeMW[1], &i, sizeof(i)) != sizeof(i)) {
            perror("[MASTER] write pipeMW");
            break;
          }
        }
        last_tested = nombre;
      } else {
        if (write(pipeMW[1], &nombre, sizeof(nombre)) != sizeof(nombre)) {
          perror("[MASTER] write pipeMW (nombre)");
        }
      }

      // Lecture non bloquante des nouveaux premiers trouvés
      int prime;
      while (read(pipeWM[0], &prime, sizeof(prime)) > 0) {
        highest_prime = prime;
        nb_primes++;  // on compte les nouveaux premiers détectés
        printf("[MASTER] Nouveau nombre premier trouvé : %d\n", prime);
      }

      // Test local (temporaire) pour savoir si "nombre" est premier
      int isPrime = 1;
      for (int i = 2; i * i <= nombre; i++) {
        if (nombre % i == 0) {
          isPrime = 0;
          break;
        }
      }

      // Envoi du résultat au client
      int fdRetour = open(fifoMasterToClient, O_WRONLY);
      if (fdRetour != -1) {
        write(fdRetour, &isPrime, sizeof(isPrime));
        close(fdRetour);
      } else {
        perror("[MASTER] open fifoMasterToClient (COMPUTE)");
      }

      printf("[MASTER] Résultat envoyé au client : %d (%s)\n", isPrime,
             isPrime ? "premier" : "non premier");
    }

    // 3) HOW_MANY : renvoyer nb_primes
    else if (order == ORDER_HOW_MANY_PRIME) {
      int fdRetour = open(fifoMasterToClient, O_WRONLY);
      if (fdRetour != -1) {
        write(fdRetour, &nb_primes, sizeof(nb_primes));
        close(fdRetour);
      } else {
        perror("[MASTER] open fifoMasterToClient (HOW_MANY)");
      }
    }

    // 4) HIGHEST : renvoyer highest_prime
    else if (order == ORDER_HIGHEST_PRIME) {
      int fdRetour = open(fifoMasterToClient, O_WRONLY);
      if (fdRetour != -1) {
        write(fdRetour, &highest_prime, sizeof(highest_prime));
        close(fdRetour);
      } else {
        perror("[MASTER] open fifoMasterToClient (HIGHEST)");
      }
    }
  }
}

/************************************************************************
 * fonction secondaires *
 ***********************************************************************/
// rend un pipe non bloquant
void set_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/************************************************************************
 * boucle principale de communication avec le client
 ************************************************************************/

int main(void) {
  printf("[MASTER] Démarrage du master\n");

  // création des FIFOs
  mkfifo(fifoClientToMaster, 0666);
  mkfifo(fifoMasterToClient, 0666);

  // création des pipes de communication avec le worker
  int pipeMW[2];  // master -> worker
  int pipeWM[2];  // worker -> master
  assert(pipe(pipeMW) == 0);
  assert(pipe(pipeWM) == 0);

  pid_t pid = fork();
  assert(pid != -1);

  if (pid == 0) {  // processus worker
    // fermer les descripteurs inutiles
    close(pipeMW[1]);  // écriture vers le worker
    close(pipeWM[0]);  // lecture depuis le worker

    // exécuter le worker
    char fdReadStr[10], fdWriteStr[10], primeStr[10];
    snprintf(fdReadStr, sizeof(fdReadStr), "%d",
             pipeMW[0]);  // lecture du master
    snprintf(fdWriteStr, sizeof(fdWriteStr), "%d",
             pipeWM[1]);  // écriture vers le master

    snprintf(primeStr, sizeof(primeStr), "%d", 2);  // premier initial
    char *args[] = {"worker.o", fdReadStr, fdWriteStr, primeStr, NULL};
    execv("./worker.o", args);
    perror("execv");
    exit(EXIT_FAILURE);
  }

  close(pipeMW[0]);            // fermer le pipe de lecture vers le worker
  close(pipeWM[1]);            // fermer le pipe d'écriture depuis le worker
  set_nonblocking(pipeWM[0]);  // lecture non bloquante

  // boucle principale de communication avec le client
  loop(pipeMW, pipeWM);

  // envoi du signal d'arrêt au worker
  close(pipeMW[1]);            // fermer le pipe d'écriture vers le worker
  close(pipeWM[0]);            // fermer le pipe de lecture depuis le worker
  unlink(fifoClientToMaster);  // supprimer la FIFO client->master
  unlink(fifoMasterToClient);  // supprimer la FIFO master->client
  printf("[MASTER] Fermeture propre du master.\n");
  return EXIT_SUCCESS;
}
