#ifndef CLIENT_CRIBLE
#define CLIENT_CRIBLE

// On peut mettre ici des éléments propres au couple master/client :
//    - des constantes pour rendre plus lisible les comunications
//    - des fonctions communes (création tubes, écriture dans un tube,
//      manipulation de sémaphores, ...)


// Fichiers FIFO
#define FIFO_CLIENT_TO_MASTER "client_to_master.fifo"
#define FIFO_MASTER_TO_CLIENT "master_to_client.fifo"


// ordres possibles pour le master
#define ORDER_NONE                0
#define ORDER_STOP               -1
#define ORDER_COMPUTE_PRIME       1
#define ORDER_HOW_MANY_PRIME      2
#define ORDER_HIGHEST_PRIME       3
#define ORDER_COMPUTE_PRIME_LOCAL 4   // ne concerne pas le master

// bref n'hésitez à mettre nombre de fonctions avec des noms explicites
// pour masquer l'implémentation


void createFifos(); //crée les fifos 
void closePipes(int pipe1, int pipe2); // ferme les pipeds
void unlinkPipes(); //supprime les pipes


/* semaphore */
void P(int semid); //prendre 
void V(int semid); //vendre

/* intreprete les ordres */
void clientInterpretOrder(int order, int number, int resultat);
int masterInterpretOrder(int order, int *nombre, int fdClient);
#endif
