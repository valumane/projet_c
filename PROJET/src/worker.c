#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "myassert.h"

#include "master_worker.h"

/************************************************************************
 * Données persistantes d'un worker
 ************************************************************************/

// on peut ici définir une structure stockant tout ce dont le worker
// a besoin : le nombre premier dont il a la charge, ...


/************************************************************************
 * Usage et analyse des arguments passés en ligne de commande
 ************************************************************************/

static void usage(const char *exeName, const char *message){
    fprintf(stderr, "usage : %s <n> <fdIn> <fdToMaster>\n", exeName);
    fprintf(stderr, "   <n> : nombre premier géré par le worker\n");
    fprintf(stderr, "   <fdIn> : canal d'entrée pour tester un nombre\n");
    fprintf(stderr, "   <fdToMaster> : canal de sortie pour indiquer si un nombre est premier ou non\n");
    if (message != NULL)
        fprintf(stderr, "message : %s\n", message);
    exit(EXIT_FAILURE);
}


static void parseArgs(int argc, char * argv[]){
    if (argc != 4)
        usage(argv[0], "Nombre d'arguments incorrect");

    // remplir la structure
}

/************************************************************************
 * Boucle principale de traitement
 ************************************************************************/

/*void loop(){
    // boucle infinie :
    //    attendre l'arrivée d'un nombre à tester
    //    si ordre d'arrêt
    //       si il y a un worker suivant, transmettre l'ordre et attendre sa fin
    //       sortir de la boucle
    //    sinon c'est un nombre à tester, 4 possibilités :
    //           - le nombre est premier
    //           - le nombre n'est pas premier
    //           - s'il y a un worker suivant lui transmettre le nombre
    //           - s'il n'y a pas de worker suivant, le créer
}
*/
/************************************************************************
 * Programme principal
 ************************************************************************/

int main(int argc, char * argv[]){
    printf("\n---------------------\n");
    printf("Debut worker.c main\n");

    //parseArgs(argc, argv /*, structure à remplir*/);
     
    
    // Si on est créé c'est qu'on est un nombre premier
    // Envoyer au master un message positif pour dire
    // que le nombre testé est bien premier

    printf("Worker démarré, pid : %d, pere : %d\n", getpid(), getppid());
    printf("nombre recu en argument : %s\n", argv[1]);    

    printf("Worker termine\n");
    printf("---------------------\n");
    
    // loop(/* paramètres */);

    // libérer les ressources : fermeture des files descriptors par exemple
    // le worker est lancé par execv donc pas besoin de libérer de la mémoire
    

    return EXIT_SUCCESS;
}
