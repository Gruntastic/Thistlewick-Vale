@echo off
rem Dedicated server for Thistlewick Vale using the EDITOR binary (the 5.8.3 launcher build cannot build a Server target).
rem Usage: run_server.bat [extra args]
rem Engine location: set UE_ROOT, default C:\Program Files\Epic Games\UE_5.8
setlocal
if "%UE_ROOT%"=="" set "UE_ROOT=C:\Program Files\Epic Games\UE_5.8"
set "EDITOR=%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor.exe"
pushd "%~dp0.."
set "PROJECT=%CD%\ThistlewickVale.uproject"
popd

if not exist "%EDITOR%" (
  echo [run_server] UnrealEditor.exe not found at "%EDITOR%". Set UE_ROOT.
  exit /b 1
)
if not exist "%PROJECT%" (
  echo [run_server] Project not found at "%PROJECT%".
  exit /b 1
)

echo [run_server] Starting dedicated server on port 7777 (log: Saved\Logs\TV_Server.log)
"%EDITOR%" "%PROJECT%" L_Thistlewick -server -log -port=7777 -LOG=TV_Server.log %*
endlocal
