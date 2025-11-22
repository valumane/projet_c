#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#define _XOPEN_SOURCE

#include <stdlib.h>
#include <stdio.h>

#include <assert.h>

#include "master_client.h"


#include <sys/stat.h>   // <-- pour mkfifo()
#include <unistd.h>     // <-- pour close() et unlink()
#include <errno.h>      // <-- pour errno si tu veux gérer les erreurs

#include <sys/ipc.h>
#include <sys/sem.h>


// fonctions éventuelles internes au fichier
void createFifos(){
  mkfifo(FIFO_CLIENT_TO_MASTER, 0666);
  mkfifo(FIFO_MASTER_TO_CLIENT, 0666);
}

void closePipes(int pipe1, int pipe2){
  close(pipe1);
  close(pipe2);
}

void unlinkPipes(){
  unlink(FIFO_CLIENT_TO_MASTER);  // supprimer la FIFO client->master
  unlink(FIFO_MASTER_TO_CLIENT);  // supprimer la FIFO master->client
}

/* semaphore */
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


/* intrepreter ordre */
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



int masterInterpretOrder(int order, int *nombre, int fdClient) {
  if (order == ORDER_COMPUTE_PRIME) {
    if (read(fdClient, nombre, sizeof(*nombre)) != sizeof(*nombre)) {
      printf("[MASTER] Nombre manquant\n");
      return -1;
    }
    printf("[MASTER] Reçu COMPUTE %d\n", *nombre);
    return *nombre;
  }

  else if (order == ORDER_STOP) {
    printf("[MASTER] Reçu STOP\n");
    return -2;
  }

  else if (order == ORDER_HOW_MANY_PRIME) {
    printf("[MASTER] Reçu HOW_MANY\n");
    return 0;
  }

  else if (order == ORDER_HIGHEST_PRIME) {
    printf("[MASTER] Reçu HIGHEST\n");
    return 0;
  }

  else {
    printf("[MASTER] Ordre inconnu.\n");
    close(fdClient);
    return -1;
  }
}

// fonctions éventuelles proposées dans le .h

