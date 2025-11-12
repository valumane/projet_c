#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/**** include ajouté ****/
#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
/***********************/

int last_tested = 2;
int highest_prime = 2;

int createTube(const char *fifoName, int flags) {
  int fd = mkfifo(fifoName, flags);
  assert(fd != -1);
  return fd;
}

char *intToArg(int number) {
  int temp = number;
  int len = 0;

  if (number == 0) {
    len = 1;
  }

  // Compte les chiffres
  while (temp != 0) {
    len++;
    temp /= 10;
  }

  len++;  // +1 pour le caractère de fin '\0'
  char *arg = malloc(len * sizeof(char));
  assert(arg != NULL);

  // Conversion en texte
  snprintf(arg, len, "%d", number);
  printf("arg : %s\n", arg);
  return arg;
}

//************************************************************************/
int main(int argc, char *argv[]) {
  int nombre = atoi(argv[1]);
  
  createTube("master-worker",0666);
  printf("tube master-worker crée\n");
  
  
  int fd = fork();
  assert(fd != -1);

  


  if (fd == 0) {  // processus fils ( worker )

    // affiché le pid du worker et du pere
    printf("Processus worker créé, pid : %d, pere : %d\n", getpid(), getppid());
    int fdReadWorker = open("master-worker",O_WRONLY);
    assert(fdReadWorker!=-1);

    
    // executé le worker avec comme argument nombre
    char *args[] = {"worker", intToArg(nombre), NULL};
    execv("./worker", args);
    perror("execv");

    exit(EXIT_FAILURE); 
    


  }  // processus père ( master )
  else {
    // affiché le pid du master et du fils
    printf("Processus master, pid : %d, fils : %d\n", getpid(), fd);
    sleep(1);
    int fdReadWorker = open("master-worker",O_RDONLY);
    assert(fdReadWorker != -1);
    //ouverture du tube en lecture 
    
    int reponse;
    int n = read(fdReadWorker, &reponse, sizeof(int)); // je lis le nombre envoyé par le client


    // affichage du nombre reçu
    if (n == sizeof(int)) {  // si j'ai bien lu un int
        printf("Reponse recue : %d\n",reponse);
    } else if (n == 0) { // le client a fermé le tube
        printf("Le client a fermé le tube sans rien envoyer.\n");
    } else { // erreur de lecture
        perror("read");
    }

  }

  unlink("master-worker");

  return EXIT_SUCCESS;
}
