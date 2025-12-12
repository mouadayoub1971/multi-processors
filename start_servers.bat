@echo off
REM Start all servers for testing

setlocal enabledelayedexpansion

echo ==========================================
echo Starting all servers...
echo ==========================================

REM Check if executables exist
if not exist serveur_esclave.exe (
    echo Error: serveur_esclave.exe not found. Run compile.bat first.
    pause
    exit /b 1
)

if not exist serveur_maitre.exe (
    echo Error: serveur_maitre.exe not found. Run compile.bat first.
    pause
    exit /b 1
)

if not exist slaves.conf (
    echo Error: slaves.conf not found.
    pause
    exit /b 1
)

echo.
echo Starting 3 slave servers...
echo.

REM Start slave server 1
echo [Slave 1] Starting on port 10001...
start "Slave 1 (port 10001)" serveur_esclave.exe 10001

REM Start slave server 2
echo [Slave 2] Starting on port 10002...
start "Slave 2 (port 10002)" serveur_esclave.exe 10002

REM Start slave server 3
echo [Slave 3] Starting on port 10003...
start "Slave 3 (port 10003)" serveur_esclave.exe 10003

echo.
REM Wait for slaves to start
timeout /t 2 /nobreak

echo.
echo Starting master server...
start "Master Server (port 9999)" serveur_maitre.exe slaves.conf

timeout /t 2 /nobreak

echo.
echo ==========================================
echo All servers are running!
echo ==========================================
echo.
echo Slave servers running on ports:
echo   - 10001
echo   - 10002
echo   - 10003
echo.
echo Master server running on port: 9999
echo.
echo You can now run: client.exe commands.txt
echo.
pause
