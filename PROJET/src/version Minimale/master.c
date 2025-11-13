#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "myassert.h"

#include "master_client.h"
#include "master_worker.h"

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

/************************************************************************
 * Usage et analyse des arguments passés en ligne de commande
 ************************************************************************/

static void usage(const char *exeName, const char *message){
    fprintf(stderr, "usage : %s\n", exeName);
    if (message != NULL){
        fprintf(stderr, "message : %s\n", message);
    }
    exit(EXIT_FAILURE);
}


/************************************************************************
 * boucle principale de communication avec le client
 ************************************************************************/
void loop(int pipeMW[2], int pipeWM[2]) {
  /*
     boucle infinie :
     - ouverture des tubes (cf. rq client.c)
     - attente d'un ordre du client (via le tube nommé)
     - si ORDER_STOP
           . envoyer ordre de fin au premier worker et attendre sa fin
           . envoyer un accusé de réception au client
     - si ORDER_COMPUTE_PRIME
           . récupérer le nombre N à tester provenant du client
           . construire le pipeline jusqu'au nombre N-1 (si non encore fait) :
                 il faut connaître le plus grand nombre (M) déjà enovoyé aux workers
                 on leur envoie tous les nombres entre M+1 et N-1
                 note : chaque envoie déclenche une réponse des workers
           . envoyer N dans le pipeline
           . récupérer la réponse
           . la transmettre au client
     - si ORDER_HOW_MANY_PRIME
           . transmettre la réponse au client (le plus simple est que cette
             information soit stockée en local dans le master)
     - si ORDER_HIGHEST_PRIME
           . transmettre la réponse au client (le plus simple est que cette
             information soit stockée en local dans le master)
     - fermer les tubes nommés
     - attendre ordre du client avant de continuer (sémaphore : précédence)
     - revenir en début de boucle
    
     il est important d'ouvrir et fermer les tubes nommés à chaque itération
     voyez-vous pourquoi ?
     */

    while (1) {  // boucle principale du master
      int fdClient = open(fifoClientToMaster, O_RDONLY);
      if (fdClient == -1) continue;

      int nombre;
      int n = read(fdClient, &nombre, sizeof(int)); 
      close(fdClient);

      if (n == 0) continue;
      if (nombre == -1) break;

      printf("[MASTER] Nombre reçu du client : %d\n", nombre);

    // envoi du nombre
    if (nombre > last_tested) {
      for (int i = last_tested + 1; i <= nombre; i++)
        write(pipeMW[1], &i, sizeof(int));
      last_tested = nombre;
    } else {
      write(pipeMW[1], &nombre, sizeof(int));
    }

    // lecture non bloquante des nouveaux premiers
    int prime;
    while (read(pipeWM[0], &prime, sizeof(int)) > 0) {
      highest_prime = prime;
      printf("[MASTER] Nouveau nombre premier trouvé : %d\n", prime);
    }

    // verif si le nombre est premier
    int isPrime = 1;
    for (int i = 2; i * i <= nombre; i++) {
      if (nombre % i == 0) {
        isPrime = 0;
      break;
      }
    }

    // envoi du résultat au client
    int fdRetour = open(fifoMasterToClient, O_WRONLY);
    if (fdRetour != -1) {
      write(fdRetour, &isPrime, sizeof(int));
      close(fdRetour);
    }

    printf("[MASTER] Résultat envoyé au client : %d (%s)\n", isPrime,
           isPrime ? "premier" : "non premier");
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
  int pipeMW[2]; // master -> worker
  int pipeWM[2]; // worker -> master
  assert(pipe(pipeMW) == 0);
  assert(pipe(pipeWM) == 0);

  pid_t pid = fork();
  assert(pid != -1);

  if (pid == 0) {  // processus worker
    // fermer les descripteurs inutiles
    close(pipeMW[1]); // écriture vers le worker
    close(pipeWM[0]); // lecture depuis le worker

    // exécuter le worker
    char fdReadStr[10], fdWriteStr[10], primeStr[10];
    snprintf(fdReadStr, sizeof(fdReadStr), "%d", pipeMW[0]); // lecture du master
    snprintf(fdWriteStr, sizeof(fdWriteStr), "%d", pipeWM[1]); // écriture vers le master

    snprintf(primeStr, sizeof(primeStr), "%d", 2); // premier initial
    char *args[] = {"worker", fdReadStr, fdWriteStr, primeStr, NULL};
    execv("./worker", args);
    perror("execv"); 
    exit(EXIT_FAILURE);
  }

  close(pipeMW[0]);// fermer le pipe de lecture vers le worker
  close(pipeWM[1]);// fermer le pipe d'écriture depuis le worker
  set_nonblocking(pipeWM[0]);  // lecture non bloquante

  // boucle principale de communication avec le client
  loop(pipeMW, pipeWM);
  
  // envoi du signal d'arrêt au worker
  close(pipeMW[1]); // fermer le pipe d'écriture vers le worker
  close(pipeWM[0]); // fermer le pipe de lecture depuis le worker
  unlink(fifoClientToMaster); // supprimer la FIFO client->master
  unlink(fifoMasterToClient); // supprimer la FIFO master->client
  printf("[MASTER] Fermeture propre du master.\n");
  return EXIT_SUCCESS;
}
