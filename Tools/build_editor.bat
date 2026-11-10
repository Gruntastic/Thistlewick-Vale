@echo off
rem Builds ThistlewickValeEditor Win64 Development with the installed engine's Build.bat.
rem Engine location: set UE_ROOT, default C:\Program Files\Epic Games\UE_5.8
setlocal
if "%UE_ROOT%"=="" set "UE_ROOT=C:\Program Files\Epic Games\UE_5.8"
set "BUILD=%UE_ROOT%\Engine\Build\BatchFiles\Build.bat"
pushd "%~dp0.."
set "PROJECT=%CD%\ThistlewickVale.uproject"
popd

if not exist "%BUILD%" (
  echo [build_editor] Build.bat not found at "%BUILD%". Set UE_ROOT.
  exit /b 1
)
call "%BUILD%" ThistlewickValeEditor Win64 Development -Project="%PROJECT%" -WaitMutex -FromMsBuild %*
set "RESULT=%ERRORLEVEL%"
if not "%RESULT%"=="0" echo [build_editor] Build FAILED with code %RESULT%
exit /b %RESULT%
