@echo off
REM Stop all servers

echo ==========================================
echo Stopping all servers...
echo ==========================================
echo.

REM Kill all server processes
taskkill /F /IM serveur_esclave.exe >nul 2>&1
taskkill /F /IM serveur_maitre.exe >nul 2>&1
taskkill /F /IM client.exe >nul 2>&1

if %errorlevel% equ 0 (
    echo All servers stopped.
) else (
    echo No running servers found or they were already stopped.
)

echo.
echo ==========================================
echo Cleanup complete!
echo ==========================================
echo.
pause
