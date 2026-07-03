@echo off
setlocal
cd /d "%~dp0"

call build_server.bat
if errorlevel 1 exit /b 1

call build_client.bat
if errorlevel 1 exit /b 1

echo [Build] all targets built successfully.
endlocal

