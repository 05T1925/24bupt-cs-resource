@echo off
if not exist bin mkdir bin
g++ -std=c++17 -Wall -Wextra -pedantic -O2 -finput-charset=UTF-8 -fexec-charset=GBK src\main.cpp -o bin\logistics_v1.exe
if errorlevel 1 (
    echo Build failed.
    exit /b 1
)
g++ -std=c++17 -Wall -Wextra -pedantic -O2 -finput-charset=UTF-8 -fexec-charset=GBK src\main.cpp -o src\main.exe
if errorlevel 1 (
    echo Build failed.
    exit /b 1
)
echo Build success: bin\logistics_v1.exe and src\main.exe
