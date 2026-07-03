@echo off
setlocal
cd /d "%~dp0"

set "TARGET=logistics_v3_client.exe"
set "SRC=client\main.cpp client\SocketClient.cpp common\protocol\ProtocolCodec.cpp common\security\InputValidator.cpp common\security\StringUtil.cpp"
set "BIN_DIR=bin"
set "BIN_TARGET=%BIN_DIR%\%TARGET%"

echo [Build] preparing client target...
if not exist "%BIN_DIR%" mkdir "%BIN_DIR%"
del /f /q "%BIN_TARGET%" 2>nul
del /f /q "%BIN_DIR%\client_*.o" 2>nul
del /f /q "%BIN_DIR%\client_*.obj" 2>nul

echo [Build] compiling %SRC% to %BIN_TARGET%...
g++ -std=c++17 -Wall -Wextra -pedantic -O2 -finput-charset=UTF-8 -fexec-charset=GBK -Icommon %SRC% -o "%BIN_TARGET%" -lws2_32
if errorlevel 1 (
    echo [Build] client build failed.
    exit /b 1
)

echo [Build] client build success: %BIN_TARGET%
endlocal
