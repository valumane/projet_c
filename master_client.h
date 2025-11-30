#ifndef CLIENT_CRIBLE
#define CLIENT_CRIBLE

// On peut mettre ici des éléments propres au couple master/client :
//    - des constantes pour rendre plus lisible les comunications
//    - des fonctions communes (création tubes, écriture dans un tube,
//      manipulation de sémaphores, ...)

// ordres possibles pour le master
#define ORDER_NONE 0
#define ORDER_STOP -1
#define ORDER_COMPUTE_PRIME 1
#define ORDER_HOW_MANY_PRIME 2
#define ORDER_HIGHEST_PRIME 3
#define ORDER_COMPUTE_PRIME_LOCAL 4  // ne concerne pas le master
#define ORDER_SHOW //montre les processus lancé

#define FIFO_CLIENT_TO_MASTER "client_to_master.fifo"
#define FIFO_MASTER_TO_CLIENT "master_to_client.fifo"

void V(int semid);
void P(int semid);
void resetSemaphore(int sem1, int sem2);
void createFifos();
void closePipes(int pipe1, int pipe2);
void unlinkPipes();
void clientInterpretOrder(int order, int number, int resultat);
void clientSendOrder(int fdOut, int order, int number);
int mysqrt(int n);
void *threadMark(void *arg);
void mode_local(int number);
void show_worker(int argc, char *argv[]);

// bref n'hésitez à mettre nombre de fonctions avec des noms explicites
// pour masquer l'implémentation

#endif