#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>
#include <errno.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winmm.lib")

#define MAX_CMD_LEN 1024
#define MAX_SLAVES 10
#define MAX_CLIENTS 100
#define MASTER_PORT 9999

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

typedef struct {
    char hostname[256];
    int port;
    SOCKET sock;
    struct sockaddr_in addr;
    int available;
} SlaveServer;

SlaveServer slaves[MAX_SLAVES];
int num_slaves = 0;

void signal_handler(int sig) {
    printf("\n[Master Server] Arrêt du serveur maître...\n");
    
    // Close all slave sockets
    for (int i = 0; i < num_slaves; i++) {
        if (slaves[i].sock != INVALID_SOCKET) {
            closesocket(slaves[i].sock);
        }
    }
    WSACleanup();
    exit(0);
}

int load_slaves_config(const char *config_file) {
    FILE *fp = fopen(config_file, "r");
    if (!fp) {
        fprintf(stderr, "Cannot open config file: %s\n", config_file);
        return -1;
    }

    num_slaves = 0;
    char line[512];
    while (fgets(line, sizeof(line), fp) && num_slaves < MAX_SLAVES) {
        // Remove trailing newline
        line[strcspn(line, "\n")] = 0;
        
        // Skip empty lines and comments
        if (line[0] == '\0' || line[0] == '#') continue;

        // Parse: hostname port
        char hostname[256];
        int port;
        if (sscanf(line, "%255s %d", hostname, &port) != 2) {
            fprintf(stderr, "Invalid config line: %s\n", line);
            continue;
        }

        strcpy(slaves[num_slaves].hostname, hostname);
        slaves[num_slaves].port = port;
        slaves[num_slaves].available = 1;

        // Create UDP socket for this slave
        slaves[num_slaves].sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (slaves[num_slaves].sock == INVALID_SOCKET) {
            fprintf(stderr, "socket failed: %d\n", WSAGetLastError());
            continue;
        }

        // Resolve hostname and setup address
        struct hostent *he = gethostbyname(hostname);
        if (!he) {
            fprintf(stderr, "Cannot resolve hostname: %s\n", hostname);
            closesocket(slaves[num_slaves].sock);
            continue;
        }

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

int find_available_slave() {
    for (int i = 0; i < num_slaves; i++) {
        if (slaves[i].available) {
            return i;
        }
    }
    return -1;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <slaves_config_file>\n", argv[0]);
        exit(1);
    }

    // Initialize Winsock
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        fprintf(stderr, "WSAStartup failed: %d\n", WSAGetLastError());
        exit(1);
    }

    // Load slave servers configuration
    if (load_slaves_config(argv[1]) <= 0) {
        fprintf(stderr, "Error: No slave servers loaded\n");
        WSACleanup();
        exit(1);
    }

    // Create master server socket (TCP for client communication)
    SOCKET master_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (master_sock == INVALID_SOCKET) {
        fprintf(stderr, "socket failed: %d\n", WSAGetLastError());
        WSACleanup();
        exit(1);
    }

    struct sockaddr_in master_addr;
    memset(&master_addr, 0, sizeof(master_addr));
    master_addr.sin_family = AF_INET;
    master_addr.sin_port = htons(MASTER_PORT);
    master_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Allow socket reuse
    int opt = 1;
    setsockopt(master_sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    if (bind(master_sock, (struct sockaddr *)&master_addr, sizeof(master_addr)) == SOCKET_ERROR) {
        fprintf(stderr, "bind failed: %d\n", WSAGetLastError());
        closesocket(master_sock);
        WSACleanup();
        exit(1);
    }

    if (listen(master_sock, 5) == SOCKET_ERROR) {
        fprintf(stderr, "listen failed: %d\n", WSAGetLastError());
        closesocket(master_sock);
        WSACleanup();
        exit(1);
    }

    printf("[Master Server] Maître lancé sur le port %d avec %d esclaves (PID=%d)\n", 
           MASTER_PORT, num_slaves, _getpid());

    // Main server loop
    while (1) {
        struct sockaddr_in client_addr;
        int client_addr_len = sizeof(client_addr);

        // Accept client connection
        SOCKET client_sock = accept(master_sock, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_sock == INVALID_SOCKET) {
            fprintf(stderr, "accept failed: %d\n", WSAGetLastError());
            continue;
        }

        char *client_ip = inet_ntoa(client_addr.sin_addr);
        int client_port = ntohs(client_addr.sin_port);

        printf("[Master Server] Nouvelle connexion client: %s:%d\n", client_ip, client_port);

        // Read filename from client
        char filename[256];
        int n = recv(client_sock, filename, sizeof(filename) - 1, 0);
        if (n <= 0) {
            fprintf(stderr, "Error reading filename from client\n");
            closesocket(client_sock);
            continue;
        }
        filename[n] = '\0';
        printf("[Master Server] Fichier demandé: %s\n", filename);

        // Open and read command file
        FILE *fp = fopen(filename, "r");
        if (!fp) {
            char error_msg[] = "ERROR: Cannot open file";
            send(client_sock, error_msg, (int)strlen(error_msg), 0);
            closesocket(client_sock);
            continue;
        }

        char line[MAX_CMD_LEN];
        int cmd_count = 0;

        // Send ACK to client
        char ack[] = "OK";
        send(client_sock, ack, (int)strlen(ack), 0);

        // Process each command
        while (fgets(line, sizeof(line), fp)) {
            // Remove trailing newline
            line[strcspn(line, "\n")] = 0;
            
            // Skip empty lines
            if (line[0] == '\0') continue;

            printf("[Master Server] Traitement commande: %s\n", line);

            // Find an available slave
            int slave_idx = find_available_slave();
            if (slave_idx < 0) {
                printf("[Master Server] Aucun esclave disponible, attente...\n");
                Sleep(1000);  // Windows Sleep in milliseconds
                slave_idx = find_available_slave();
                if (slave_idx < 0) {
                    fprintf(stderr, "No available slaves\n");
                    continue;
                }
            }

            // Prepare command request
            CommandRequest req;
            strcpy(req.command, line);
            strcpy(req.client_addr, client_ip);
            req.client_port = client_port;

            // Send command to slave
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

        fclose(fp);

        printf("[Master Server] %d commandes traitées pour le client %s:%d\n", 
               cmd_count, client_ip, client_port);

        closesocket(client_sock);
    }

    closesocket(master_sock);
    WSACleanup();
    return 0;
}
