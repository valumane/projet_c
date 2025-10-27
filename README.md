==============================
FONCTIONNEMENT DU PROJET
==============================

principe :
- Le client demande au master si un chiffre n est premier.
- Le master envoie la demande aux workers, qui vérifient le nombre.
- Une fois la vérification terminée, le master renvoie le résultat au client.

COMMUNICATIONS :
----------------
- client <-> master : tubes nommés (mkfifo)
- master <-> premier worker : pipes anonymes
- worker <-> worker suivant : pipes anonymes
- tous les workers -> master : même pipe anonyme partagé pour les réponses

==============================
ETAPES DE FONCTIONNEMENT
==============================

1) LANCEMENT DU MASTER
-----------------------
- Le master crée deux tubes nommés (avec mkfifo) :
  - client_to_master.fifo : pour recevoir les demandes du client.
  - master_to_client.fifo : pour envoyer les résultats au client.

- Il crée aussi les sémaphores IPC pour gérer la synchronisation entre plusieurs clients.
( on va zappé sa pour l'instant )

- Ensuite, il crée le premier worker (W2) :
  - il crée deux pipes anonymes :
        pipe(to_worker)
        pipe(from_worker)
  - il fork() et exécute :
        execv("./worker", {"worker", "2", fdIn, fdToMaster, NULL})
  - il garde les descripteurs pour écrire vers le worker et lire ses réponses.

2) BOUCLE PRINCIPALE DU MASTER
-------------------------------
- le master attend une demande d’un client via le tube nommé client_to_master.fifo.
- quand une demande arrive, il lit le nombre n.
- il envoie ce nombre dans le pipeline, en commençant par le premier worker (W2).
- il attend la réponse de l’un des workers via from_worker.
- quand il reçoit la réponse, il renvoie le résultat au client via master_to_client.fifo.
- puis il retourne attendre une nouvelle demande.

3) COMPORTEMENT D’UN WORKER
----------------------------
chaque worker est lancé avec :
    ./worker p fdIn fdToMaster

signification des arguments :
- p : nombre premier géré par ce worker.
- fdIn : pipe anonyme d’où le worker lit les nombres à tester (depuis le master ou un worker précédent).
- fdToMaster : pipe anonyme vers le master pour envoyer les résultats (premier / non premier).

BOUCLE DU WORKER :
------------------
1. le worker lit un nombre n depuis fdIn.
2. il teste plusieurs cas :
   - Cas 1 : n == p
       -> Le worker envoie "n est premier" au master.

   - Cas 2 : n % p == 0
       -> Le worker envoie "n n’est pas premier" au master.

   - Cas 3 : n % p != 0
       -> Le nombre n n’est pas divisible par p, donc il faut le transmettre plus loin.
         deux possibilités :
         a) il existe déjà un worker suivant :
             -> Le worker écrit n dans le pipe vers ce suivant.
         b) aucun worker suivant n’existe encore :
             -> n est un nouveau nombre premier.
             -> le worker crée un nouveau pipe anonyme vers le futur worker.
             -> il fork() et lance execv("./worker", {"worker", "<n>", fdInNext, fdToMaster, NULL}).
             -> le nouveau worker gère le nombre premier n.
             -> le worker parent garde le pipe pour transmettre les futurs nombres.
             -> le worker parent ne meurt pas, il continue à traiter d’autres nombres.

4. tous les workers partagent le même fdToMaster, donc chacun peut écrire un message au master.

4) COMPORTEMENT DU MASTER APRES LE FORK
----------------------------------------
- côté père (master) :
    - garde :
        to_worker[1] pour écrire vers W2
        from_worker[0] pour lire les réponses
    - ferme les extrémités inutiles :
        close(to_worker[0]);
        close(from_worker[1]);

- côté fils (worker) :
    - exécute "./worker p fdIn fdToMaster"
    - ferme les fd inutiles et entre dans sa boucle principale.

==============================
EXEMPLE CONCRET : n = 9
==============================

CLIENT -> MASTER : "est-ce que 9 est premier ?"
MASTER -> W2 : 9
W2 (P=2) : 9 % 2 != 0 -> transmet 9 à W3
W3 (P=3) : 9 % 3 == 0 -> envoie "9 n’est pas premier" au master
MASTER -> CLIENT : "9 n’est pas premier"

Résultat final : 9 n’est pas premier.

==============================
EXEMPLE CONCRET : n = 5
==============================

CLIENT → MASTER : "est-ce que 5 est premier ?"
MASTER → W2 : 5
W2 (P=2) : 5 % 2 != 0 -> pas divisible, pas de worker suivant -> crée W3
W3 (P=3) : 5 % 3 != 0 ->pas divisible, pas de worker suivant -> crée W5
W5 (P=5) : n == p -> envoie "5 est premier" au master
MASTER → CLIENT : "5 est premier"

resultat final : 5 est premier.


