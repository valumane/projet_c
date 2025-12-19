#ifndef MASTER_WORKER_H
#define MASTER_WORKER_H

// On peut mettre ici des éléments propres au couple master/worker :
//    - des constantes pour rendre plus lisible les comunications
//    - des fonctions communes (écriture dans un tube, ...)

void init_semaphores(int *sem_mutex, int *sem_sync);
void launch_worker(int prime, int pipeMW[2], int pipeWM[2]);

#endif
