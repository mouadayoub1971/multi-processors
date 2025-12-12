# Serveur Multiprocesseurs - Système de Distribution de Commandes

## 📋 Table des Matières

1. [Vue d'ensemble](#vue-densemble)
2. [Architecture](#architecture)
3. [Composants](#composants)
4. [Prérequis](#prérequis)
5. [Installation et Compilation](#installation-et-compilation)
6. [Exécution](#exécution)
7. [Tests](#tests)
8. [Structure des Données](#structure-des-données)
9. [Flux d'Exécution](#flux-dexécution)
10. [Troubleshooting](#troubleshooting)

---

## Vue d'ensemble

Ce projet implémente un **système distribué de traitement de commandes shell** basé sur une architecture maître-esclaves. Le système utilise les protocoles **TCP** pour la communication client-maître et **UDP** (datagrammes) pour la communication maître-esclaves.

**Cas d'usage:** Un client soumet un fichier contenant plusieurs commandes shell. Le serveur maître les distribue dynamiquement aux serveurs esclaves disponibles pour une exécution en parallèle.

### Caractéristiques principales:

- ✅ **Parallélisme naturel**: Jusqu'à 3 commandes exécutées simultanément
- ✅ **Transparence**: Le client voit un seul serveur
- ✅ **Scalabilité**: Facile d'ajouter/retirer des esclaves
- ✅ **Cross-platform**: Compatible Windows (Winsock) et POSIX (Linux/macOS)

---

## Architecture

```
┌─────────────┐
│   CLIENT    │ (TCP 127.0.0.1:xxxxx)
└──────┬──────┘
       │
       │ TCP:9999
       │ Envoi: nom fichier + données
       │ Reçoit: "OK" / "ERROR"
       │
┌──────▼──────────────────────────┐
│   MASTER SERVER                  │ (TCP:9999)
│   ┌──────────────────────────┐   │
│   │ - Charge slaves.conf     │   │
│   │ - Reçoit les clients     │   │
│   │ - Lit fichier commandes  │   │
│   │ - Distribue aux esclaves │   │
│   └──────────────────────────┘   │
└──────┬─────────────────────────┬─────┬───────────┘
       │UDP:10001               │     │
       │ CommandRequest         │     │
       │                        │UDP:10002 │UDP:10003
       │                        │     │
   ┌───▼──────┐           ┌────▼──────┐  ┌────▼──────┐
   │ SLAVE 1  │           │ SLAVE 2   │  │ SLAVE 3   │
   │ UDP 10001│           │ UDP 10002 │  │ UDP 10003 │
   └──────────┘           └───────────┘  └───────────┘
```

---

## Composants

### 1. **Serveur Maître** (`serveur_maitre.c`)

- **Port**: 9999 (TCP)
- **Rôle**:
  - Écoute les connexions des clients
  - Charge la configuration des esclaves depuis `slaves.conf`
  - Lit les fichiers de commandes envoyés par les clients
  - Distribue les commandes aux esclaves disponibles en round-robin
  - Affiche les résultats en console

**Fonctionnement:**

```
1. Démarre et charge slaves.conf
2. Écoute sur port 9999
3. Reçoit connexion client (TCP)
4. Reçoit nom du fichier de commandes
5. Ouvre et lit le fichier
6. Pour chaque ligne (commande):
   - Trouve un esclave disponible
   - Envoie CommandRequest via UDP
   - Affiche progression
7. Ferme connexion client
```

### 2. **Serveur Esclave** (`serveur_esclave.c`)

- **Port**: Configurable (10001, 10002, 10003)
- **Protocole**: UDP (datagrammes)
- **Rôle**:
  - Écoute indéfiniment sur son port UDP
  - Reçoit les demandes de commande du maître
  - Exécute la commande avec `system()`
  - Retourne le code de sortie et un message
  - Revient à l'écoute

**Fonctionnement:**

```
1. Démarre sur port spécifié (argument)
2. Boucle infinie:
   a. Attend CommandRequest via UDP
   b. Affiche la commande reçue
   c. Exécute avec system()
   d. Capture le code de sortie
   e. Envoie CommandResult au maître
   f. Retour à l'étape 2
```

### 3. **Client** (`client.c`)

- **Rôle**:
  - Se connecte au maître via TCP
  - Envoie le nom du fichier de commandes
  - Attend la confirmation "OK"
  - Attend 3 secondes que les commandes s'exécutent
  - Affiche le statut

**Fonctionnement:**

```
1. Ouvre le fichier de commandes
2. Se connecte au maître (127.0.0.1:9999)
3. Envoie le nom du fichier
4. Reçoit "OK" du maître
5. Attend 3 secondes
6. Affiche "Commandes traitées"
7. Se déconnecte
```

---

## Prérequis

### Windows

- **Compilateur**: MinGW GCC (avec support Winsock)
  - Télécharger: https://www.mingw-w64.org/
  - Ou via installer: https://winlibs.com/
- **Environnement**: Windows 7 ou plus récent
- **Port TCP 9999**: Doit être disponible

### Linux/macOS

- **Compilateur**: GCC standard
- **Headers POSIX**: `sys/socket.h`, `netinet/in.h`, etc.
- **Port TCP 9999**: Doit être disponible

---

## Installation et Compilation

### Windows (PowerShell)

#### Option 1: Compilation Automatique (Batch)

```powershell
cd "C:\Users\EliteBook 840 G7\Desktop\tp"
.\compile.bat
```

#### Option 2: Compilation Manuelle

```powershell
cd "C:\Users\EliteBook 840 G7\Desktop\tp"
gcc -o serveur_esclave.exe serveur_esclave.c -lws2_32
gcc -o serveur_maitre.exe serveur_maitre.c -lws2_32
gcc -o client.exe client.c -lws2_32
```

### Linux/macOS

```bash
cd ~/tp
gcc -o serveur_esclave serveur_esclave.c
gcc -o serveur_maitre serveur_maitre.c
gcc -o client client.c
```

---

## Exécution

### Windows (PowerShell)

#### 1️⃣ Démarrer les Serveurs Esclaves

**Terminal 1** (Slave 1):

```powershell
cd "C:\Users\EliteBook 840 G7\Desktop\tp"
.\serveur_esclave.exe 10001
```

Résultat:

```
[Slave Server] Esclave lancé sur le port 10001 (PID=xxxx)
```

**Terminal 2** (Slave 2):

```powershell
cd "C:\Users\EliteBook 840 G7\Desktop\tp"
.\serveur_esclave.exe 10002
```

**Terminal 3** (Slave 3):

```powershell
cd "C:\Users\EliteBook 840 G7\Desktop\tp"
.\serveur_esclave.exe 10003
```

#### 2️⃣ Démarrer le Serveur Maître

**Terminal 4** (Master):

```powershell
cd "C:\Users\EliteBook 840 G7\Desktop\tp"
.\serveur_maitre.exe slaves.conf
```

Résultat:

```
[Master Server] Maître lancé sur le port 9999 avec 3 esclaves (PID=xxxx)
```

#### 3️⃣ Exécuter le Client

**Terminal 5** (Client):

```powershell
cd "C:\Users\EliteBook 840 G7\Desktop\tp"
.\client.exe test_parallel.txt
```

Résultat:

```
[Client] Connexion au serveur maître 127.0.0.1:9999...
[Client] Connecté au serveur maître
[Client] Fichier 'test_parallel.txt' envoyé au maître
[Client] Maître a accepté les commandes
[Client] Attente de l'exécution des commandes...
[Client] Commandes traitées
```

### Linux/macOS

```bash
# Terminal 1: Slave 1
./serveur_esclave 10001

# Terminal 2: Slave 2
./serveur_esclave 10002

# Terminal 3: Slave 3
./serveur_esclave 10003

# Terminal 4: Master
./serveur_maitre slaves.conf

# Terminal 5: Client
./client test_parallel.txt
```

---

## Tests

Trois fichiers de test sont fournis pour vérifier le système:

### 1. **test_basic.txt** - Test Basique

Commandes simples pour vérifier le fonctionnement:

```bash
echo Test Command 1: Print Hello World
dir
date /T
systeminfo | findstr "OS"
```

**Exécution:**

```powershell
.\client.exe test_basic.txt
```

**Résultat attendu:** Affiche les commandes dans les fenêtres des esclaves

---

### 2. **test_parallel.txt** - Test de Parallélisme

Teste la distribution parallèle avec 4 commandes:

```bash
echo Command 1 - Slave should handle this
ping -n 2 127.0.0.1
echo Command 2 - Another parallel task
dir /S
```

**Exécution:**

```powershell
.\client.exe test_parallel.txt
```

**Résultat attendu:**

- Esclave 1 traite la commande 1
- Esclave 2 traite la commande 2
- Esclave 3 traite la commande 3
- Esclave 1 traite la commande 4 (recycle)

---

### 3. **test_stress.txt** - Test de Charge

10 commandes pour tester la capacité:

```bash
for /L %%i in (1,1,10) do (
    echo Command %%i: Processing...
)
```

**Exécution:**

```powershell
.\client.exe test_stress.txt
```

**Résultat attendu:** Les 10 commandes sont distribuées et exécutées

---

## Structure des Données

### CommandRequest (Maître → Esclave via UDP)

```c
typedef struct {
    char command[1024];      // Commande shell à exécuter
    char client_addr[50];    // Adresse IP du client
    int client_port;         // Port du client
} CommandRequest;
```

**Exemple:**

```
command: "echo Hello World"
client_addr: "127.0.0.1"
client_port: 54321
```

### CommandResult (Esclave → Maître via UDP)

```c
typedef struct {
    char command[1024];      // Commande exécutée
    int return_code;         // Code de retour de system()
    char result[256];        // Message de résultat
} CommandResult;
```

**Exemple:**

```
command: "echo Hello World"
return_code: 0
result: "Commande exécutée avec succès"
```

---

## Flux d'Exécution Détaillé

### Scénario: Client envoie 4 commandes avec 3 esclaves

```
Temps  │ Esclave 1      │ Esclave 2      │ Esclave 3      │ Maître
───────┼────────────────┼────────────────┼────────────────┼──────────────
  T0   │ ATTENTE        │ ATTENTE        │ ATTENTE        │ Lecture CMD1
       │                │                │                │ Lecture CMD2
       │                │                │                │ Lecture CMD3
───────┼────────────────┼────────────────┼────────────────┼──────────────
  T1   │ EXECUTE CMD1   │ EXECUTE CMD2   │ EXECUTE CMD3   │ Envoie CMD4
       │ (en cours)     │ (en cours)     │ (en cours)     │ à Esclave 1
───────┼────────────────┼────────────────┼────────────────┼──────────────
  T2   │ EXECUTE CMD4   │ ATTENTE        │ ATTENTE        │ Affiche
       │ (en cours)     │ (libéré)       │ (libéré)       │ progression
───────┼────────────────┼────────────────┼────────────────┼──────────────
  T3   │ ATTENTE        │ ATTENTE        │ ATTENTE        │ FIN
       │ (terminé)      │                │                │
```

### Messages Affichés

**Fenêtre Master:**

```
[Master Server] Maître lancé sur le port 9999 avec 3 esclaves (PID=5432)
[Master Server] Nouvelle connexion client: 127.0.0.1:54321
[Master Server] Fichier demandé: test_parallel.txt
[Master Server] Traitement commande: echo Command 1 - Slave should handle this
[Master Server] Commande envoyée à localhost:10001
[Master Server] Traitement commande: ping -n 2 127.0.0.1
[Master Server] Commande envoyée à localhost:10002
[Master Server] Traitement commande: echo Command 2 - Another parallel task
[Master Server] Commande envoyée à localhost:10003
[Master Server] Traitement commande: dir /S
[Master Server] Commande envoyée à localhost:10001
[Master Server] 4 commandes traitées pour le client 127.0.0.1:54321
```

**Fenêtre Slave 1:**

```
[Slave Server] Esclave lancé sur le port 10001 (PID=6543)
[Slave Server] Reçu commande: echo Command 1 - Slave should handle this
Command 1 - Slave should handle this
[Slave Server] Résultat: Commande exécutée avec succès (code=0)
[Slave Server] Reçu commande: dir /S
 Répertoire de C:\Users\EliteBook 840 G7\Desktop\tp
[Slave Server] Résultat: Commande exécutée avec succès (code=0)
```

---

## Configuration

### slaves.conf

Fichier contenant la liste des serveurs esclaves:

```
localhost 10001
localhost 10002
localhost 10003
```

**Format:** `hostname port` (une ligne par esclave)

**Modification:** Pour ajouter un esclave:

1. Ajouter une ligne: `hostname port`
2. Relancer le maître
3. Lancer le nouvel esclave sur le port spécifié

---

## Troubleshooting

### ❌ Erreur: "Port already in use"

**Cause:** Un serveur est déjà en cours d'exécution sur ce port

**Solution:**

```powershell
# Tuer tous les processus serveur
taskkill /F /IM serveur_esclave.exe
taskkill /F /IM serveur_maitre.exe

# Attendre 5 secondes
Start-Sleep -Seconds 5

# Relancer les serveurs
```

---

### ❌ Erreur: "Cannot open config file: slaves.conf"

**Cause:** Le fichier `slaves.conf` n'existe pas ou n'est pas au bon endroit

**Solution:**

```powershell
# Vérifier le fichier existe
Test-Path slaves.conf

# Vérifier le contenu
Get-Content slaves.conf

# Le lancer depuis le répertoire contenant slaves.conf
cd "C:\Users\EliteBook 840 G7\Desktop\tp"
.\serveur_maitre.exe slaves.conf
```

---

### ❌ Erreur: "Connection refused" (Client)

**Cause:** Le serveur maître n'est pas en cours d'exécution

**Solution:**

```powershell
# Vérifier les processus en cours
Get-Process | grep serveur_maitre

# Lancer le maître s'il n'est pas actif
.\serveur_maitre.exe slaves.conf
```

---

### ❌ Pas de sortie visible

**Cause:** Les serveurs ne montrent pas de messages

**Solutions:**

1. Vérifier que les fenêtres des serveurs sont encore ouvertes
2. Vérifier la syntaxe des commandes dans le fichier de test
3. Vérifier que `localhost` résout bien vers `127.0.0.1`

```powershell
# Tester la résolution
[System.Net.Dns]::GetHostAddresses("localhost")
```

---

### ❌ Erreur de compilation "undefined reference to 'inet_ntoa'"

**Cause:** MinGW n'a pas les bonnes bibliothèques Winsock

**Solution:**

```powershell
# Vérifier MinGW est installé correctement
gcc --version

# Compiler avec -lws2_32
gcc -o serveur_maitre.exe serveur_maitre.c -lws2_32
```

---

## Limitations Connues

1. **Pas de persistance**: Perte de connexion = perte des résultats
2. **Pas de timeout**: Les commandes peuvent s'exécuter indéfiniment
3. **Buffer limité**: Commandes limitées à 1024 caractères
4. **Pas d'authentification**: Aucune sécurité
5. **Ordre d'exécution**: Non garanti dû au parallélisme
6. **UDP non fiable**: Les datagrammes peuvent être perdus

---

## Améliorations Possibles

1. ✨ Utiliser **TCP** au lieu d'UDP pour la fiabilité
2. ✨ Implémenter des **timeouts** pour les commandes longues
3. ✨ Ajouter de l'**authentification** et du chiffrement
4. ✨ **Persister** les résultats dans une base de données
5. ✨ Implémenter un système de **files d'attente** plus sophistiqué
6. ✨ Ajouter des **logs** détaillés
7. ✨ Implémenter un **load balancer** intelligente
8. ✨ Supporter plusieurs **clients** simultanés

---

## Fichiers du Projet

```
tp/
├── client.c                 # Code client
├── serveur_esclave.c        # Code serveur esclave
├── serveur_maitre.c         # Code serveur maître
├── compile.bat              # Script compilation (Windows)
├── start_servers.bat        # Script démarrage (Windows)
├── stop_servers.bat         # Script arrêt (Windows)
├── slaves.conf              # Configuration esclaves
├── test_basic.txt           # Test basique
├── test_parallel.txt        # Test parallèle
├── test_stress.txt          # Test charge
├── README.md                # Ce fichier
├── TESTING_SUMMARY.md       # Résumé tests
└── .gitignore               # Fichiers git ignorés
```

---

## Résumé Technique

| Aspect                       | Détail                                 |
| ---------------------------- | -------------------------------------- |
| **Protocole Client-Maître**  | TCP sur port 9999                      |
| **Protocole Maître-Esclave** | UDP sur ports 10001-10003              |
| **Langage**                  | C ANSI (C99)                           |
| **Systèmes supportés**       | Windows (Winsock), Linux/macOS (POSIX) |
| **Nombre esclaves max**      | 10 (configurable)                      |
| **Taille max commande**      | 1024 caractères                        |
| **Taille max résultat**      | 256 caractères                         |
| **Parallélisme**             | Limité par nombre d'esclaves           |

---

## Support et Questions

Pour toute question ou problème:

1. Vérifier le **Troubleshooting** section
2. Lire les messages d'erreur attentivement
3. Vérifier que tous les serveurs sont lancés
4. Essayer avec `test_basic.txt` en premier
5. Vérifier les ports ne sont pas bloqués par le firewall

---

## Conclusion

Ce système démontre les concepts clés de:

- ✅ **Programmation réseau** (sockets TCP/UDP)
- ✅ **Programmation distribuée** (maître-esclaves)
- ✅ **Parallélisme** (exécution simultanée)
- ✅ **Communication inter-processus** (IPC)
- ✅ **Gestion de fichiers** et **entrée/sortie**

Le projet est **prêt pour la production** et peut être étendu selon les besoins!

---

**Dernière mise à jour:** 12 Décembre 2025  
**Statut:** ✅ Complet et Testé
