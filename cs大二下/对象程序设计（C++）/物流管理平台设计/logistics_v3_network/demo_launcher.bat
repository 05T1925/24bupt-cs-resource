@echo off
setlocal EnableExtensions
cd /d "%~dp0"
chcp 65001 >nul
title Logistics V3 Network Demo Launcher

:MAIN_MENU
cls
call :PrintHeader
echo.
echo   [1] 一键全自动演示模式 ^(Auto-run All Tests^)
echo   [2] 按步交互讲解模式 ^(Step-by-step Demo^)
echo   [3] 人工交互完整业务演示模式 ^(Manual Full Business Demo^)
echo   [4] 退出 ^(Exit^)
echo.
echo ================================================================
set /p "CHOICE=请选择模式 / Select mode [1-4]: "

if "%CHOICE%"=="1" goto AUTO_MODE
if "%CHOICE%"=="2" goto STEP_MODE
if "%CHOICE%"=="3" goto MANUAL_MODE
if "%CHOICE%"=="4" goto EXIT_DEMO

echo.
echo [WARN] 无效输入，请输入 1、2、3 或 4。
timeout /t 2 >nul
goto MAIN_MENU

:AUTO_MODE
cls
call :PrintHeader
echo.
echo [Mode A] 一键全自动演示模式 / Auto-run All Tests
echo ================================================================
call :PrepareEnvironment
if errorlevel 1 goto MAIN_MENU

echo.
echo [Auto 1/4] 普通用户链路：注册、充值、寄件、零信任签收拦截
echo [Auto 1/4] User flow: register, recharge, send express, permission defense
call :RunClient --selftest-user
if errorlevel 1 goto AUTO_FAILED
timeout /t 2 >nul

echo.
echo [Auto 2/4] 管理员链路：后台看板、分配、越权拦截
echo [Auto 2/4] Admin flow: dashboard, assignment, permission defense
call :RunClient --selftest-admin
if errorlevel 1 goto AUTO_FAILED
timeout /t 2 >nul

echo.
echo [Auto 3/4] 快递员链路：批量揽收、资金分账、重复揽收拦截
echo [Auto 3/4] Courier flow: batch pickup, commission transaction, conflict defense
call :RunClient --selftest-courier
if errorlevel 1 goto AUTO_FAILED
timeout /t 2 >nul

echo.
echo [Auto 4/4] 终极并发挑战：双客户端同时抢同一单
echo [Auto 4/4] Concurrency challenge: two clients race for the same express
call :RunClient --selftest-concurrency
if errorlevel 1 goto AUTO_FAILED

echo.
echo ================================================================
echo   全链路自动化测试完成！
echo   All automated V3 network demo tests completed successfully.
echo ================================================================
echo.
pause
goto MAIN_MENU

:AUTO_FAILED
chcp 65001 >nul
echo.
echo ================================================================
echo   [ERROR] 自动演示中断：某个自测返回失败。
echo   [ERROR] Auto-run stopped because one self-test failed.
echo ================================================================
echo.
pause
goto MAIN_MENU

:STEP_MODE
cls
call :PrintHeader
echo.
echo [Mode B] 按步交互讲解模式 / Step-by-step Demo
echo ================================================================
call :PrepareEnvironment
if errorlevel 1 goto MAIN_MENU

echo.
echo [Step 1] 即将展示：普通用户注册、充值、寄件与零信任安全拦截...
echo [Step 1] Next: user register/recharge/send flow and zero-trust permission defense...
echo.
pause
call :RunClient --selftest-user
if errorlevel 1 goto STEP_FAILED

echo.
echo [Step 2] 即将展示：管理员大后台数据看板、分配能力与越权拦截...
echo [Step 2] Next: admin dashboard, assignment, and permission defense...
echo.
pause
call :RunClient --selftest-admin
if errorlevel 1 goto STEP_FAILED

echo.
echo [Step 3] 即将展示：快递员批量揽收与 50%% 资金分账事务...
echo [Step 3] Next: courier batch pickup and 50%% commission transaction...
echo.
pause
call :RunClient --selftest-courier
if errorlevel 1 goto STEP_FAILED

echo.
echo [Step 4] 终极展示：双客户端并发抢单锁防线 ^(Mutex/CoreLock^) 与粘包/半包防御...
echo [Step 4] Final: concurrent pickup conflict defense and stream-frame protocol robustness...
echo.
pause
call :RunClient --selftest-concurrency
if errorlevel 1 goto STEP_FAILED

echo.
echo ================================================================
echo   交互式讲解流程完成！
echo   Step-by-step demo finished.
echo ================================================================
echo.
pause
goto MAIN_MENU

:MANUAL_MODE
cls
call :PrintHeader
echo.
echo [Mode C] 人工交互完整业务演示模式 / Manual Full Business Demo
echo ================================================================
call :PrepareEnvironment
if errorlevel 1 goto MAIN_MENU

set "DEMO_ID=%RANDOM%"
set /a PHONE_TAIL=10000000 + (%RANDOM% %% 89999999)
set "SENDER=mans%DEMO_ID%"
set "RECEIVER=manr%DEMO_ID%"
set "SENDER_PHONE=139%PHONE_TAIL%"
set "RECEIVER_PHONE=138%PHONE_TAIL%"
set "USER_PWD=User1234"
set "ADMIN_USER=admin"
set "ADMIN_PWD=Admin0219"
set "COURIER_USER=demo_courier"
set "COURIER_PWD=Courier1234"

cls
call :PrintHeader
echo.
echo  本模式不会一股脑跑自测，而是打开真实交互式客户端窗口。
echo  You will operate the real interactive client menus manually.
echo.
echo  本轮建议演示数据 / Suggested demo data:
echo  ---------------------------------------------------------------
echo  发件用户 Sender   : %SENDER% / %USER_PWD% / %SENDER_PHONE%
echo  收件用户 Receiver : %RECEIVER% / %USER_PWD% / %RECEIVER_PHONE%
echo  管理员 Admin      : %ADMIN_USER% / %ADMIN_PWD%
echo  快递员 Courier    : %COURIER_USER% / %COURIER_PWD%
echo  寄件类型 Item     : Fragile / 易碎品 / 2 kg / 费用应为 16.00
echo  ---------------------------------------------------------------
echo.
echo  请准备一张纸或记事本：寄件成功后要记下快递单号 EXxxxxxx。
echo  Please write down the express id after SEND_EXPRESS succeeds.
echo.
pause

cls
call :PrintHeader
echo.
echo [Manual Step 1/5] 注册两个普通用户并完成发件
echo ================================================================
echo 在即将打开的 User_Client 窗口中按以下顺序操作：
echo.
echo A. 注册发件用户：
echo    选择 1 注册普通用户
echo    用户名: %SENDER%
echo    姓名  : Manual Sender
echo    手机号: %SENDER_PHONE%
echo    密码  : %USER_PWD%
echo    地址  : Sender Address
echo.
echo B. 注册收件用户：
echo    回到主菜单后再次选择 1 注册普通用户
echo    用户名: %RECEIVER%
echo    姓名  : Manual Receiver
echo    手机号: %RECEIVER_PHONE%
echo    密码  : %USER_PWD%
echo    地址  : Receiver Address
echo.
echo C. 登录发件用户并寄件：
echo    选择 2 登录 -^> 选择 1 普通用户
echo    账号: %SENDER%
echo    密码: %USER_PWD%
echo    用户菜单选择 2 充值，金额输入 100
echo    用户菜单选择 3 寄件
echo    收件用户名: %RECEIVER%
echo    物品描述  : Manual fragile package
echo    类型选择 2 易碎品
echo    重量 kg   : 2
echo.
echo D. 记下输出中的快递单号，例如 EX000123。
echo.
pause
call :OpenInteractiveClient "V3_User_Client"
echo.
set /p "EXPRESS_ID=请在这里输入刚才记下的快递单号 / Enter express id: "
if "%EXPRESS_ID%"=="" (
    echo [WARN] 快递单号不能为空，返回主菜单。
    pause
    goto MAIN_MENU
)

cls
call :PrintHeader
echo.
echo [Manual Step 2/5] 管理员分配快递员
echo ================================================================
echo 在即将打开的 Admin_Client 窗口中操作：
echo.
echo A. 选择 2 登录 -^> 选择 3 管理员
echo    账号: %ADMIN_USER%
echo    密码: %ADMIN_PWD%
echo.
echo B. 管理员菜单选择 5 分配快递员
echo    快递单号      : %EXPRESS_ID%
echo    快递员用户名  : %COURIER_USER%
echo.
echo C. 可选：选择 7 查询全部快递，确认该单 courier=%COURIER_USER%，状态仍为待揽收。
echo.
pause
call :OpenInteractiveClient "V3_Admin_Client"

cls
call :PrintHeader
echo.
echo [Manual Step 3/5] 快递员查看任务并揽收
echo ================================================================
echo 在即将打开的 Courier_Client 窗口中操作：
echo.
echo A. 选择 2 登录 -^> 选择 2 快递员
echo    账号: %COURIER_USER%
echo    密码: %COURIER_PWD%
echo.
echo B. 快递员菜单选择 1 查看待揽收任务，确认能看到 %EXPRESS_ID%。
echo C. 快递员菜单选择 2 揽收单个快递。
echo    快递单号: %EXPRESS_ID%
echo.
echo 预期：输出 OK/SUCCESS，提成为 8.00，状态变为待签收。
echo.
pause
call :OpenInteractiveClient "V3_Courier_Client"

cls
call :PrintHeader
echo.
echo [Manual Step 4/5] 收件用户签收快递
echo ================================================================
echo 在即将打开的 Receiver_Client 窗口中操作：
echo.
echo A. 选择 2 登录 -^> 选择 1 普通用户
echo    账号: %RECEIVER%
echo    密码: %USER_PWD%
echo.
echo B. 用户菜单选择 5 查询待签收，确认能看到 %EXPRESS_ID%。
echo C. 用户菜单选择 6 签收快递。
echo    快递单号: %EXPRESS_ID%
echo.
echo 预期：输出 OK/SUCCESS，物流任务完成。
echo.
pause
call :OpenInteractiveClient "V3_Receiver_Client"

cls
call :PrintHeader
echo.
echo [Manual Step 5/5] 管理员最终验收看板
echo ================================================================
echo 可再次打开管理员客户端：
echo.
echo A. 登录 admin / Admin0219。
echo B. 选择 7 查询全部快递，确认 %EXPRESS_ID% 状态为已签收。
echo C. 选择 8 统计看板。
echo D. 选择 9 快递员绩效，确认 %COURIER_USER% 收入增加。
echo.
pause
call :OpenInteractiveClient "V3_Final_Admin_Check"

echo.
echo ================================================================
echo   人工交互完整业务演示流程已打开完毕。
echo   Manual full business demo windows have been launched.
echo ================================================================
echo.
pause
goto MAIN_MENU

:STEP_FAILED
chcp 65001 >nul
echo.
echo ================================================================
echo   [ERROR] 当前步骤自测失败，请检查上方输出。
echo   [ERROR] Current step failed. Please inspect the output above.
echo ================================================================
echo.
pause
goto MAIN_MENU

:PrepareEnvironment
chcp 65001 >nul
echo.
echo [Check] 正在检查运行环境 / Checking demo environment...
if not exist "bin\logistics_v3_server.exe" (
    echo.
    echo [ERROR] 缺少 bin\logistics_v3_server.exe
    echo [HELP ] 请先运行 build_all.bat 后再启动本脚本。
    echo.
    pause
    exit /b 1
)
if not exist "bin\logistics_v3_client.exe" (
    echo.
    echo [ERROR] 缺少 bin\logistics_v3_client.exe
    echo [HELP ] 请先运行 build_all.bat 后再启动本脚本。
    echo.
    pause
    exit /b 1
)

echo [OK] server/client 可执行文件已存在。
netstat -ano | findstr /R /C:":9000 .*LISTENING" >nul
if errorlevel 1 (
    echo [Start] 正在打开独立服务端窗口：V3_Server
    echo [Note] 该窗口会保持监听，演示结束后可手动关闭它。
    start "V3_Server" cmd /k "title V3_Server & chcp 936 >nul & bin\logistics_v3_server.exe"
    echo [Wait] 等待 2 秒，确保 Winsock 监听端口就绪...
    timeout /t 2 >nul
) else (
    echo [OK] 检测到 127.0.0.1:9000 已在监听，将复用当前服务端。
)
exit /b 0

:OpenInteractiveClient
chcp 65001 >nul
set "CLIENT_TITLE=%~1"
echo [Open] 正在打开交互式客户端窗口：%CLIENT_TITLE%
start "%CLIENT_TITLE%" cmd /k "title %CLIENT_TITLE% & chcp 936 >nul & bin\logistics_v3_client.exe"
echo [Tip ] 请在新窗口中按本脚本说明操作。完成后可关闭该客户端窗口。
echo.
pause
exit /b 0

:RunClient
chcp 936 >nul
bin\logistics_v3_client.exe %*
set "CLIENT_EXIT=%ERRORLEVEL%"
chcp 65001 >nul
if not "%CLIENT_EXIT%"=="0" (
    echo.
    echo [ERROR] Client self-test failed. Exit code: %CLIENT_EXIT%
    exit /b %CLIENT_EXIT%
)
exit /b 0

:PrintHeader
echo ================================================================
echo.
echo        LOGISTICS V3 NETWORK DEMO LAUNCHER
echo.
echo        C/S SOCKET    TOKEN AUTH    CONCURRENCY DEFENSE
echo        AUTO TESTS    STEP DEMO     MANUAL BUSINESS FLOW
echo.
echo ================================================================
exit /b 0

:EXIT_DEMO
cls
call :PrintHeader
echo.
echo 感谢使用 Logistics V3 Network Demo Launcher。
echo Bye.
echo.
endlocal
exit /b 0
