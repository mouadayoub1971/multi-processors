#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#define MASTER_HOST "127.0.0.1"
#define MASTER_PORT 9999
#define MAX_CMD_LEN 1024

typedef struct {
    char command[MAX_CMD_LEN];
    int return_code;
    char result[256];
} CommandResult;

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <command_file>\n", argv[0]);
        exit(1);
    }

    const char *command_file = argv[1];

    // Open command file
    FILE *fp = fopen(command_file, "r");
    if (!fp) {
        fprintf(stderr, "Cannot open file: %s\n", command_file);
        exit(1);
    }

    // Initialize Winsock
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        fprintf(stderr, "WSAStartup failed: %d\n", WSAGetLastError());
        fclose(fp);
        exit(1);
    }

    // Connect to master server
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        fprintf(stderr, "socket failed: %d\n", WSAGetLastError());
        fclose(fp);
        WSACleanup();
        exit(1);
    }

    struct sockaddr_in master_addr;
    memset(&master_addr, 0, sizeof(master_addr));
    master_addr.sin_family = AF_INET;
    master_addr.sin_port = htons(MASTER_PORT);
    master_addr.sin_addr.s_addr = inet_addr(MASTER_HOST);
    
    if (master_addr.sin_addr.s_addr == INADDR_NONE) {
        fprintf(stderr, "Invalid master address: %s\n", MASTER_HOST);
        closesocket(sock);
        fclose(fp);
        WSACleanup();
        exit(1);
    }

    printf("[Client] Connexion au serveur maître %s:%d...\n", MASTER_HOST, MASTER_PORT);

    if (connect(sock, (struct sockaddr *)&master_addr, sizeof(master_addr)) == SOCKET_ERROR) {
        fprintf(stderr, "connect failed: %d\n", WSAGetLastError());
        closesocket(sock);
        fclose(fp);
        WSACleanup();
        exit(1);
    }

    printf("[Client] Connecté au serveur maître\n");

    // Send filename to master
    if (send(sock, command_file, (int)strlen(command_file), 0) == SOCKET_ERROR) {
        fprintf(stderr, "send filename failed: %d\n", WSAGetLastError());
        closesocket(sock);
        fclose(fp);
        WSACleanup();
        exit(1);
    }

    printf("[Client] Fichier '%s' envoyé au maître\n", command_file);

    // Wait for ACK from master
    char ack[256];
    int n = recv(sock, ack, sizeof(ack) - 1, 0);
    if (n <= 0) {
        fprintf(stderr, "No response from master\n");
        closesocket(sock);
        fclose(fp);
        WSACleanup();
        exit(1);
    }
    ack[n] = '\0';

    if (strncmp(ack, "OK", 2) != 0) {
        fprintf(stderr, "Master error: %s\n", ack);
        closesocket(sock);
        fclose(fp);
        WSACleanup();
        exit(1);
    }

    printf("[Client] Maître a accepté les commandes\n");

    // Now wait for command results from slave servers (via UDP or alternative method)
    // Note: In a real implementation, the slave servers would send results back to the client
    // For now, we'll wait a bit to let commands execute
    printf("[Client] Attente de l'exécution des commandes...\n");
    Sleep(3000);  // Windows Sleep in milliseconds

    printf("[Client] Commandes traitées\n");

    closesocket(sock);
    fclose(fp);
    WSACleanup();
    return 0;
}
