@echo off
setlocal
cd /d "%~dp0"

set "TARGET=logistics_v2.exe"
set "SRC=src\main.cpp"
set "BIN_DIR=bin"
set "BIN_TARGET=%BIN_DIR%\%TARGET%"

echo [Clean] removing old V2 build artifacts...
if not exist "%BIN_DIR%" mkdir "%BIN_DIR%"
del /f /q "%BIN_TARGET%" 2>nul
del /f /q "%BIN_DIR%\*.obj" 2>nul
del /f /q "%BIN_DIR%\*.o" 2>nul
del /f /q "src\*.obj" 2>nul
del /f /q "src\*.o" 2>nul
del /f /q "src\main.exe" 2>nul
del /f /q "src\logistics_v1.exe" 2>nul
del /f /q "bin\logistics_v1.exe" 2>nul

echo [Build] compiling %SRC% to %BIN_TARGET%...
g++ -std=c++17 -Wall -Wextra -pedantic -O2 -finput-charset=GBK -fexec-charset=GBK "%SRC%" -o "%BIN_TARGET%"
if errorlevel 1 (
    echo Build failed.
    exit /b 1
)

echo Build success: %BIN_TARGET%
endlocal
