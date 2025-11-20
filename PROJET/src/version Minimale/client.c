
#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../myassert.h"
#include "master_client.h"

/* include ajouté */
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
/******************/

/************** debut **************/
// chaines possibles pour le premier paramètre de la ligne de commande
#define TK_STOP "stop"
#define TK_COMPUTE "compute"
#define TK_HOW_MANY "howmany"
#define TK_HIGHEST "highest"
#define TK_LOCAL "local"

/************************************************************************
 * Usage et analyse des arguments passés en ligne de commande
 ************************************************************************/

static void usage(const char *exeName, const char *message) {
  fprintf(stderr, "usage : %s <ordre> [<nombre>]\n", exeName);
  fprintf(stderr, "   ordre \"" TK_STOP "\" : arrêt master\n");
  fprintf(stderr, "   ordre \"" TK_COMPUTE "\" : calcul de nombre premier\n");
  fprintf(stderr, "                       <nombre> doit être fourni\n");
  fprintf(stderr, "   ordre \"" TK_HOW_MANY
                  "\" : combien de nombres premiers calculés\n");
  fprintf(stderr, "   ordre \"" TK_HIGHEST
                  "\" : quel est le plus grand nombre premier calculé\n");
  fprintf(stderr,
          "   ordre \"" TK_LOCAL "\" : calcul de nombres premiers en local\n");
  if (message != NULL) fprintf(stderr, "message : %s\n", message);
  exit(EXIT_FAILURE);
}

/************************************************************************
 * Analyse des arguments passés en ligne de commande
 ************************************************************************/
static int parseArgs(int argc, char *argv[], int *number) {
  int order = ORDER_NONE;

  if ((argc != 2) && (argc != 3))
    usage(argv[0], "Nombre d'arguments incorrect");

  if (strcmp(argv[1], TK_STOP) == 0) {
    order = ORDER_STOP;
  } else if (strcmp(argv[1], TK_COMPUTE) == 0) {
    order = ORDER_COMPUTE_PRIME;
  } else if (strcmp(argv[1], TK_HOW_MANY) == 0) {
    order = ORDER_HOW_MANY_PRIME;
  } else if (strcmp(argv[1], TK_HIGHEST) == 0) {
    order = ORDER_HIGHEST_PRIME;
  } else if (strcmp(argv[1], TK_LOCAL) == 0) {
    order = ORDER_COMPUTE_PRIME_LOCAL;
  }

  if (order == ORDER_NONE) {
    usage(argv[0], "ordre incorrect");
  }
  if ((order == ORDER_STOP) && (argc != 2)) {
    usage(argv[0], TK_STOP " : il ne faut pas de second argument");
  }
  if ((order == ORDER_COMPUTE_PRIME) && (argc != 3)) {
    usage(argv[0], TK_COMPUTE " : il faut le second argument");
  }
  if ((order == ORDER_HOW_MANY_PRIME) && (argc != 2)) {
    usage(argv[0], TK_HOW_MANY " : il ne faut pas de second argument");
  }
  if ((order == ORDER_HIGHEST_PRIME) && (argc != 2)) {
    usage(argv[0], TK_HIGHEST " : il ne faut pas de second argument");
  }
  if ((order == ORDER_COMPUTE_PRIME_LOCAL) && (argc != 3)) {
    usage(argv[0], TK_LOCAL " : il faut le second argument");
  }

  if ((order == ORDER_COMPUTE_PRIME) || (order == ORDER_COMPUTE_PRIME_LOCAL)) {
    *number = strtol(argv[2], NULL, 10);
    if (*number < 2) {
      usage(argv[0], "le nombre doit être >= 2");
    }
  }

  return order;
}

/************************************************************************
 * Fonction principale
 ************************************************************************/

const char *fifoClientToMaster = "client_to_master.fifo";
const char *fifoMasterToClient = "master_to_client.fifo";

int main(int argc, char *argv[]) {
  int number = 0;
  int order = parseArgs(argc, argv, &number);

  // Cas particulier : mode local -> ne concerne pas le master
  if (order == ORDER_COMPUTE_PRIME_LOCAL) {
    printf("[CLIENT] Mode local pas encore implémenté.\n");
    return EXIT_SUCCESS;
  }

  // === Envoi de l'ordre (et éventuellement du nombre) au master ===
  int fdWrite = open(fifoClientToMaster, O_WRONLY);
  if (fdWrite == -1) {
    perror("[CLIENT] open fifoClientToMaster");
    return EXIT_FAILURE;
  }

  /* on envoie d'abord l'ordre */
  if (write(fdWrite, &order, sizeof(order)) != sizeof(order)) {
    perror("[CLIENT] write ordre");
    close(fdWrite);
    return EXIT_FAILURE;
  }

  if (order == ORDER_COMPUTE_PRIME) {
    if (write(fdWrite, &number, sizeof(number)) != sizeof(number)) {
      perror("[CLIENT] write number");
      close(fdWrite);
      return EXIT_FAILURE;
    }
    printf("[CLIENT] Envoi de l'ordre COMPUTE pour %d au master\n", number);
  } else if (order == ORDER_STOP) {
    printf("[CLIENT] Envoi de l'ordre STOP au master\n");
  } else if (order == ORDER_HOW_MANY_PRIME) {
    printf("[CLIENT] Envoi de l'ordre HOW_MANY au master\n");
  } else if (order == ORDER_HIGHEST_PRIME) {
    printf("[CLIENT] Envoi de l'ordre HIGHEST au master\n");
  }

  close(fdWrite);

  // === Réception de la réponse du master ===
  int fdRead = open(fifoMasterToClient, O_RDONLY);
  if (fdRead == -1) {
    perror("[CLIENT] open fifoMasterToClient");
    return EXIT_FAILURE;
  }

  int resultat;
  ssize_t n = read(fdRead, &resultat, sizeof(resultat));
  close(fdRead);

  if (n != (ssize_t)sizeof(resultat)) {
    fprintf(stderr, "[CLIENT] Aucune réponse valide du master.\n");
    return EXIT_FAILURE;
  }

  // Interprétation de la réponse en fonction de l'ordre
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

  return EXIT_SUCCESS;
}