#!/bin/bash

# Stop all servers

echo "=========================================="
echo "Arrêt des serveurs..."
echo "=========================================="

# Get the directory where the script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

# Kill master server
if [ -f .master.pid ]; then
    MASTER_PID=$(cat .master.pid)
    if kill -0 $MASTER_PID 2>/dev/null; then
        echo "Arrêt du serveur maître (PID=$MASTER_PID)"
        kill $MASTER_PID
        sleep 1
        # Force kill if still running
        kill -9 $MASTER_PID 2>/dev/null
    fi
    rm -f .master.pid
fi

# Kill slave servers
for i in 1 2 3; do
    PID_FILE=".slave${i}.pid"
    if [ -f "$PID_FILE" ]; then
        SLAVE_PID=$(cat "$PID_FILE")
        if kill -0 $SLAVE_PID 2>/dev/null; then
            echo "Arrêt du serveur esclave $i (PID=$SLAVE_PID)"
            kill $SLAVE_PID
            sleep 1
            # Force kill if still running
            kill -9 $SLAVE_PID 2>/dev/null
        fi
        rm -f "$PID_FILE"
    fi
done

# Also try to kill any remaining instances
echo "Nettoyage des processus résiduels..."
killall -9 serveur_esclave 2>/dev/null
killall -9 serveur_maitre 2>/dev/null

echo ""
echo "=========================================="
echo "Tous les serveurs ont été arrêtés"
echo "=========================================="
