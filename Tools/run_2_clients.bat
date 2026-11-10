@echo off
rem Smoke test launcher: dedicated server + two clients (Ash the Warrior, Bryn the Cleric). See Tests/Net/README.md.
rem Usage: run_2_clients.bat [ServerWaitSeconds=20]
setlocal
set "WAIT=%~1"
if "%WAIT%"=="" set "WAIT=20"
set "TOOLS=%~dp0"

start "TV Server" cmd /c ""%TOOLS%run_server.bat""
echo [run_2_clients] Waiting %WAIT% s for the server to load L_Thistlewick...
timeout /t %WAIT% /nobreak >nul

start "TV Client Ash" cmd /c ""%TOOLS%run_client.bat" Ash CLS_WARRIOR 127.0.0.1:7777 0 40"
timeout /t 3 /nobreak >nul
start "TV Client Bryn" cmd /c ""%TOOLS%run_client.bat" Bryn CLS_CLERIC 127.0.0.1:7777 1290 40"
echo [run_2_clients] Server and two clients started.
endlocal
