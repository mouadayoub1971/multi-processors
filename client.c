/*
 * ============================================================================
 * CLIENT - Système de Distribution de Commandes
 * ============================================================================
 *
 * Auteur: Mouad
 * Date: Décembre 2025
 *
 * Description:
 *   Ce programme client permet d'envoyer un fichier de commandes shell au
 *   serveur maître pour une exécution distribuée sur les serveurs esclaves.
 *
 * Fonctionnement:
 *   1. Le client ouvre le fichier de commandes spécifié en argument
 *   2. Il se connecte au serveur maître via TCP (port 9999)
 *   3. Il envoie le nom du fichier de commandes au maître
 *   4. Il attend la confirmation "OK" du maître
 *   5. Il attend que les commandes soient exécutées (3 secondes)
 *   6. Il se déconnecte proprement
 *
 * Usage: client.exe <fichier_commandes>
 *   Exemple: client.exe test_commands.txt
 *
 * ============================================================================
 */

/* Inclusion des bibliothèques standard */
#include <stdio.h>      /* Pour les fonctions d'entrée/sortie (printf, fprintf, fopen, etc.) */
#include <stdlib.h>     /* Pour exit(), atoi() et autres fonctions utilitaires */
#include <string.h>     /* Pour les fonctions de manipulation de chaînes (strlen, strncmp, etc.) */

/* Inclusion des bibliothèques Windows Socket (Winsock2) */
#include <winsock2.h>   /* API principale Windows Socket version 2 */
#include <ws2tcpip.h>   /* Fonctions supplémentaires TCP/IP (inet_addr, etc.) */

/* Directive pour lier automatiquement la bibliothèque Winsock */
#pragma comment(lib, "ws2_32.lib")

/* ============================================================================
 * CONSTANTES DE CONFIGURATION
 * ============================================================================ */

#define MASTER_HOST "127.0.0.1"  /* Adresse IP du serveur maître (localhost) */
#define MASTER_PORT 9999          /* Port TCP du serveur maître */
#define MAX_CMD_LEN 1024          /* Longueur maximale d'une commande */

/* ============================================================================
 * STRUCTURES DE DONNÉES
 * ============================================================================ */

/*
 * Structure CommandResult
 * -----------------------
 * Représente le résultat d'une commande exécutée par un esclave.
 * Note: Cette structure n'est pas utilisée activement dans ce client,
 * mais est définie pour la compatibilité avec le protocole.
 */
typedef struct {
    char command[MAX_CMD_LEN];  /* La commande qui a été exécutée */
    int return_code;            /* Code de retour de la commande (0 = succès) */
    char result[256];           /* Message décrivant le résultat */
} CommandResult;

/* ============================================================================
 * FONCTION PRINCIPALE
 * ============================================================================ */

/*
 * Fonction main()
 * ---------------
 * Point d'entrée du programme client.
 *
 * Paramètres:
 *   argc - Nombre d'arguments de la ligne de commande
 *   argv - Tableau des arguments (argv[0] = nom du programme, argv[1] = fichier)
 *
 * Retourne:
 *   0 en cas de succès, 1 en cas d'erreur
 */
int main(int argc, char *argv[]) {

    /*
     * ÉTAPE 1: Vérification des arguments
     * ------------------------------------
     * Le programme nécessite exactement un argument: le nom du fichier
     * contenant les commandes à exécuter.
     */
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <command_file>\n", argv[0]);
        exit(1);
    }

    /* Stockage du nom du fichier de commandes */
    const char *command_file = argv[1];

    /*
     * ÉTAPE 2: Ouverture du fichier de commandes
     * -------------------------------------------
     * On vérifie que le fichier existe et peut être ouvert en lecture.
     * Le fichier contient les commandes shell à exécuter, une par ligne.
     */
    FILE *fp = fopen(command_file, "r");
    if (!fp) {
        fprintf(stderr, "Cannot open file: %s\n", command_file);
        exit(1);
    }

    /*
     * ÉTAPE 3: Initialisation de Winsock
     * -----------------------------------
     * Avant d'utiliser les sockets Windows, il faut initialiser la
     * bibliothèque Winsock avec WSAStartup().
     * MAKEWORD(2, 2) demande la version 2.2 de Winsock.
     */
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        fprintf(stderr, "WSAStartup failed: %d\n", WSAGetLastError());
        fclose(fp);
        exit(1);
    }

    /*
     * ÉTAPE 4: Création du socket TCP
     * --------------------------------
     * Création d'un socket pour la communication avec le serveur maître.
     * - AF_INET: Famille d'adresses IPv4
     * - SOCK_STREAM: Socket orienté connexion (TCP)
     * - IPPROTO_TCP: Protocole TCP
     */
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        fprintf(stderr, "socket failed: %d\n", WSAGetLastError());
        fclose(fp);
        WSACleanup();
        exit(1);
    }

    /*
     * ÉTAPE 5: Configuration de l'adresse du serveur maître
     * ------------------------------------------------------
     * Préparation de la structure sockaddr_in avec les informations
     * du serveur maître (adresse IP et port).
     */
    struct sockaddr_in master_addr;
    memset(&master_addr, 0, sizeof(master_addr));  /* Initialisation à zéro */
    master_addr.sin_family = AF_INET;              /* Famille IPv4 */
    master_addr.sin_port = htons(MASTER_PORT);     /* Port en format réseau (big-endian) */
    master_addr.sin_addr.s_addr = inet_addr(MASTER_HOST);  /* Conversion de l'adresse IP */

    /* Vérification que l'adresse IP est valide */
    if (master_addr.sin_addr.s_addr == INADDR_NONE) {
        fprintf(stderr, "Invalid master address: %s\n", MASTER_HOST);
        closesocket(sock);
        fclose(fp);
        WSACleanup();
        exit(1);
    }

    /*
     * ÉTAPE 6: Connexion au serveur maître
     * -------------------------------------
     * Établissement de la connexion TCP avec le serveur maître.
     * Cette opération est bloquante jusqu'à ce que la connexion soit
     * établie ou qu'une erreur survienne.
     */
    printf("[Client] Connexion au serveur maître %s:%d...\n", MASTER_HOST, MASTER_PORT);

    if (connect(sock, (struct sockaddr *)&master_addr, sizeof(master_addr)) == SOCKET_ERROR) {
        fprintf(stderr, "connect failed: %d\n", WSAGetLastError());
        closesocket(sock);
        fclose(fp);
        WSACleanup();
        exit(1);
    }

    printf("[Client] Connecté au serveur maître\n");

    /*
     * ÉTAPE 7: Envoi du nom du fichier au serveur maître
     * ---------------------------------------------------
     * Le client envoie le nom du fichier de commandes au maître.
     * Le maître utilisera ce nom pour ouvrir et lire le fichier localement.
     */
    if (send(sock, command_file, (int)strlen(command_file), 0) == SOCKET_ERROR) {
        fprintf(stderr, "send filename failed: %d\n", WSAGetLastError());
        closesocket(sock);
        fclose(fp);
        WSACleanup();
        exit(1);
    }

    printf("[Client] Fichier '%s' envoyé au maître\n", command_file);

    /*
     * ÉTAPE 8: Attente de l'accusé de réception (ACK) du maître
     * ----------------------------------------------------------
     * Le client attend que le serveur maître confirme la réception
     * et l'acceptation du fichier de commandes.
     * Réponse attendue: "OK" si succès, "ERROR: ..." si échec.
     */
    char ack[256];
    int n = recv(sock, ack, sizeof(ack) - 1, 0);
    if (n <= 0) {
        fprintf(stderr, "No response from master\n");
        closesocket(sock);
        fclose(fp);
        WSACleanup();
        exit(1);
    }
    ack[n] = '\0';  /* Terminaison de la chaîne */

    /* Vérification que le maître a accepté les commandes */
    if (strncmp(ack, "OK", 2) != 0) {
        fprintf(stderr, "Master error: %s\n", ack);
        closesocket(sock);
        fclose(fp);
        WSACleanup();
        exit(1);
    }

    printf("[Client] Maître a accepté les commandes\n");

    /*
     * ÉTAPE 9: Attente de l'exécution des commandes
     * ----------------------------------------------
     * Le client attend un temps fixe pour permettre aux serveurs
     * esclaves d'exécuter les commandes.
     *
     * Note: Dans une implémentation réelle, le client pourrait
     * recevoir les résultats des commandes via UDP ou un callback.
     */
    printf("[Client] Attente de l'exécution des commandes...\n");
    Sleep(3000);  /* Pause de 3000 millisecondes (3 secondes) */

    printf("[Client] Commandes traitées\n");

    /*
     * ÉTAPE 10: Nettoyage et fermeture
     * ---------------------------------
     * Fermeture propre de toutes les ressources utilisées:
     * - Le socket de connexion
     * - Le fichier de commandes
     * - La bibliothèque Winsock
     */
    closesocket(sock);  /* Fermeture du socket */
    fclose(fp);         /* Fermeture du fichier */
    WSACleanup();       /* Libération des ressources Winsock */

    return 0;  /* Succès */
}
