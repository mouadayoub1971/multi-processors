#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winmm.lib")

#define MAX_CMD_LEN 1024
#define MAX_RESULT_LEN 2048

typedef struct {
    char command[MAX_CMD_LEN];
    char client_addr[50];
    int client_port;
} CommandRequest;

typedef struct {
    char command[MAX_CMD_LEN];
    int return_code;
    char result[256];
} CommandResult;

void signal_handler(int sig) {
    printf("\n[Slave Server] Arrêt du serveur esclave...\n");
    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    int port = atoi(argv[1]);
    SOCKET sock;
    struct sockaddr_in server_addr, client_addr;
    int client_addr_len;
    CommandRequest request;
    CommandResult result;

    // Initialize Winsock
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        fprintf(stderr, "WSAStartup failed: %d\n", WSAGetLastError());
        exit(1);
    }

    // Create UDP socket
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        fprintf(stderr, "socket failed: %d\n", WSAGetLastError());
        WSACleanup();
        exit(1);
    }

    // Bind socket to port
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        fprintf(stderr, "bind failed: %d\n", WSAGetLastError());
        closesocket(sock);
        WSACleanup();
        exit(1);
    }

    printf("[Slave Server] Esclave lancé sur le port %d (PID=%d)\n", port, getpid());

    // Main server loop
    while (1) {
        client_addr_len = sizeof(client_addr);

        // Receive command from master
        int n = recvfrom(sock, (char *)&request, sizeof(request), 0,
                        (struct sockaddr *)&client_addr, &client_addr_len);
        if (n == SOCKET_ERROR) {
            fprintf(stderr, "recvfrom failed: %d\n", WSAGetLastError());
            continue;
        }

        printf("[Slave Server] Reçu commande: %s (de %s:%d)\n", 
               request.command, request.client_addr, request.client_port);

        // Execute the command
        int ret = system(request.command);

        // Prepare result
        strcpy(result.command, request.command);
        result.return_code = ret;
        
        if (ret < 0) {
            strcpy(result.result, "Erreur: impossible d'exécuter la commande");
        } else if (ret > 0) {
            sprintf(result.result, "Erreur d'exécution (code: %d)", ret);
        } else {
            strcpy(result.result, "Commande exécutée avec succès");
        }

        printf("[Slave Server] Résultat: %s (code=%d)\n", result.result, ret);

        // Send result back to master (who will forward to client)
        if (sendto(sock, (const char *)&result, sizeof(result), 0,
                  (struct sockaddr *)&client_addr, client_addr_len) == SOCKET_ERROR) {
            fprintf(stderr, "sendto failed: %d\n", WSAGetLastError());
        }
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}
