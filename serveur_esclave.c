/*
 * ============================================================================
 * SERVEUR ESCLAVE - Système de Distribution de Commandes
 * ============================================================================
 *
 * Auteur: Mouad
 * Date: Décembre 2025
 *
 * Description:
 *   Ce programme représente un serveur esclave dans l'architecture maître-esclaves.
 *   Il reçoit des commandes shell du serveur maître via UDP, les exécute
 *   localement, et renvoie les résultats.
 *
 * Fonctionnement:
 *   1. Le serveur démarre et écoute sur un port UDP spécifié
 *   2. Il attend les requêtes de commande (CommandRequest) du maître
 *   3. Pour chaque commande reçue:
 *      a. Il exécute la commande avec system()
 *      b. Il capture le code de retour
 *      c. Il renvoie le résultat (CommandResult) au maître
 *   4. Il retourne à l'écoute pour la prochaine commande
 *
 * Usage: serveur_esclave.exe <port>
 *   Exemple: serveur_esclave.exe 10001
 *
 * Protocole:
 *   - Entrée: CommandRequest via UDP (commande + info client)
 *   - Sortie: CommandResult via UDP (commande + code retour + message)
 *
 * ============================================================================
 */

/* Inclusion des bibliothèques standard */
#include <stdio.h>      /* Pour les fonctions d'entrée/sortie (printf, fprintf, etc.) */
#include <stdlib.h>     /* Pour exit(), atoi(), system() et autres fonctions utilitaires */
#include <string.h>     /* Pour les fonctions de manipulation de chaînes (strcpy, sprintf, etc.) */

/* Inclusion des bibliothèques Windows Socket (Winsock2) */
#include <winsock2.h>   /* API principale Windows Socket version 2 */
#include <ws2tcpip.h>   /* Fonctions supplémentaires TCP/IP */
#include <process.h>    /* Pour getpid() - obtenir l'ID du processus */

/* Directives pour lier automatiquement les bibliothèques nécessaires */
#pragma comment(lib, "ws2_32.lib")   /* Bibliothèque Winsock */
#pragma comment(lib, "winmm.lib")    /* Bibliothèque multimédia Windows */

/* ============================================================================
 * CONSTANTES DE CONFIGURATION
 * ============================================================================ */

#define MAX_CMD_LEN 1024     /* Longueur maximale d'une commande shell */
#define MAX_RESULT_LEN 2048  /* Longueur maximale d'un résultat (non utilisé actuellement) */

/* ============================================================================
 * STRUCTURES DE DONNÉES
 * ============================================================================ */

/*
 * Structure CommandRequest
 * ------------------------
 * Représente une requête de commande envoyée par le serveur maître.
 * Cette structure est reçue via UDP et contient toutes les informations
 * nécessaires pour exécuter et tracer une commande.
 *
 * Champs:
 *   - command: La commande shell à exécuter
 *   - client_addr: Adresse IP du client original (pour traçabilité)
 *   - client_port: Port du client original (pour traçabilité)
 */
typedef struct {
    char command[MAX_CMD_LEN];   /* Commande shell à exécuter */
    char client_addr[50];        /* Adresse IP du client (ex: "127.0.0.1") */
    int client_port;             /* Port du client */
} CommandRequest;

/*
 * Structure CommandResult
 * -----------------------
 * Représente le résultat d'une commande exécutée.
 * Cette structure est renvoyée au serveur maître via UDP.
 *
 * Champs:
 *   - command: La commande qui a été exécutée (pour correspondance)
 *   - return_code: Code de retour de system() (0 = succès)
 *   - result: Message textuel décrivant le résultat
 */
typedef struct {
    char command[MAX_CMD_LEN];   /* Commande exécutée */
    int return_code;             /* Code de retour (0 = succès, autre = erreur) */
    char result[256];            /* Message de résultat */
} CommandResult;

/* ============================================================================
 * FONCTIONS UTILITAIRES
 * ============================================================================ */

/*
 * Fonction signal_handler()
 * -------------------------
 * Gestionnaire de signal pour l'arrêt propre du serveur.
 * Appelé lors de la réception d'un signal d'interruption (Ctrl+C).
 *
 * Paramètre:
 *   sig - Numéro du signal reçu
 *
 * Note: Sur Windows, cette fonction n'est pas connectée aux signaux POSIX
 * standards, mais est conservée pour la compatibilité cross-platform.
 */
void signal_handler(int sig) {
    printf("\n[Slave Server] Arrêt du serveur esclave...\n");
    exit(0);
}

/* ============================================================================
 * FONCTION PRINCIPALE
 * ============================================================================ */

/*
 * Fonction main()
 * ---------------
 * Point d'entrée du serveur esclave.
 * Configure le socket UDP, puis entre dans une boucle infinie
 * d'attente et d'exécution de commandes.
 *
 * Paramètres:
 *   argc - Nombre d'arguments (doit être 2)
 *   argv - argv[1] = port d'écoute UDP
 *
 * Retourne:
 *   0 en cas de succès (jamais atteint en fonctionnement normal)
 *   1 en cas d'erreur d'initialisation
 */
int main(int argc, char *argv[]) {

    /*
     * ÉTAPE 1: Vérification des arguments
     * ------------------------------------
     * Le programme nécessite exactement un argument: le numéro de port
     * sur lequel le serveur esclave doit écouter.
     */
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    /* Conversion du port de chaîne en entier */
    int port = atoi(argv[1]);

    /* Déclaration des variables */
    SOCKET sock;                        /* Socket UDP du serveur */
    struct sockaddr_in server_addr;     /* Adresse du serveur (ce programme) */
    struct sockaddr_in client_addr;     /* Adresse du client (serveur maître) */
    int client_addr_len;                /* Taille de l'adresse client */
    CommandRequest request;             /* Structure pour recevoir les requêtes */
    CommandResult result;               /* Structure pour envoyer les résultats */

    /*
     * ÉTAPE 2: Initialisation de Winsock
     * -----------------------------------
     * Initialisation obligatoire de la bibliothèque Winsock avant
     * toute utilisation des fonctions socket sous Windows.
     */
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        fprintf(stderr, "WSAStartup failed: %d\n", WSAGetLastError());
        exit(1);
    }

    /*
     * ÉTAPE 3: Création du socket UDP
     * --------------------------------
     * Création d'un socket pour la communication avec le serveur maître.
     * - AF_INET: Famille d'adresses IPv4
     * - SOCK_DGRAM: Socket datagramme (UDP) - sans connexion
     * - IPPROTO_UDP: Protocole UDP
     *
     * UDP est choisi pour sa simplicité et sa faible latence,
     * bien qu'il ne garantisse pas la livraison des paquets.
     */
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        fprintf(stderr, "socket failed: %d\n", WSAGetLastError());
        WSACleanup();
        exit(1);
    }

    /*
     * ÉTAPE 4: Configuration et liaison du socket (bind)
     * ---------------------------------------------------
     * Configuration de l'adresse locale du serveur et association
     * du socket au port spécifié.
     *
     * - sin_family: IPv4
     * - sin_port: Port converti en format réseau (big-endian)
     * - sin_addr: INADDR_ANY = écouter sur toutes les interfaces
     */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);              /* Conversion host-to-network */
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); /* Écoute sur toutes les interfaces */

    /* Liaison du socket au port - permet de recevoir des paquets sur ce port */
    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        fprintf(stderr, "bind failed: %d\n", WSAGetLastError());
        closesocket(sock);
        WSACleanup();
        exit(1);
    }

    /* Affichage du message de démarrage avec le PID pour identification */
    printf("[Slave Server] Esclave lancé sur le port %d (PID=%d)\n", port, getpid());

    /*
     * ÉTAPE 5: Boucle principale du serveur
     * --------------------------------------
     * Boucle infinie qui:
     * 1. Attend une requête de commande du maître
     * 2. Exécute la commande
     * 3. Renvoie le résultat
     * 4. Recommence
     */
    while (1) {
        /* Préparation pour la réception */
        client_addr_len = sizeof(client_addr);

        /*
         * ÉTAPE 5a: Réception d'une commande du maître
         * ---------------------------------------------
         * recvfrom() est bloquant - le serveur attend ici jusqu'à
         * recevoir un datagramme UDP.
         *
         * Paramètres:
         *   - sock: Socket UDP
         *   - &request: Buffer pour stocker les données reçues
         *   - sizeof(request): Taille maximale à recevoir
         *   - 0: Pas de flags spéciaux
         *   - &client_addr: Stocke l'adresse de l'expéditeur
         *   - &client_addr_len: Taille de l'adresse
         */
        int n = recvfrom(sock, (char *)&request, sizeof(request), 0,
                        (struct sockaddr *)&client_addr, &client_addr_len);

        /* Vérification des erreurs de réception */
        if (n == SOCKET_ERROR) {
            fprintf(stderr, "recvfrom failed: %d\n", WSAGetLastError());
            continue;  /* Ignorer l'erreur et attendre la prochaine requête */
        }

        /* Affichage de la commande reçue avec les informations du client */
        printf("[Slave Server] Reçu commande: %s (de %s:%d)\n",
               request.command, request.client_addr, request.client_port);

        /*
         * ÉTAPE 5b: Exécution de la commande
         * -----------------------------------
         * La fonction system() exécute la commande dans un shell.
         * Elle retourne:
         *   - 0: Succès
         *   - Valeur positive: Code d'erreur de la commande
         *   - Valeur négative: Erreur système (impossible d'exécuter)
         *
         * ATTENTION: L'utilisation de system() avec des entrées non
         * validées présente des risques de sécurité (injection de commandes).
         */
        int ret = system(request.command);

        /*
         * ÉTAPE 5c: Préparation du résultat
         * ----------------------------------
         * Construction de la structure CommandResult avec:
         * - La commande exécutée (pour correspondance)
         * - Le code de retour
         * - Un message descriptif du résultat
         */
        strcpy(result.command, request.command);
        result.return_code = ret;

        /* Génération du message de résultat selon le code de retour */
        if (ret < 0) {
            /* Erreur système - impossible d'exécuter la commande */
            strcpy(result.result, "Erreur: impossible d'exécuter la commande");
        } else if (ret > 0) {
            /* La commande a retourné une erreur */
            sprintf(result.result, "Erreur d'exécution (code: %d)", ret);
        } else {
            /* Succès - code de retour 0 */
            strcpy(result.result, "Commande exécutée avec succès");
        }

        /* Affichage du résultat dans la console du serveur */
        printf("[Slave Server] Résultat: %s (code=%d)\n", result.result, ret);

        /*
         * ÉTAPE 5d: Envoi du résultat au serveur maître
         * ----------------------------------------------
         * sendto() envoie le résultat à l'adresse du maître
         * (stockée dans client_addr lors de la réception).
         */
        if (sendto(sock, (const char *)&result, sizeof(result), 0,
                  (struct sockaddr *)&client_addr, client_addr_len) == SOCKET_ERROR) {
            fprintf(stderr, "sendto failed: %d\n", WSAGetLastError());
        }

        /* Retour au début de la boucle pour attendre la prochaine commande */
    }

    /*
     * ÉTAPE 6: Nettoyage (jamais atteint en fonctionnement normal)
     * -------------------------------------------------------------
     * Ces lignes ne sont jamais exécutées car le serveur tourne
     * indéfiniment. Elles sont présentes pour la complétude du code.
     */
    closesocket(sock);
    WSACleanup();
    return 0;
}
