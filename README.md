FONCTIONNEMENT DU PROJET  

principe :  
- Le client demande au master si un chiffre n est premier.  
- Le master envoie la demande aux workers, qui vérifient le nombre.  
- Une fois la vérification terminée, le master renvoie le résultat au client.  

COMMUNICATIONS :  
- client <-> master : tubes nommés (mkfifo)  
- master <-> premier worker : deux pipes anonymes (écrire/lire)  
- worker <-> worker suivant : pipes anonymes en chaîne  
- tous les workers -> master : **même pipe anonyme partagé** pour les réponses  

ORDRES CÔTÉ CLIENT :  
- 1) arrêt du master  
- 2) “n est-il premier ?”  
- 3) combien de nombres premiers trouvés  
- 4) plus grand nombre premier trouvé  
(remarque : 3) et 4) sont répondus directement par le master, sans passer par les workers)  

SYNCHRO (SÉMAPHORES IPC) :  
- mutex_clients (init = 1) : un seul client à la fois dans la section critique (FIFOs).  
- sync_client (init = 0) : le master attend que le **client** relâche après lecture de la réponse pour accepter un nouveau client.  

ETAPES DE FONCTIONNEMENT  

1) LANCEMENT DU MASTER  
- Le master crée deux tubes nommés :  
  - client_to_master.fifo : pour recevoir les demandes du client.  
  - master_to_client.fifo : pour envoyer les résultats au client.  
- Il crée/initialise les sémaphores IPC (mutex_clients, sync_client).  
- Il crée le premier worker (W2) :  
  - crée deux pipes anonymes :  
        pipe(to_worker)  
        pipe(from_worker)  
  - fork() puis, côté fils : execv("./worker", {"worker", "2", fdIn, fdToMaster, NULL})  
  - côté père (master), conserve :  
        to_worker[1] pour écrire vers W2  
        from_worker[0] pour lire W2 / pipeline  
  - ferme les extrémités inutiles.  

2) BOUCLE PRINCIPALE DU MASTER  
- attendre qu’un client prenne mutex_clients puis ouvre client_to_master.fifo.  
- lire l’**ordre** :  
  - (2) “n est-il premier ?” :  
    - si n > dernier_nombre_injecté, **injecter séquentiellement** tous les entiers (dernier+1 … n) dans le pipeline pour “poser” les nouveaux étages, puis injecter n.  
    - attendre une **réponse** d’un worker via from_worker.  
    - écrire la réponse dans master_to_client.fifo.  
  - (3) “combien de nombres premiers ?” : répondre avec le **nombre de workers actifs** (le premier W2 compte).  
  - (4) “plus grand nombre premier ?” : répondre avec le **P du dernier worker** de la chaîne.  
  - (1) “arrêt” : envoyer un **ordre d’arrêt** au premier worker, relayer jusqu’au bout, attendre la fin du pipeline, puis répondre “ok”.  
- fermer les FIFOs côté master.  
- attendre que le client poste **sync_client** (il a lu la réponse).  
- retourner en attente d’un nouveau client.  

3) COMPORTEMENT D’UN WORKER  
lancement :  
    ./worker p fdIn fdToMaster  

signification :  
- p : nombre premier géré par ce worker.  
- fdIn : pipe d’où il lit les nombres à tester (depuis le master ou un worker précédent).  
- fdToMaster : pipe partagé vers le master (écriture des résultats).  

boucle :  
1. lire un message depuis fdIn (un entier n, ou un **ordre d’arrêt**).  
2. si **ordre d’arrêt** :  
   - relayer l’arrêt au worker suivant (s’il existe), attendre sa fin, puis se terminer.  
3. sinon (entier n) :  
   - cas A : n == p  
       -> écrire “n est premier” au master via fdToMaster.  
   - cas B : n % p == 0  
       -> écrire “n n’est pas premier” au master.  
   - cas C : n % p != 0  
       -> si un **suivant** existe : écrire n dans le pipe vers le suivant.  
       -> sinon (pas de suivant) : **n est premier** → créer un pipe vers un **nouveau worker Wn**, fork()+execv("./worker","n",...), puis (optionnellement) laisser Wn traiter n (il répondra “n est premier”). Le worker parent conserve le pipe vers Wn pour les futurs nombres.  

(remarque importante : on **ne crée pas** W3 puis W5 pour tester 5 ; W2 crée **directement W5** quand 5 arrive.)  

4) COMPORTEMENT DU MASTER APRES LE FORK INITIAL  
- côté père (master) :  
    - garde :  
        to_worker[1] pour écrire vers W2  
        from_worker[0] pour lire les réponses (tous les workers écrivent sur fdToMaster partagé)  
    - ferme :  
        to_worker[0]  
        from_worker[1]  
- côté fils (worker) :  
    - exécute "./worker p fdIn fdToMaster"  
    - ferme ses fd inutiles et entre dans sa boucle.  

EXEMPLE  : n = 9  
CLIENT -> MASTER : “9 est-il premier ?”  
MASTER : injecte séquentiellement (si nécessaire) jusqu’à 9, puis envoie 9 à W2  
W2 (P=2) : 9 % 2 != 0 -> transmet 9 au suivant  
W3 (P=3) : 9 % 3 == 0 -> envoie “9 n’est pas premier” au master  
MASTER -> CLIENT : “9 n’est pas premier”  
Résultat final : 9 n’est pas premier.  

EXEMPLE : n = 5  
CLIENT -> MASTER : “5 est-il premier ?”  
MASTER : injecte séquentiellement (si nécessaire) jusqu’à 5, puis envoie 5 à W2  
W2 (P=2) : 5 % 2 != 0, **pas de suivant** -> crée **W5**  
W5 (P=5) : reçoit 5 -> n == p -> envoie “5 est premier” au master  
MASTER -> CLIENT : “5 est premier”  
Résultat final : 5 est premier.  
