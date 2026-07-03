@echo off
setlocal
cd /d "%~dp0"

set "TARGET=logistics_v3_server.exe"
set "SRC=server\main.cpp server\SocketServer.cpp server\SessionManager.cpp server\ServerController.cpp common\models\Entities.cpp common\models\ExpressItem.cpp common\protocol\ProtocolCodec.cpp common\security\StringUtil.cpp common\security\HashUtil.cpp common\security\PasswordHasher.cpp common\security\InputValidator.cpp common\storage\StorageManager.cpp common\storage\Repositories.cpp common\storage\Logger.cpp common\service\LogisticsSystem.cpp"
set "BIN_DIR=bin"
set "BIN_TARGET=%BIN_DIR%\%TARGET%"

echo [Build] preparing server target...
if not exist "%BIN_DIR%" mkdir "%BIN_DIR%"
del /f /q "%BIN_TARGET%" 2>nul
del /f /q "%BIN_DIR%\server_*.o" 2>nul
del /f /q "%BIN_DIR%\server_*.obj" 2>nul

echo [Build] compiling %SRC% to %BIN_TARGET%...
g++ -std=c++17 -Wall -Wextra -pedantic -O2 -finput-charset=UTF-8 -fexec-charset=GBK -Icommon %SRC% -o "%BIN_TARGET%" -lws2_32
if errorlevel 1 (
    echo [Build] server build failed.
    exit /b 1
)

echo [Build] server build success: %BIN_TARGET%
endlocal
