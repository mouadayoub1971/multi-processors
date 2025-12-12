/*
 * ============================================================================
 * SERVEUR MAÎTRE - Système de Distribution de Commandes
 * ============================================================================
 *
 * Auteur: Mouad
 * Date: Décembre 2025
 *
 * Description:
 *   Ce programme représente le serveur maître (coordinateur) dans l'architecture
 *   maître-esclaves. Il accepte les connexions des clients via TCP, lit les
 *   fichiers de commandes, et distribue les commandes aux serveurs esclaves
 *   via UDP pour une exécution parallèle.
 *
 * Architecture:
 *   - Communication Client-Maître: TCP sur port 9999
 *   - Communication Maître-Esclaves: UDP sur ports configurés (10001, 10002, ...)
 *
 * Fonctionnement:
 *   1. Le serveur charge la configuration des esclaves depuis slaves.conf
 *   2. Il écoute les connexions clients sur le port TCP 9999
 *   3. Pour chaque client connecté:
 *      a. Il reçoit le nom du fichier de commandes
 *      b. Il ouvre et lit le fichier ligne par ligne
 *      c. Pour chaque commande, il trouve un esclave disponible
 *      d. Il envoie la commande à l'esclave via UDP
 *   4. Il ferme la connexion client et attend le prochain
 *
 * Usage: serveur_maitre.exe <fichier_config_esclaves>
 *   Exemple: serveur_maitre.exe slaves.conf
 *
 * Format du fichier de configuration (slaves.conf):
 *   hostname port
 *   Exemple:
 *     localhost 10001
 *     localhost 10002
 *     localhost 10003
 *
 * ============================================================================
 */

/* Inclusion des bibliothèques standard */
#include <stdio.h>      /* Pour les fonctions d'entrée/sortie (printf, fprintf, fopen, etc.) */
#include <stdlib.h>     /* Pour exit(), atoi() et autres fonctions utilitaires */
#include <string.h>     /* Pour les fonctions de manipulation de chaînes */
#include <errno.h>      /* Pour les codes d'erreur système */

/* Inclusion des bibliothèques Windows Socket (Winsock2) */
#include <winsock2.h>   /* API principale Windows Socket version 2 */
#include <ws2tcpip.h>   /* Fonctions supplémentaires TCP/IP */
#include <process.h>    /* Pour _getpid() - obtenir l'ID du processus */

/* Directives pour lier automatiquement les bibliothèques nécessaires */
#pragma comment(lib, "ws2_32.lib")   /* Bibliothèque Winsock */
#pragma comment(lib, "winmm.lib")    /* Bibliothèque multimédia Windows */

/* ============================================================================
 * CONSTANTES DE CONFIGURATION
 * ============================================================================ */

#define MAX_CMD_LEN 1024     /* Longueur maximale d'une commande shell */
#define MAX_SLAVES 10        /* Nombre maximum de serveurs esclaves supportés */
#define MAX_CLIENTS 100      /* Nombre maximum de clients simultanés (non utilisé) */
#define MASTER_PORT 9999     /* Port TCP sur lequel le maître écoute les clients */

/* ============================================================================
 * STRUCTURES DE DONNÉES
 * ============================================================================ */

/*
 * Structure CommandRequest
 * ------------------------
 * Représente une requête de commande envoyée aux serveurs esclaves.
 * Contient la commande à exécuter ainsi que les informations du client
 * original pour la traçabilité.
 *
 * Champs:
 *   - command: La commande shell à exécuter
 *   - client_addr: Adresse IP du client original
 *   - client_port: Port du client original
 */
typedef struct {
    char command[MAX_CMD_LEN];   /* Commande shell à exécuter */
    char client_addr[50];        /* Adresse IP du client (ex: "127.0.0.1") */
    int client_port;             /* Port du client */
} CommandRequest;

/*
 * Structure CommandResult
 * -----------------------
 * Représente le résultat d'une commande exécutée par un esclave.
 * Utilisé pour recevoir les résultats des esclaves.
 *
 * Champs:
 *   - command: La commande qui a été exécutée
 *   - return_code: Code de retour de la commande (0 = succès)
 *   - result: Message textuel décrivant le résultat
 */
typedef struct {
    char command[MAX_CMD_LEN];   /* Commande exécutée */
    int return_code;             /* Code de retour (0 = succès, autre = erreur) */
    char result[256];            /* Message de résultat */
} CommandResult;

/*
 * Structure SlaveServer
 * ---------------------
 * Représente un serveur esclave dans le système.
 * Contient toutes les informations nécessaires pour communiquer
 * avec l'esclave via UDP.
 *
 * Champs:
 *   - hostname: Nom d'hôte ou adresse IP de l'esclave
 *   - port: Port UDP de l'esclave
 *   - sock: Socket UDP utilisé pour communiquer avec cet esclave
 *   - addr: Structure sockaddr_in pré-configurée pour l'envoi
 *   - available: Indicateur de disponibilité (1 = disponible)
 */
typedef struct {
    char hostname[256];          /* Nom d'hôte de l'esclave */
    int port;                    /* Port UDP de l'esclave */
    SOCKET sock;                 /* Socket UDP pour cet esclave */
    struct sockaddr_in addr;     /* Adresse socket pré-configurée */
    int available;               /* 1 si disponible, 0 sinon */
} SlaveServer;

/* ============================================================================
 * VARIABLES GLOBALES
 * ============================================================================ */

SlaveServer slaves[MAX_SLAVES];  /* Tableau des serveurs esclaves */
int num_slaves = 0;              /* Nombre d'esclaves chargés */

/* ============================================================================
 * FONCTIONS UTILITAIRES
 * ============================================================================ */

/*
 * Fonction signal_handler()
 * -------------------------
 * Gestionnaire de signal pour l'arrêt propre du serveur.
 * Ferme tous les sockets esclaves et libère les ressources Winsock.
 *
 * Paramètre:
 *   sig - Numéro du signal reçu
 */
void signal_handler(int sig) {
    printf("\n[Master Server] Arrêt du serveur maître...\n");

    /* Fermeture de tous les sockets esclaves */
    for (int i = 0; i < num_slaves; i++) {
        if (slaves[i].sock != INVALID_SOCKET) {
            closesocket(slaves[i].sock);
        }
    }

    /* Libération des ressources Winsock */
    WSACleanup();
    exit(0);
}

/*
 * Fonction load_slaves_config()
 * -----------------------------
 * Charge la configuration des serveurs esclaves depuis un fichier.
 * Crée un socket UDP pour chaque esclave et résout son adresse.
 *
 * Format du fichier:
 *   hostname port
 *   (une ligne par esclave, les lignes commençant par # sont ignorées)
 *
 * Paramètre:
 *   config_file - Chemin vers le fichier de configuration
 *
 * Retourne:
 *   Nombre d'esclaves chargés avec succès, ou -1 en cas d'erreur
 */
int load_slaves_config(const char *config_file) {
    /* Ouverture du fichier de configuration */
    FILE *fp = fopen(config_file, "r");
    if (!fp) {
        fprintf(stderr, "Cannot open config file: %s\n", config_file);
        return -1;
    }

    /* Réinitialisation du compteur d'esclaves */
    num_slaves = 0;
    char line[512];

    /* Lecture ligne par ligne du fichier */
    while (fgets(line, sizeof(line), fp) && num_slaves < MAX_SLAVES) {
        /* Suppression du caractère de nouvelle ligne */
        line[strcspn(line, "\n")] = 0;

        /* Ignorer les lignes vides et les commentaires */
        if (line[0] == '\0' || line[0] == '#') continue;

        /* Extraction du hostname et du port */
        char hostname[256];
        int port;
        if (sscanf(line, "%255s %d", hostname, &port) != 2) {
            fprintf(stderr, "Invalid config line: %s\n", line);
            continue;
        }

        /* Stockage des informations de l'esclave */
        strcpy(slaves[num_slaves].hostname, hostname);
        slaves[num_slaves].port = port;
        slaves[num_slaves].available = 1;  /* Disponible par défaut */

        /*
         * Création du socket UDP pour cet esclave
         * Chaque esclave a son propre socket pour permettre
         * l'envoi de commandes en parallèle si nécessaire.
         */
        slaves[num_slaves].sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (slaves[num_slaves].sock == INVALID_SOCKET) {
            fprintf(stderr, "socket failed: %d\n", WSAGetLastError());
            continue;
        }

        /*
         * Résolution du nom d'hôte en adresse IP
         * gethostbyname() convertit "localhost" en 127.0.0.1, etc.
         */
        struct hostent *he = gethostbyname(hostname);
        if (!he) {
            fprintf(stderr, "Cannot resolve hostname: %s\n", hostname);
            closesocket(slaves[num_slaves].sock);
            continue;
        }

        /* Configuration de la structure d'adresse pour l'esclave */
        memset(&slaves[num_slaves].addr, 0, sizeof(slaves[num_slaves].addr));
        slaves[num_slaves].addr.sin_family = AF_INET;
        slaves[num_slaves].addr.sin_port = htons(port);
        memcpy(&slaves[num_slaves].addr.sin_addr, he->h_addr_list[0], he->h_length);

        printf("[Master Server] Loaded slave: %s:%d\n", hostname, port);
        num_slaves++;
    }

    fclose(fp);
    return num_slaves;
}

/*
 * Fonction find_available_slave()
 * -------------------------------
 * Recherche un serveur esclave disponible pour traiter une commande.
 * Utilise une stratégie simple: premier disponible trouvé.
 *
 * Note: Dans cette implémentation, tous les esclaves sont toujours
 * marqués comme disponibles. Une amélioration serait d'implémenter
 * un vrai suivi de disponibilité basé sur les réponses des esclaves.
 *
 * Retourne:
 *   Index de l'esclave disponible, ou -1 si aucun disponible
 */
int find_available_slave() {
    for (int i = 0; i < num_slaves; i++) {
        if (slaves[i].available) {
            return i;
        }
    }
    return -1;  /* Aucun esclave disponible */
}

/* ============================================================================
 * FONCTION PRINCIPALE
 * ============================================================================ */

/*
 * Fonction main()
 * ---------------
 * Point d'entrée du serveur maître.
 * Initialise le système, charge la configuration, puis entre dans
 * la boucle principale d'acceptation des clients et de distribution
 * des commandes.
 *
 * Paramètres:
 *   argc - Nombre d'arguments (doit être 2)
 *   argv - argv[1] = fichier de configuration des esclaves
 *
 * Retourne:
 *   0 en cas de succès (jamais atteint en fonctionnement normal)
 *   1 en cas d'erreur d'initialisation
 */
int main(int argc, char *argv[]) {

    /*
     * ÉTAPE 1: Vérification des arguments
     * ------------------------------------
     * Le programme nécessite exactement un argument: le chemin vers
     * le fichier de configuration des serveurs esclaves.
     */
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <slaves_config_file>\n", argv[0]);
        exit(1);
    }

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
     * ÉTAPE 3: Chargement de la configuration des esclaves
     * -----------------------------------------------------
     * Lecture du fichier de configuration pour obtenir la liste
     * des serveurs esclaves disponibles et création des sockets UDP.
     */
    if (load_slaves_config(argv[1]) <= 0) {
        fprintf(stderr, "Error: No slave servers loaded\n");
        WSACleanup();
        exit(1);
    }

    /*
     * ÉTAPE 4: Création du socket TCP maître
     * ---------------------------------------
     * Création d'un socket TCP pour accepter les connexions des clients.
     * - AF_INET: Famille d'adresses IPv4
     * - SOCK_STREAM: Socket orienté connexion (TCP)
     * - IPPROTO_TCP: Protocole TCP
     */
    SOCKET master_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (master_sock == INVALID_SOCKET) {
        fprintf(stderr, "socket failed: %d\n", WSAGetLastError());
        WSACleanup();
        exit(1);
    }

    /*
     * ÉTAPE 5: Configuration de l'adresse du serveur maître
     * ------------------------------------------------------
     * Préparation de la structure sockaddr_in pour le binding.
     */
    struct sockaddr_in master_addr;
    memset(&master_addr, 0, sizeof(master_addr));
    master_addr.sin_family = AF_INET;
    master_addr.sin_port = htons(MASTER_PORT);      /* Port 9999 en format réseau */
    master_addr.sin_addr.s_addr = htonl(INADDR_ANY); /* Écouter sur toutes les interfaces */

    /*
     * Option SO_REUSEADDR
     * -------------------
     * Permet de réutiliser le port immédiatement après l'arrêt du serveur.
     * Sans cette option, il faudrait attendre que le système libère le port.
     */
    int opt = 1;
    setsockopt(master_sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    /*
     * ÉTAPE 6: Liaison du socket au port (bind)
     * ------------------------------------------
     * Association du socket à l'adresse et au port configurés.
     */
    if (bind(master_sock, (struct sockaddr *)&master_addr, sizeof(master_addr)) == SOCKET_ERROR) {
        fprintf(stderr, "bind failed: %d\n", WSAGetLastError());
        closesocket(master_sock);
        WSACleanup();
        exit(1);
    }

    /*
     * ÉTAPE 7: Mise en écoute du socket (listen)
     * -------------------------------------------
     * Le socket commence à écouter les connexions entrantes.
     * Le paramètre 5 est la taille de la file d'attente des connexions.
     */
    if (listen(master_sock, 5) == SOCKET_ERROR) {
        fprintf(stderr, "listen failed: %d\n", WSAGetLastError());
        closesocket(master_sock);
        WSACleanup();
        exit(1);
    }

    /* Affichage du message de démarrage */
    printf("[Master Server] Maître lancé sur le port %d avec %d esclaves (PID=%d)\n",
           MASTER_PORT, num_slaves, _getpid());

    /*
     * ÉTAPE 8: Boucle principale du serveur
     * --------------------------------------
     * Boucle infinie qui:
     * 1. Accepte une connexion client
     * 2. Reçoit le nom du fichier de commandes
     * 3. Distribue les commandes aux esclaves
     * 4. Ferme la connexion client
     * 5. Recommence
     */
    while (1) {
        struct sockaddr_in client_addr;
        int client_addr_len = sizeof(client_addr);

        /*
         * ÉTAPE 8a: Acceptation d'une connexion client
         * ---------------------------------------------
         * accept() est bloquant - le serveur attend ici jusqu'à
         * ce qu'un client se connecte.
         */
        SOCKET client_sock = accept(master_sock, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_sock == INVALID_SOCKET) {
            fprintf(stderr, "accept failed: %d\n", WSAGetLastError());
            continue;
        }

        /* Extraction des informations du client connecté */
        char *client_ip = inet_ntoa(client_addr.sin_addr);  /* Conversion IP en chaîne */
        int client_port = ntohs(client_addr.sin_port);       /* Conversion port en entier */

        printf("[Master Server] Nouvelle connexion client: %s:%d\n", client_ip, client_port);

        /*
         * ÉTAPE 8b: Réception du nom de fichier
         * --------------------------------------
         * Le client envoie le nom du fichier contenant les commandes.
         */
        char filename[256];
        int n = recv(client_sock, filename, sizeof(filename) - 1, 0);
        if (n <= 0) {
            fprintf(stderr, "Error reading filename from client\n");
            closesocket(client_sock);
            continue;
        }
        filename[n] = '\0';  /* Terminaison de la chaîne */
        printf("[Master Server] Fichier demandé: %s\n", filename);

        /*
         * ÉTAPE 8c: Ouverture du fichier de commandes
         * --------------------------------------------
         * Le maître ouvre le fichier localement pour lire les commandes.
         */
        FILE *fp = fopen(filename, "r");
        if (!fp) {
            /* Envoi d'un message d'erreur au client */
            char error_msg[] = "ERROR: Cannot open file";
            send(client_sock, error_msg, (int)strlen(error_msg), 0);
            closesocket(client_sock);
            continue;
        }

        char line[MAX_CMD_LEN];
        int cmd_count = 0;  /* Compteur de commandes traitées */

        /*
         * ÉTAPE 8d: Envoi de l'accusé de réception (ACK)
         * -----------------------------------------------
         * Confirmation au client que le fichier a été ouvert avec succès.
         */
        char ack[] = "OK";
        send(client_sock, ack, (int)strlen(ack), 0);

        /*
         * ÉTAPE 8e: Traitement des commandes
         * -----------------------------------
         * Lecture du fichier ligne par ligne et distribution
         * des commandes aux esclaves.
         */
        while (fgets(line, sizeof(line), fp)) {
            /* Suppression du caractère de nouvelle ligne */
            line[strcspn(line, "\n")] = 0;

            /* Ignorer les lignes vides */
            if (line[0] == '\0') continue;

            printf("[Master Server] Traitement commande: %s\n", line);

            /*
             * Recherche d'un esclave disponible
             * ---------------------------------
             * Tentative de trouver un esclave libre pour exécuter la commande.
             * Si aucun n'est disponible, attendre 1 seconde et réessayer.
             */
            int slave_idx = find_available_slave();
            if (slave_idx < 0) {
                printf("[Master Server] Aucun esclave disponible, attente...\n");
                Sleep(1000);  /* Attente de 1 seconde */
                slave_idx = find_available_slave();
                if (slave_idx < 0) {
                    fprintf(stderr, "No available slaves\n");
                    continue;  /* Passer à la commande suivante */
                }
            }

            /*
             * Préparation de la requête de commande
             * -------------------------------------
             * Construction de la structure CommandRequest avec
             * la commande et les informations du client.
             */
            CommandRequest req;
            strcpy(req.command, line);
            strcpy(req.client_addr, client_ip);
            req.client_port = client_port;

            /*
             * Envoi de la commande à l'esclave via UDP
             * -----------------------------------------
             * sendto() envoie la requête à l'esclave sélectionné.
             */
            if (sendto(slaves[slave_idx].sock, (const char *)&req, sizeof(req), 0,
                      (struct sockaddr *)&slaves[slave_idx].addr,
                      sizeof(slaves[slave_idx].addr)) == SOCKET_ERROR) {
                fprintf(stderr, "sendto to slave failed: %d\n", WSAGetLastError());
                continue;
            }

            printf("[Master Server] Commande envoyée à %s:%d\n",
                   slaves[slave_idx].hostname, slaves[slave_idx].port);

            cmd_count++;
        }

        /* Fermeture du fichier de commandes */
        fclose(fp);

        /* Affichage du résumé pour ce client */
        printf("[Master Server] %d commandes traitées pour le client %s:%d\n",
               cmd_count, client_ip, client_port);

        /* Fermeture de la connexion avec le client */
        closesocket(client_sock);
    }

    /*
     * ÉTAPE 9: Nettoyage (jamais atteint en fonctionnement normal)
     * -------------------------------------------------------------
     * Ces lignes ne sont jamais exécutées car le serveur tourne
     * indéfiniment. Elles sont présentes pour la complétude du code.
     */
    closesocket(master_sock);
    WSACleanup();
    return 0;
}
