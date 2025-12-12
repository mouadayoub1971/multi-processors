@echo off
REM Compile all servers and client for Windows

echo ==========================================
echo Compiling servers...
echo ==========================================

REM Compile slave server
echo Compiling serveur_esclave.exe...
gcc -o serveur_esclave.exe serveur_esclave.c -lws2_32
if %errorlevel% neq 0 (
    echo Error compiling serveur_esclave.c
    exit /b 1
)

REM Compile master server
echo Compiling serveur_maitre.exe...
gcc -o serveur_maitre.exe serveur_maitre.c -lws2_32
if %errorlevel% neq 0 (
    echo Error compiling serveur_maitre.c
    exit /b 1
)

REM Compile client
echo Compiling client.exe...
gcc -o client.exe client.c -lws2_32
if %errorlevel% neq 0 (
    echo Error compiling client.c
    exit /b 1
)

echo.
echo ==========================================
echo Compilation successful!
echo ==========================================
echo.
echo Executables created:
echo - serveur_esclave.exe
echo - serveur_maitre.exe
echo - client.exe
echo.
echo To start servers, run: start_servers.bat
echo.
pause
