// Demo.cpp : 定义应用程序的入口点。
#include "Demo.h"
using namespace std;

#pragma region 全局变量

#define MAX_LOADSTRING 100

// 全局变量：
HINSTANCE hInst;                     // 当前实例
WCHAR szTitle[MAX_LOADSTRING];       // 标题栏文本
WCHAR szWindowClass[MAX_LOADSTRING]; // 主窗口类名

// 位图资源：
HBITMAP bmp_StartButton;     // 开始按钮图像资源
HBITMAP bmp_Rock;            // 石头图像资源
HBITMAP bmp_Brick;           // 砖块图像资源
HBITMAP bmp_WhiteBackground; // 生成的纯白色背景图像
HBITMAP bmp_GameBackground;  // 生成的游戏背景图像

// 游戏状态：
Stage *currentStage = NULL; // 当前场景状态
vector<Button *> buttons;   // 按钮
Brick *brick = NULL;        // 砖块

// 键盘：
bool keyUpDown = false;    // 上
bool keyDownDown = false;  // 下
bool keyLeftDown = false;  // 左
bool keyRightDown = false; // 右

// 鼠标：
int mouseX = 0;         // 鼠标X坐标
int mouseY = 0;         // 鼠标Y坐标
bool mouseDown = false; // 鼠标按下

#pragma endregion

#pragma region Win32程序框架

// 此代码模块中包含的函数的前向声明:
ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                      _In_opt_ HINSTANCE hPrevInstance,
                      _In_ LPWSTR lpCmdLine,
                      _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // 初始化全局字符串
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_DEMO, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // 执行应用程序初始化:
    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_DEMO));

    MSG msg;

    // 主消息循环:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int)msg.wParam;
}

//
//  函数: MyRegisterClass()
//
//  目标: 注册窗口类。
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex{};

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_DEMO));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_DEMO);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   函数: InitInstance(HINSTANCE, int)
//
//   目标: 保存实例句柄并创建主窗口
//
//   注释:
//
//        在此函数中，我们在全局变量中保存实例句柄并
//        创建和显示主程序窗口。
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance; // 将实例句柄存储在全局变量中

    HWND hWnd = CreateWindowW(
        szWindowClass, szTitle,
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX, // 设置窗口样式，不可改变大小，不可最大化
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        WINDOW_WIDTH,
        WINDOW_HEIGHT + WINDOW_TITLEBARHEIGHT,
        nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
    {
        return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return TRUE;
}

//
//  函数: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  目标: 处理主窗口的消息。
//
//  WM_CREATE   - 处理窗口创建事件
//  WM_TIMER    - 处理定时器TimeOut事件
//  WM_PAINT    - 绘制主窗口
//  WM_COMMAND  - 处理应用程序菜单
//  WM_DESTROY  - 发送退出消息并返回
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        // 初始化游戏窗体
        InitGame(hWnd, wParam, lParam);
    }
    break;
    case WM_KEYDOWN:
    {
        // 键盘按下事件
        KeyDown(hWnd, wParam, lParam);
    }
    break;
    case WM_KEYUP:
    {
        // 键盘松开事件
        KeyUp(hWnd, wParam, lParam);
    }
    break;
    case WM_MOUSEMOVE:
    {
        // 鼠标移动事件
        MouseMove(hWnd, wParam, lParam);
    }
    break;
    case WM_LBUTTONDOWN:
    {
        // 鼠标左键按下事件
        LButtonDown(hWnd, wParam, lParam);
    }
    break;
    case WM_LBUTTONUP:
    {
        // 鼠标左键松开事件
        LButtonUp(hWnd, wParam, lParam);
    }
    break;
    case WM_TIMER:
    {
        // 判断游戏是否暂停
        if (currentStage != NULL && !currentStage->pause)
        {
            TimerUpdate(hWnd, wParam, lParam);
        }
    }
    break;
    case WM_PAINT:
    {
        // 绘图
        Paint(hWnd);
    }
    break;

    // TODO：其他消息
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        // 分析菜单选择:
        switch (wmId)
        {
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;
    case WM_DESTROY:
    {
        // 窗口退出
        PostQuitMessage(0);
    }
    break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// “关于”框的消息处理程序。
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

#pragma endregion

#pragma region 事件处理函数

// 初始化游戏窗体函数
void InitGame(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    // 加载图像资源
    bmp_StartButton = LoadBitmap(((LPCREATESTRUCT)lParam)->hInstance, MAKEINTRESOURCE(IDB_BITMAP_START));
    bmp_Rock = LoadBitmap(((LPCREATESTRUCT)lParam)->hInstance, MAKEINTRESOURCE(IDB_BITMAP_ROCK));
    bmp_Brick = LoadBitmap(((LPCREATESTRUCT)lParam)->hInstance, MAKEINTRESOURCE(IDB_BITMAP_BRICK));
    bmp_WhiteBackground = CreateWhiteBackground(hWnd);
    bmp_GameBackground = CreateGameBackground(hWnd);

    // 初始化开始场景
    InitStage(hWnd, STAGE_START);

    // 初始化主计时器
    SetTimer(hWnd, TIMER_GAMETIMER, TIMER_GAMETIMER_ELAPSE, NULL);
}

// 键盘按下事件处理函数
void KeyDown(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    switch (wParam)
    {
    case 0x57:  // W
    case VK_UP: // 方向键上
        keyUpDown = true;
        break;
    case 0x53:    // S
    case VK_DOWN: // 方向键下
        keyDownDown = true;
        break;
    case 0x41:    // A
    case VK_LEFT: // 方向键左
        keyLeftDown = true;
        break;
    case 0x44:     // D
    case VK_RIGHT: // 方向键右
        keyRightDown = true;
        break;

    // TODO：其他按键
    default:
        break;
    }
}

// 键盘松开事件处理函数
void KeyUp(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    switch (wParam)
    {
    case 0x57:  // W
    case VK_UP: // 方向键上
        keyUpDown = false;
        break;
    case 0x53:    // S
    case VK_DOWN: // 方向键下
        keyDownDown = false;
        break;
    case 0x41:    // A
    case VK_LEFT: // 方向键左
        keyLeftDown = false;
        break;
    case 0x44:     // D
    case VK_RIGHT: // 方向键右
        keyRightDown = false;
        break;

    // TODO：其他按键
    default:
        break;
    }
}

// 鼠标移动事件处理函数
void MouseMove(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    mouseX = LOWORD(lParam);
    mouseY = HIWORD(lParam);
}

// 鼠标左键按下事件处理函数
void LButtonDown(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    mouseX = LOWORD(lParam);
    mouseY = HIWORD(lParam);
    mouseDown = true;

    // 判断鼠标是否点击按钮
    for (int i = 0; i < buttons.size(); i++)
    {
        Button *button = buttons[i];
        if (button->x <= mouseX && button->x + button->width >= mouseX && button->y <= mouseY && button->y + button->height >= mouseY)
        {
            switch (button->buttonID)
            {
            case BUTTON_STARTGAME_ID:
                InitStage(hWnd, STAGE_GAME);
                break;

            // TODO: 其他按钮
            default:
                break;
            }
        }
    }
}

// 鼠标左键松开事件处理函数
void LButtonUp(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    mouseX = LOWORD(lParam);
    mouseY = HIWORD(lParam);
    mouseDown = false;
}

// 定时器事件处理函数
void TimerUpdate(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    // 状态更新
    switch (currentStage->stageID)
    {
    case STAGE_START:
        break;
    case STAGE_GAME:
    {
        UpdateBrick(hWnd);
    }
    break;

    // TODO：添加多个游戏场景
    default:
        break;
    }

    // 刷新显示
    InvalidateRect(hWnd, NULL, FALSE);
}

#pragma endregion

#pragma region 其它游戏状态处理函数

// 初始化游戏场景函数
void InitStage(HWND hWnd, int stageID)
{
    // 初始化场景实例
    if (currentStage != NULL)
        delete currentStage;
    currentStage = new Stage();
    currentStage->stageID = stageID;

    switch (stageID)
    {
    case STAGE_START:
    {
        currentStage->bg = bmp_WhiteBackground;
        currentStage->pause = false;

        // 删除旧的按钮
        for (size_t i = 0; i < buttons.size(); i++)
        {
            Button *button = buttons[i];
            delete button;
        }
        buttons.clear();

        // 添加新的开始按钮
        Button *startButton = CreateButton(
            BUTTON_STARTGAME_ID, bmp_StartButton,
            BUTTON_STARTGAME_X, BUTTON_STARTGAME_Y,
            BUTTON_STARTGAME_WIDTH, BUTTON_STARTGAME_HEIGHT);
        buttons.push_back(startButton);
    }
    break;
    case STAGE_GAME:
    {
        currentStage->bg = bmp_GameBackground;
        currentStage->pause = false;

        // 删除旧的按钮
        for (size_t i = 0; i < buttons.size(); i++)
        {
            Button *button = buttons[i];
            delete button;
        }
        buttons.clear();

        // 删除旧的砖块，创建新的砖块
        if (brick != NULL)
            delete brick;
        brick = CreateBrick(bmp_Brick);
    }
    break;

    // TODO：添加多个游戏场景
    default:
        break;
    }

    // 刷新显示
    InvalidateRect(hWnd, NULL, FALSE);
}

// 添加按钮函数
Button *CreateButton(int buttonID, HBITMAP img, int x, int y, int width, int height)
{
    Button *button = new Button();
    button->buttonID = buttonID;
    button->img = img;
    button->x = x;
    button->y = y;
    button->width = width;
    button->height = height;
    return button;
}

// 添加砖块函数
Brick *CreateBrick(HBITMAP img)
{
    Brick *brick = new Brick();
    brick->img = img;
    brick->frame_index = 0;
    brick->cell_x = BRICK_COLUMNS / 2;
    brick->cell_y = BRICK_ROWS / 2;
    return brick;
}

// 刷新砖块状态函数
void UpdateBrick(HWND hWnd)
{
    // 动画更新
    brick->frame_index = (brick->frame_index + 1) % BRICK_FRAME_SIZE;

    // 位置更新
    if (keyUpDown)
    {
        brick->cell_y--;
    }
    if (keyDownDown)
    {
        brick->cell_y++;
    }
    if (keyLeftDown)
    {
        brick->cell_x--;
    }
    if (keyRightDown)
    {
        brick->cell_x++;
    }
    // 防止超出屏幕
    if (brick->cell_x < 0)
    {
        brick->cell_x = 0;
    }
    if (brick->cell_x >= BRICK_COLUMNS)
    {
        brick->cell_x = BRICK_COLUMNS - 1;
    }
    if (brick->cell_y < 0)
    {
        brick->cell_y = 0;
    }
    if (brick->cell_y >= BRICK_ROWS)
    {
        brick->cell_y = BRICK_ROWS - 1;
    }
}

#pragma endregion

#pragma region 绘图函数

// 绘图函数
void Paint(HWND hWnd)
{
    // 开始绘制
    PAINTSTRUCT ps;
    HDC hdc_window = BeginPaint(hWnd, &ps);

    // 创建缓存
    HDC hdc_memBuffer = CreateCompatibleDC(hdc_window);
    HDC hdc_loadBmp = CreateCompatibleDC(hdc_window);

    // 初始化缓存
    HBITMAP blankBmp = CreateCompatibleBitmap(hdc_window, WINDOW_WIDTH, WINDOW_HEIGHT);
    SelectObject(hdc_memBuffer, blankBmp);

    // 绘制背景到缓存
    SelectObject(hdc_loadBmp, currentStage->bg);
    BitBlt(hdc_memBuffer, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, hdc_loadBmp, 0, 0, SRCCOPY);

    // 按场景分类绘制内容到缓存
    switch (currentStage->stageID)
    {
    case STAGE_START:
    {
        // 绘制开始界面提示文字

        // 创建一个字体，设置高度为 30（单位是逻辑单元）
        HFONT hFont = CreateFont(
            30,                       // 字体高度
            0,                        // 字体宽度（0 表示自动计算）
            0,                        // 字体的倾斜角度
            0,                        // 字体的基线倾斜角度
            FW_NORMAL,                // 字体的粗细
            FALSE,                    // 是否斜体
            FALSE,                    // 是否下划线
            FALSE,                    // 是否删除线
            DEFAULT_CHARSET,          // 字符集
            OUT_DEFAULT_PRECIS,       // 输出精度
            CLIP_DEFAULT_PRECIS,      // 剪裁精度
            DEFAULT_QUALITY,          // 输出质量
            DEFAULT_PITCH | FF_SWISS, // 字体家族和间距
            TEXT("微软雅黑")          // 字体名称
        );
        // 选择自定义字体到设备上下文
        HFONT hOldFont = (HFONT)SelectObject(hdc_memBuffer, hFont);
        // 设置字体区域
        RECT rect = {
            BUTTON_STARTGAME_X,
            BUTTON_STARTGAME_Y + 250,
            BUTTON_STARTGAME_X + BUTTON_STARTGAME_WIDTH,
            BUTTON_STARTGAME_Y + 300};
        // 绘制
        DrawText(hdc_memBuffer, TEXT("使用WASD控制方块移动"), -1, &rect, DT_CENTER);
        // 恢复原来的字体
        SelectObject(hdc_memBuffer, hOldFont);
        // 删除自定义字体以释放资源
        DeleteObject(hFont);
    }
    break;
    case STAGE_GAME:
    {
        // 绘制砖块
        SelectObject(hdc_loadBmp, brick->img);
        TransparentBlt(
            hdc_memBuffer, brick->cell_x * BG_CELL_WIDTH, brick->cell_y * BG_CELL_HEIGHT,
            BG_CELL_WIDTH, BG_CELL_HEIGHT,
            hdc_loadBmp, brick->frame_index * BG_CELL_WIDTH, 0, BG_CELL_WIDTH, BG_CELL_WIDTH,
            RGB(255, 255, 255));
    }
    break;

    // TODO：添加多个游戏场景
    default:
        break;
    }

    // 绘制按钮到缓存
    for (int i = 0; i < buttons.size(); i++)
    {
        Button *button = buttons[i];
        SelectObject(hdc_loadBmp, button->img);
        TransparentBlt(
            hdc_memBuffer, button->x, button->y,
            button->width, button->height,
            hdc_loadBmp, 0, 0, button->width, button->height,
            RGB(255, 255, 255));
    }

    // 最后将所有的信息绘制到屏幕上
    BitBlt(hdc_window, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, hdc_memBuffer, 0, 0, SRCCOPY);

    // 回收资源所占的内存（非常重要）
    DeleteObject(blankBmp);
    DeleteDC(hdc_loadBmp);
    DeleteDC(hdc_memBuffer);

    // 结束绘制
    EndPaint(hWnd, &ps);
}

// 创建纯白背景图像
HBITMAP CreateWhiteBackground(HWND hWnd)
{
    // 开始绘制
    PAINTSTRUCT ps;
    HDC hdc_window = BeginPaint(hWnd, &ps);

    // 创建缓存
    HDC hdc_memBuffer = CreateCompatibleDC(hdc_window);

    // 初始化缓存
    HBITMAP bmp_output = CreateCompatibleBitmap(hdc_window, WINDOW_WIDTH, WINDOW_HEIGHT);
    SelectObject(hdc_memBuffer, bmp_output);

    // 创建纯白色画刷
    HBRUSH hBrush = CreateSolidBrush(RGB(255, 255, 255));

    // 创建一个矩形
    RECT rect = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};

    // 将缓存填充为白色
    FillRect(hdc_memBuffer, &rect, hBrush);

    // 回收资源所占的内存（非常重要）
    DeleteObject(hBrush);
    DeleteDC(hdc_memBuffer);

    // 结束绘制
    EndPaint(hWnd, &ps);

    return bmp_output;
}

HBITMAP CreateGameBackground(HWND hWnd)
{
    // 开始绘制
    PAINTSTRUCT ps;
    HDC hdc_window = BeginPaint(hWnd, &ps);

    // 创建缓存
    HDC hdc_memBuffer = CreateCompatibleDC(hdc_window);
    HDC hdc_loadBmp = CreateCompatibleDC(hdc_window);

    // 初始化缓存
    HBITMAP bmp_output = CreateCompatibleBitmap(hdc_window, WINDOW_WIDTH, WINDOW_HEIGHT);
    SelectObject(hdc_memBuffer, bmp_output);

    // 初始化为白色背景
    SelectObject(hdc_loadBmp, bmp_WhiteBackground);
    BitBlt(hdc_memBuffer, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, hdc_loadBmp, 0, 0, SRCCOPY);

    // 画四周石头
    SelectObject(hdc_loadBmp, bmp_Rock);
    for (int i = 0; i < BG_ROWS; i++)
    {
        BitBlt(hdc_memBuffer, 0, i * BG_CELL_HEIGHT, BG_CELL_WIDTH, BG_CELL_HEIGHT, hdc_loadBmp, 0, 0, SRCCOPY);
        BitBlt(hdc_memBuffer, (BG_COLUMNS - 1) * BG_CELL_WIDTH, i * BG_CELL_HEIGHT, BG_CELL_WIDTH, BG_CELL_HEIGHT, hdc_loadBmp, 0, 0, SRCCOPY);
    }
    for (int j = 0; j < BG_COLUMNS; j++)
    {
        BitBlt(hdc_memBuffer, j * BG_CELL_WIDTH, 0, BG_CELL_WIDTH, BG_CELL_HEIGHT, hdc_loadBmp, 0, 0, SRCCOPY);
        BitBlt(hdc_memBuffer, j * BG_CELL_WIDTH, (BG_ROWS - 1) * BG_CELL_HEIGHT, BG_CELL_WIDTH, BG_CELL_HEIGHT, hdc_loadBmp, 0, 0, SRCCOPY);
    }

    // 回收资源所占的内存（非常重要）
    DeleteDC(hdc_loadBmp);
    DeleteDC(hdc_memBuffer);

    // 结束绘制
    EndPaint(hWnd, &ps);

    return bmp_output;
}

#pragma endregion