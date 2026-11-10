@echo off
rem Game client that connects straight to a server with name and class, so tests need no login UI.
rem Usage: run_client.bat [Name=Ash] [Class=CLS_WARRIOR] [Server=127.0.0.1:7777] [WinX=0] [WinY=40]
rem Example: run_client.bat Bryn CLS_CLERIC 127.0.0.1:7777 1290 40
rem The name and class travel on the URL (?Character=?Class=?Account=; the engine replaces ?Name= with the online
rem subsystem nickname) and also as -TVName= -TVClass=.
setlocal
if "%UE_ROOT%"=="" set "UE_ROOT=C:\Program Files\Epic Games\UE_5.8"
set "EDITOR=%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor.exe"
pushd "%~dp0.."
set "PROJECT=%CD%\ThistlewickVale.uproject"
popd

set "TVNAME=%~1"
if "%TVNAME%"=="" set "TVNAME=Ash"
set "TVCLASS=%~2"
if "%TVCLASS%"=="" set "TVCLASS=CLS_WARRIOR"
set "TVSERVER=%~3"
if "%TVSERVER%"=="" set "TVSERVER=127.0.0.1:7777"
set "WINX=%~4"
if "%WINX%"=="" set "WINX=0"
set "WINY=%~5"
if "%WINY%"=="" set "WINY=40"

if not exist "%EDITOR%" (
  echo [run_client] UnrealEditor.exe not found at "%EDITOR%". Set UE_ROOT.
  exit /b 1
)

echo [run_client] %TVNAME% (%TVCLASS%) connecting to %TVSERVER% (log: Saved\Logs\TV_Client_%TVNAME%.log)
"%EDITOR%" "%PROJECT%" "%TVSERVER%?Name=%TVNAME%?Character=%TVNAME%?Class=%TVCLASS%?Account=%TVNAME%" -game -windowed -ResX=1280 -ResY=720 -WinX=%WINX% -WinY=%WINY% -log -LOG=TV_Client_%TVNAME%.log -TVName=%TVNAME% -TVClass=%TVCLASS%
endlocal
