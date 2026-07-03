#include <iostream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>

using namespace std;

/*
    题目 1：C++ 基础知识实验
    功能：使用 new 动态申请一个 3 x 6 的二维整型数组，并完成输入、输出、求和、查找最大值等操作。

    说明：
    这里采用“指针数组”的方式申请二维数组：
        int** arr = new int*[rows];
        每一行再申请 int[cols]

    释放时必须和申请顺序相反：
        先 delete[] arr[i]
        再 delete[] arr

    补充说明：
    int** 可以看成“指向指针的指针”。本程序中第一层保存每一行的地址，
    第二层 arr[i][j] 才访问到具体整数。由于每一行都单独 new 了一块数组，
    释放时也要逐行 delete[]，最后再释放保存行地址的指针数组。

    delete 和 delete[] 要和 new 的形式对应：
        new 单个对象  -> delete
        new 数组      -> delete[]
*/

const int ROWS = 3;
const int COLS = 6;

/*
    函数功能：读取一个整数，并检查输入是否合法。
    参数 prompt：每次输入前显示的提示文字。
    返回值：用户输入的合法整数。

    本实验只需要普通整型测试数据，因此额外限制输入范围，避免超长数字
    或极端数值导致后续求和、比较时失去实际意义。
*/
int readInt(const string& prompt) {
    const long long MIN_VALUE = -1000000;
    const long long MAX_VALUE = 1000000;
    string line;
    while (true) {
        cout << prompt;
        getline(cin >> ws, line);

        stringstream ss(line);
        long long value;
        char extra;
        if (ss >> value && !(ss >> extra) && value >= MIN_VALUE && value <= MAX_VALUE) {
            return static_cast<int>(value);
        }

        cout << "输入不合法，请输入 " << MIN_VALUE << " 到 " << MAX_VALUE << " 之间的整数。" << endl;
    }
}

/*
    函数功能：动态创建 rows x cols 的二维数组。
    参数 rows、cols：数组行数和列数。
    返回值：指向二维数组首地址的 int** 指针。
*/
int** createArray(int rows, int cols) {
    // arr 本身是“行指针数组”，arr[i] 指向第 i 行的首元素。
    int** arr = new int*[rows];
    for (int i = 0; i < rows; ++i) {
        arr[i] = new int[cols];
    }
    return arr;
}

/*
    函数功能：从键盘输入二维数组所有元素。
    参数 arr：待输入的二维数组。
    参数 rows、cols：数组行数和列数。
    参数 name：数组名称，用于输出提示。
*/
void inputArray(int** arr, int rows, int cols, const string& name) {
    cout << "请输入数组 " << name << " 的 " << rows << " x " << cols << " 个整数：" << endl;
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            arr[i][j] = readInt(name + "[" + to_string(i) + "][" + to_string(j) + "] = ");
        }
    }
}

/*
    函数功能：按矩阵形式输出二维数组。
    setw 用来设置每个元素的输出宽度，使各列更整齐。
*/
void printArray(int** arr, int rows, int cols, const string& name) {
    cout << endl << "数组 " << name << " 的内容如下：" << endl;
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            cout << setw(6) << arr[i][j];
        }
        cout << endl;
    }
}

/*
    函数功能：计算二维数组所有元素的总和。
    返回值：数组元素累加结果。
*/
int sumArray(int** arr, int rows, int cols) {
    int sum = 0;
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            sum += arr[i][j];
        }
    }
    return sum;
}

/*
    函数功能：查找二维数组中的最大值。
    返回值：数组中最大的整数。
*/
int findMaxValue(int** arr, int rows, int cols) {
    int maxValue = arr[0][0];

    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            if (arr[i][j] > maxValue) {
                maxValue = arr[i][j];
            }
        }
    }
    return maxValue;
}

/*
    函数功能：统计某个目标值在二维数组中出现的次数。
    参数 target：需要统计的目标值，本题中就是最大值。
*/
int countValue(int** arr, int rows, int cols, int target) {
    int count = 0;
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            if (arr[i][j] == target) {
                ++count;
            }
        }
    }
    return count;
}

/*
    函数功能：输出目标值在二维数组中的所有位置。
    同时输出从 0 开始的数组下标和从 1 开始的日常编号。
*/
void printValuePositions(int** arr, int rows, int cols, int target) {
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            if (arr[i][j] == target) {
                cout << "下标位置：第 " << i << " 行，第 " << j << " 列";
                cout << "；从 1 开始编号：第 " << i + 1 << " 行，第 " << j + 1 << " 列" << endl;
            }
        }
    }
}

/*
    函数功能：释放动态申请的二维数组空间。
    释放顺序必须和申请顺序相反，先释放每一行，再释放行指针数组。
*/
void destroyArray(int** arr, int rows) {
    for (int i = 0; i < rows; ++i) {
        delete[] arr[i];
    }
    delete[] arr;
}

int main() {
    // 动态申请两个 3 x 6 的二维数组
    int** B1 = createArray(ROWS, COLS);
    int** B2 = createArray(ROWS, COLS);

    inputArray(B1, ROWS, COLS, "B1");
    inputArray(B2, ROWS, COLS, "B2");

    printArray(B1, ROWS, COLS, "B1");
    printArray(B2, ROWS, COLS, "B2");

    cout << endl << "B1 的元素总和为：" << sumArray(B1, ROWS, COLS) << endl;

    int maxValue = findMaxValue(B2, ROWS, COLS);
    int maxCount = countValue(B2, ROWS, COLS, maxValue);
    cout << "B2 的最大元素为：" << maxValue << endl;
    cout << "最大元素共出现 " << maxCount << " 次，位置如下：" << endl;
    printValuePositions(B2, ROWS, COLS, maxValue);

    // 动态申请的空间必须手动释放
    destroyArray(B1, ROWS);
    destroyArray(B2, ROWS);

    return 0;
}
