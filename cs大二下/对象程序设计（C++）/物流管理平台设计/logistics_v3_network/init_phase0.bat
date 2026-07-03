@echo off
setlocal
cd /d "%~dp0"

echo [Init] preparing Logistics V3 network project tree...

if not exist "bin" mkdir "bin"
if not exist "client" mkdir "client"
if not exist "common" mkdir "common"
if not exist "common\models" mkdir "common\models"
if not exist "common\protocol" mkdir "common\protocol"
if not exist "common\security" mkdir "common\security"
if not exist "common\service" mkdir "common\service"
if not exist "common\storage" mkdir "common\storage"
if not exist "data" mkdir "data"
if not exist "docs" mkdir "docs"
if not exist "LOG" mkdir "LOG"
if not exist "server" mkdir "server"
if not exist "tests" mkdir "tests"

if not exist "server\main.cpp" type nul > "server\main.cpp"
if not exist "client\main.cpp" type nul > "client\main.cpp"
if not exist "README.md" type nul > "README.md"
if not exist "docs\.gitkeep" type nul > "docs\.gitkeep"
if not exist "tests\.gitkeep" type nul > "tests\.gitkeep"
if not exist "LOG\.gitkeep" type nul > "LOG\.gitkeep"
if not exist "data\.gitkeep" type nul > "data\.gitkeep"
if not exist "common\models\.gitkeep" type nul > "common\models\.gitkeep"
if not exist "common\protocol\.gitkeep" type nul > "common\protocol\.gitkeep"
if not exist "common\security\.gitkeep" type nul > "common\security\.gitkeep"
if not exist "common\service\.gitkeep" type nul > "common\service\.gitkeep"
if not exist "common\storage\.gitkeep" type nul > "common\storage\.gitkeep"

echo [Init] done.
endlocal

