
#if defined HAVE_CONFIG_H
#include "config.h"
#endif


#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "myassert.h"
#include "master_client.h"

/* include ajouté */
  #include <assert.h> 
  #include <fcntl.h>
  #include <unistd.h>
/******************/

/************** debut **************/
// chaines possibles pour le premier paramètre de la ligne de commande
#define TK_STOP      "stop"
#define TK_COMPUTE   "compute"
#define TK_HOW_MANY  "howmany"
#define TK_HIGHEST   "highest"
#define TK_LOCAL     "local"


/************************************************************************
 * Usage et analyse des arguments passés en ligne de commande
 ************************************************************************/

static void usage(const char *exeName, const char *message){
    fprintf(stderr, "usage : %s <ordre> [<nombre>]\n", exeName);
    fprintf(stderr, "   ordre \"" TK_STOP  "\" : arrêt master\n");
    fprintf(stderr, "   ordre \"" TK_COMPUTE  "\" : calcul de nombre premier\n");
    fprintf(stderr, "                       <nombre> doit être fourni\n");
    fprintf(stderr, "   ordre \"" TK_HOW_MANY "\" : combien de nombres premiers calculés\n");
    fprintf(stderr, "   ordre \"" TK_HIGHEST "\" : quel est le plus grand nombre premier calculé\n");
    fprintf(stderr, "   ordre \"" TK_LOCAL  "\" : calcul de nombres premiers en local\n");
    if (message != NULL)
        fprintf(stderr, "message : %s\n", message);
    exit(EXIT_FAILURE);
}


/************************************************************************
 * Analyse des arguments passés en ligne de commande
 ************************************************************************/
static int parseArgs(int argc, char * argv[], int *number){
    int order = ORDER_NONE;

    if ((argc != 2) && (argc != 3))
        usage(argv[0], "Nombre d'arguments incorrect");

    if (strcmp(argv[1], TK_STOP) == 0){ order = ORDER_STOP;} else
    if (strcmp(argv[1], TK_COMPUTE) == 0){ order = ORDER_COMPUTE_PRIME; } else
    if (strcmp(argv[1], TK_HOW_MANY) == 0){ order = ORDER_HOW_MANY_PRIME; } else
    if (strcmp(argv[1], TK_HIGHEST) == 0){ order = ORDER_HIGHEST_PRIME; } else
    if (strcmp(argv[1], TK_LOCAL) == 0){ order = ORDER_COMPUTE_PRIME_LOCAL;}
    
    if (order == ORDER_NONE){ usage(argv[0], "ordre incorrect"); }
    if ((order == ORDER_STOP) && (argc != 2)){ usage(argv[0], TK_STOP" : il ne faut pas de second argument");}
    if ((order == ORDER_COMPUTE_PRIME) && (argc != 3)){ usage(argv[0], TK_COMPUTE " : il faut le second argument"); }
    if ((order == ORDER_HOW_MANY_PRIME) && (argc != 2)){ usage(argv[0], TK_HOW_MANY" : il ne faut pas de second argument"); }
    if ((order == ORDER_HIGHEST_PRIME) && (argc != 2)){ usage(argv[0], TK_HIGHEST " : il ne faut pas de second argument"); }
    if ((order == ORDER_COMPUTE_PRIME_LOCAL) && (argc != 3)){ usage(argv[0], TK_LOCAL " : il faut le second argument"); }
    
    if ((order == ORDER_COMPUTE_PRIME) || (order == ORDER_COMPUTE_PRIME_LOCAL)){
        *number = strtol(argv[2], NULL, 10);
        if (*number < 2){ 
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

int main(int argc, char * argv[]){
    assert(argc > 1);
    // === Envoi du nombre au master ===
    int fdWrite = open(fifoClientToMaster, O_WRONLY);
    assert(fdWrite != -1);
    
    
    int nombre = atoi(argv[1]);
    write(fdWrite, &nombre, sizeof(int));// envoyer le nombre au master
    fprintf(stdout,"[CLIENT] Envoi du nombre %d au master\n", nombre);
    close(fdWrite); // fermer le descripteur d'écriture

    // === Réception de la réponse du master ===
    int fdRead = open(fifoMasterToClient, O_RDONLY);
    assert(fdRead != -1);


    int resultat;
    int n = read(fdRead, &resultat, sizeof(int));
    close(fdRead);

    if (n == sizeof(int)) {
        if (resultat == 1)
            printf("[CLIENT] %d est premier\n", nombre);
        else
            printf("[CLIENT] %d n'est pas premier\n", nombre);
    } else {
        printf("[CLIENT] Aucune réponse du master.\n");
    }

    return EXIT_SUCCESS;
}
