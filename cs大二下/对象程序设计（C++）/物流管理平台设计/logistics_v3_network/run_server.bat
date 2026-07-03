@echo off
setlocal
cd /d "%~dp0"
chcp 936 >nul
if not exist "bin\logistics_v3_server.exe" (
    call build_server.bat
    if errorlevel 1 exit /b 1
)
"bin\logistics_v3_server.exe"
endlocal
