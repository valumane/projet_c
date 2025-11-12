#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/**** include ajouté ****/
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <assert.h>
/***********************/

#include "myassert.h"

#include "master_client.h"
#include "master_worker.h"

/************************************************************************
 * Données persistantes d'un master
 ************************************************************************/

// on peut ici définir une structure stockant tout ce dont le master
// a besoin

    int last_tested = 2;
    int highest_prime = 2;


    // nom des tubes nommés
    //const char *fifoClientToMaster = "client_to_master.fifo";
    //const char *fifoMasterToClient = "master_to_client.fifo";
    //int res;

/************************************************************************
 * Usage et analyse des arguments passés en ligne de commande
 ************************************************************************/

static void usage(const char *exeName, const char *message){
    fprintf(stderr, "usage : %s\n", exeName);
    if (message != NULL)
        fprintf(stderr, "message : %s\n", message);
    exit(EXIT_FAILURE);
}


/************************************************************************
 * boucle principale de communication avec le client
 ************************************************************************/
void loop(/* paramètres */)
{
    // boucle infinie :
    // - ouverture des tubes (cf. rq client.c)
    // - attente d'un ordre du client (via le tube nommé)
    // - si ORDER_STOP
    //       . envoyer ordre de fin au premier worker et attendre sa fin
    //       . envoyer un accusé de réception au client
    // - si ORDER_COMPUTE_PRIME
    //       . récupérer le nombre N à tester provenant du client
    //       . construire le pipeline jusqu'au nombre N-1 (si non encore fait) :
    //             il faut connaître le plus grand nombre (M) déjà enovoyé aux workers
    //             on leur envoie tous les nombres entre M+1 et N-1
    //             note : chaque envoie déclenche une réponse des workers
    //       . envoyer N dans le pipeline
    //       . récupérer la réponse
    //       . la transmettre au client
    // - si ORDER_HOW_MANY_PRIME
    //       . transmettre la réponse au client (le plus simple est que cette
    //         information soit stockée en local dans le master)
    // - si ORDER_HIGHEST_PRIME
    //       . transmettre la réponse au client (le plus simple est que cette
    //         information soit stockée en local dans le master)
    // - fermer les tubes nommés
    // - attendre ordre du client avant de continuer (sémaphore : précédence)
    // - revenir en début de boucle
    //
    // il est important d'ouvrir et fermer les tubes nommés à chaque itération
    // voyez-vous pourquoi ?
}

/*********************************************************** *
 * fonction secondaire
 * ***********************************************************/

 int createTube(const char *fifoName, int flags){
    int fd = mkfifo(fifoName, flags);
    assert(fd != -1);
    return fd;
}

char *intToArg(int number) {
    int temp = number;
    int len = 0;

    if (number == 0){ len = 1;} 
    
    // Compte les chiffres
    while (temp != 0) {
        len++;
        temp /= 10;
    }

    len++; // +1 pour le caractère de fin '\0'
    char *arg = malloc(len * sizeof(char));
    assert(arg != NULL);

    // Conversion en texte
    snprintf(arg, len, "%d", number);
    printf("arg : %s\n", arg);
    return arg;
}


/* le client envoie un truc au master, code bougé pour pouvoir commencé la gestion master -> worker
printf("Création du tube nommé client_to_master.fifo\n");
        mkfifo(fifoClientToMaster, 0666);

        // ouverture bloquante : attend qu’un client écrive
        int fdRead = open(fifoClientToMaster, O_RDONLY); //j'ouvre le tube en lecture ( READ ONLY )
        assert(fdRead != -1); //je test si le tube est bien ouvert
        fprintf(stdout,"Tube ouvert, en attente d’un nombre...\n"); 

        int nombre;
        int n = read(fdRead, &nombre, sizeof(int)); // je lis le nombre envoyé par le client


        // affichage du nombre reçu
        if (n == sizeof(int)) {  // si j'ai bien lu un int
            fprintf(stdout,"Nombre reçu du client : %d\n", nombre);
        } else if (n == 0) { // le client a fermé le tube
            fprintf(stdout,"Le client a fermé le tube sans rien envoyer.\n");
        } else { // erreur de lecture
            fprintf(stderr,"Erreur de lecture");
        }
*/

/************************************************************************
 * Fonction principale
 ************************************************************************/

int main(int argc, char * argv[]){
    /*if (argc != 1){
        printf("argc %d\n", argc);
        usage(argv[0], NULL);
    }*/
    // - création des sémaphores
        // ipc ( mutex_clients, sync_client )
    
    // - création des tubes nommés
        // client_to_master.fifo : recevoir les demandes du client
        
        
    

        
/*
        // master_to_client.fifo : pour envoyer les resultats au client
        printf("Création du tube nommé master_to_client.fifo\n");
        mkfifo(fifoMasterToClient, 0666);
*/
        

    // - création du premier worker
        // creation de deux pipes anonyme :
            // pipe(to_worker) : pour envoyer des ordres au worker
            
            int nombre = atoi(argv[1]);
            printf("nombre : %d\n", nombre);

            int fd = fork();
            assert(fd != -1);

            if (fd == 0){ // processus fils
                // affiché le pid du worker et du pere
                printf("Processus worker créé, pid : %d, pere : %d\n", getpid(), getppid());
                // executé le worker avec comme argument nombre
                char *args[] = {"worker", intToArg(nombre), NULL};
                int fdWorker = execv( "./worker", args );
                assert(fdWorker != -1);
                    
            } // processus père
            else{
                // affiché le pid du master et du fils
                printf("Processus master, pid : %d, fils : %d\n", getpid(), fd);
            }







            // pipe(from_worker) : pour recevoir des réponses du worker

        // fork()
        // dans le fils :
            // execv( "worker", {"worker", "2", fdIn, fdToMaster, NULL} )
        // dans le père :
            // to_worker[1] : pour ecrire vers worker
            // from_worker[0] : pour lire worker / pipeline
            // ferme les extremités inutilisées des pipes

    // boucle infinie
    loop(/* paramètres */);

    // destruction des tubes nommés, des sémaphores, ...
        //unlink(fifoClientToMaster);
        //unlink(fifoMasterToClient);
        


    return EXIT_SUCCESS;
}

// N'hésitez pas à faire des fonctions annexes ; si les fonctions main
// et loop pouvaient être "courtes", ce serait bien
