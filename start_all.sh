#!/bin/bash

# Start all servers: 3 slave servers and 1 master server

echo "=========================================="
echo "Démarrage des serveurs..."
echo "=========================================="

# Get the directory where the script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

# Compile if needed
if [ ! -f serveur_esclave ] || [ serveur_esclave.c -nt serveur_esclave ]; then
    echo "Compilation du serveur esclave..."
    gcc -o serveur_esclave serveur_esclave.c
fi

if [ ! -f serveur_maitre ] || [ serveur_maitre.c -nt serveur_maitre ]; then
    echo "Compilation du serveur maître..."
    gcc -o serveur_maitre serveur_maitre.c
fi

if [ ! -f client ] || [ client.c -nt client ]; then
    echo "Compilation du client..."
    gcc -o client client.c
fi

# Start 3 slave servers
echo ""
echo "Démarrage des serveurs esclaves..."
./serveur_esclave 10001 &
SLAVE1_PID=$!
echo "Esclave 1 lancé (PID=$SLAVE1_PID)"

./serveur_esclave 10002 &
SLAVE2_PID=$!
echo "Esclave 2 lancé (PID=$SLAVE2_PID)"

./serveur_esclave 10003 &
SLAVE3_PID=$!
echo "Esclave 3 lancé (PID=$SLAVE3_PID)"

# Wait a bit for slaves to start
sleep 1

# Start master server
echo ""
echo "Démarrage du serveur maître..."
./serveur_maitre slaves.conf &
MASTER_PID=$!
echo "Maître lancé (PID=$MASTER_PID)"

# Save PIDs for stop script
echo "$MASTER_PID" > .master.pid
echo "$SLAVE1_PID" > .slave1.pid
echo "$SLAVE2_PID" > .slave2.pid
echo "$SLAVE3_PID" > .slave3.pid

echo ""
echo "=========================================="
echo "Tous les serveurs sont en cours d'exécution"
echo "Master PID: $MASTER_PID"
echo "Slave 1 PID: $SLAVE1_PID"
echo "Slave 2 PID: $SLAVE2_PID"
echo "Slave 3 PID: $SLAVE3_PID"
echo "=========================================="
