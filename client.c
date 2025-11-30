#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <unistd.h>

#include "master_client.h"

// chaines possibles pour le premier parametre de la ligne de commande
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

static int parseArgs(int argc, char *argv[], int *number) {
  int order = ORDER_NONE;

  if ((argc != 2) && (argc != 3))
    usage(argv[0], "Nombre d'arguments incorrect");

  if (strcmp(argv[1], TK_STOP) == 0)
    order = ORDER_STOP;
  else if (strcmp(argv[1], TK_COMPUTE) == 0)
    order = ORDER_COMPUTE_PRIME;
  else if (strcmp(argv[1], TK_HOW_MANY) == 0)
    order = ORDER_HOW_MANY_PRIME;
  else if (strcmp(argv[1], TK_HIGHEST) == 0)
    order = ORDER_HIGHEST_PRIME;
  else if (strcmp(argv[1], TK_LOCAL) == 0)
    order = ORDER_COMPUTE_PRIME_LOCAL;

  if (order == ORDER_NONE) usage(argv[0], "ordre incorrect");
  if ((order == ORDER_STOP) && (argc != 2))
    usage(argv[0], TK_STOP " : il ne faut pas de second argument");
  if ((order == ORDER_COMPUTE_PRIME) && (argc != 3))
    usage(argv[0], TK_COMPUTE " : il faut le second argument");
  if ((order == ORDER_HOW_MANY_PRIME) && (argc != 2))
    usage(argv[0], TK_HOW_MANY " : il ne faut pas de second argument");
  if ((order == ORDER_HIGHEST_PRIME) && (argc != 2))
    usage(argv[0], TK_HIGHEST " : il ne faut pas de second argument");
  if ((order == ORDER_COMPUTE_PRIME_LOCAL) && (argc != 3))
    usage(argv[0], TK_LOCAL " : il faut le second argument");
  if ((order == ORDER_COMPUTE_PRIME) || (order == ORDER_COMPUTE_PRIME_LOCAL)) {
    *number = strtol(argv[2], NULL, 10);
    if (*number < 2) usage(argv[0], "le nombre doit être >= 2");
  }

  return order;
}

int main(int argc, char *argv[]) {
  if (argc == 2 && strcmp(argv[1], "showworker") == 0) {
    show_worker(argc, argv);
    return 0;  // on ne parle pas au master dans ce mode
  }

  int number = 0;
  int order = parseArgs(argc, argv, &number);

  // ----- MODE LOCAL (3.3 bis) : indépendant du master -----
  if (order == ORDER_COMPUTE_PRIME_LOCAL) {
    printf("[CLIENT] Mode local : calcul des premiers jusqu'à %d\n", number);
    mode_local(number);
    return 0;
  }

  // --- Récupération des sémaphores ---
  int key_mutex = ftok("master.c", 'M');
  int key_sync = ftok("master.c", 'S');

  int sem_mutex = semget(key_mutex, 1, 0666);
  int sem_sync = semget(key_sync, 1, 0666);

  assert(sem_mutex != -1);
  assert(sem_sync != -1);

 
  // --- SECTION CRITIQUE ---
  P(sem_mutex);

  int fdOut = open(FIFO_CLIENT_TO_MASTER, O_WRONLY);
  write(fdOut, &order, sizeof(order));
  if (order == ORDER_COMPUTE_PRIME) write(fdOut, &number, sizeof(number));
  close(fdOut);

  int fdIn = open(FIFO_MASTER_TO_CLIENT, O_RDONLY);
  int resultat = 0;
  read(fdIn, &resultat, sizeof(resultat));
  close(fdIn);

  V(sem_mutex);  // libère le mutex pour un autre client
  V(sem_sync);   // réveille le master (bloqué sur P(sem_sync))
  // --- FIN SECTION CRITIQUE ---

  clientInterpretOrder(order, number, resultat);
  return 0;
}
