#pragma once

#pragma region 头文件引用

// 平台版本头文件
#include "targetver.h"

// Windows 头文件
#define WIN32_LEAN_AND_MEAN // 从 Windows 头文件中排除极少使用的内容
#include <windows.h>

// C 运行时头文件
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <time.h>

// 资源头文件
#include "resource.h"
#pragma comment(lib, "Msimg32.lib") // 解决报错：无法解析的外部符号 __imp_TransparentBlt

// TODO: 在此处引用程序需要的其他头文件
#include <cmath>
#include <vector>

#pragma endregion

#pragma region 宏定义

// 窗体
#define WINDOW_TITLEBARHEIGHT 32 // 标题栏高度
#define WINDOW_WIDTH 1200        // 游戏窗口宽度
#define WINDOW_HEIGHT 800        // 游戏窗口高度

// 计时器
#define TIMER_GAMETIMER 1          // 游戏的默认计时器ID
#define TIMER_GAMETIMER_ELAPSE 50  // 默认计时器刷新间隔的毫秒数

// 游戏阶段
#define STAGE_START 0 // 开始画面的ID
#define STAGE_GAME 1  // 第一个游戏场景的ID

// 背景
#define BG_CELL_WIDTH 100                       // 背景单格宽度
#define BG_CELL_HEIGHT 100                      // 背景单格高度
#define BG_COLUMNS WINDOW_WIDTH / BG_CELL_WIDTH // 背景沿X方向的单元个数
#define BG_ROWS WINDOW_HEIGHT / BG_CELL_HEIGHT  // 背景沿Y方向的单元个数

// 砖块
#define BRICK_CELL_WIDTH 100                          // 砖块单格宽度
#define BRICK_CELL_HEIGHT 100                         // 砖块单格高度
#define BRICK_COLUMNS WINDOW_WIDTH / BRICK_CELL_WIDTH // 砖块沿X方向的单元个数
#define BRICK_ROWS WINDOW_HEIGHT / BRICK_CELL_HEIGHT  // 砖块沿Y方向的单元个数
#define BRICK_FRAME_SIZE 4                            // 砖块动画帧序列的大小

// 开始游戏按钮
#define BUTTON_STARTGAME_ID 1001                                              // 开始游戏按钮ID
#define BUTTON_STARTGAME_WIDTH 300                                            // 开始游戏按钮宽度
#define BUTTON_STARTGAME_HEIGHT 200                                           // 开始游戏按钮高度
#define BUTTON_STARTGAME_X (WINDOW_WIDTH - BUTTON_STARTGAME_WIDTH) / 2        // 开始游戏按钮X坐标
#define BUTTON_STARTGAME_Y (WINDOW_HEIGHT - BUTTON_STARTGAME_HEIGHT) / 2 - 50 // 开始游戏按钮Y坐标

// TODO: 添加游戏需要的更多宏定义

#pragma endregion

#pragma region 结构体声明

// 场景结构体
struct Stage
{
    int stageID; // 场景编号
    HBITMAP bg;  // 背景图片
    bool pause;  // 游戏是否被暂停（计时器是否运行）

    // TODO: 其他需要记录的信息
};

// 按钮结构体
struct Button
{
    int buttonID; // 按钮编号
    HBITMAP img;  // 图片
    int x;        // 坐标x
    int y;        // 坐标y
    int width;    // 宽度
    int height;   // 高度
};

// 砖块结构体
struct Brick
{
    HBITMAP img;     // 图片
    int frame_index; // 当前显示的是帧序列的第几个
    int cell_x;      // 单元坐标x
    int cell_y;      // 单元坐标y
};

// TODO: 添加游戏需要的更多结构体

#pragma endregion

#pragma region 事件处理函数声明

// 初始化游戏窗体函数
void InitGame(HWND hWnd, WPARAM wParam, LPARAM lParam);

// 键盘按下事件处理函数
void KeyDown(HWND hWnd, WPARAM wParam, LPARAM lParam);

// 键盘松开事件处理函数
void KeyUp(HWND hWnd, WPARAM wParam, LPARAM lParam);

// 鼠标移动事件处理函数
void MouseMove(HWND hWnd, WPARAM wParam, LPARAM lParam);

// 鼠标左键按下事件处理函数
void LButtonDown(HWND hWnd, WPARAM wParam, LPARAM lParam);

// 鼠标左键松开事件处理函数
void LButtonUp(HWND hWnd, WPARAM wParam, LPARAM lParam);

// 定时器事件处理函数
void TimerUpdate(HWND hWnd, WPARAM wParam, LPARAM lParam);

// TODO: 添加游戏需要的更多处事件理函数

#pragma endregion

#pragma region 其它游戏状态处理函数声明

// 初始化场景函数
void InitStage(HWND hWnd, int stageID);

// 添加按钮函数
Button *CreateButton(int buttonID, HBITMAP img, int width, int height, int x, int y);

// 添加砖块函数
Brick *CreateBrick(HBITMAP img);

// 刷新砖块状态函数
void UpdateBrick(HWND hWnd);

// TODO: 添加游戏需要的更多函数

#pragma endregion

#pragma region 绘图函数声明

// 绘图函数
void Paint(HWND hWnd);

// 创建纯白背景图像
HBITMAP CreateWhiteBackground(HWND hWnd);

// 创建游戏背景资源
HBITMAP CreateGameBackground(HWND hWnd);

// TODO: 添加游戏需要的更多绘图函数

#pragma endregion
