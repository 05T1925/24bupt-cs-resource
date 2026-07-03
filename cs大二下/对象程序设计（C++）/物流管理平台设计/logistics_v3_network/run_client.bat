@echo off
setlocal
cd /d "%~dp0"
chcp 936 >nul
if not exist "bin\logistics_v3_client.exe" (
    call build_client.bat
    if errorlevel 1 exit /b 1
)
"bin\logistics_v3_client.exe"
endlocal
