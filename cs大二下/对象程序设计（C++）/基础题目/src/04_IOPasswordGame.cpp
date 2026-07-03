#include <algorithm>
#include <ctime>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using namespace std;

/*
    题目 4：I/O 流实验 - 猜密码游戏

    说明：
    题目说“生成一个 4 位不重复数字的密码，每位 0-9”，同时又说用户输入“4 位整数”。
    如果密码允许首位为 0，例如 0123，那么用 int 输入会丢失开头的 0。

    为了保证逻辑严谨，本程序使用 string 读取用户输入：
        1. 长度必须为 4
        2. 每个字符必须是数字
        3. 四个数字不能重复

    这样既能支持 0123，也能完成题目对合法性的检查。

    补充说明：
    密码可能以 0 开头，例如 0123。若使用 int 读取，会丢失首位 0；
    使用 string 可以完整保存四个字符，方便检查长度、数字字符和重复情况。

    生成密码时先准备 '0' 到 '9'，用 shuffle 打乱后取前 4 个字符，
    这样可以自然保证四位数字不重复。
*/

/*
    函数功能：判断字符串中是否存在重复数字字符。
    返回值：存在重复字符返回 true，否则返回 false。
*/
bool hasRepeatedDigit(const string& text) {
    for (int i = 0; i < static_cast<int>(text.size()); ++i) {
        for (int j = i + 1; j < static_cast<int>(text.size()); ++j) {
            if (text[i] == text[j]) {
                return true;
            }
        }
    }
    return false;
}

/*
    函数功能：检查用户输入的密码猜测是否合法。
    合法条件：长度为 4、每一位都是数字、四个数字互不重复。
*/
bool isValidGuess(const string& guess) {
    if (guess.size() != 4) {
        return false;
    }

    for (char ch : guess) {
        if (ch < '0' || ch > '9') {
            return false;
        }
    }

    return !hasRepeatedDigit(guess);
}

/*
    函数功能：随机生成一个四位不重复数字密码。
    返回值：长度为 4 的字符串，可能以 0 开头。
*/
string generatePassword() {
    vector<char> digits;
    for (char ch = '0'; ch <= '9'; ++ch) {
        digits.push_back(ch);
    }

    random_device rd;
    mt19937 generator(rd());
    // shuffle 是 C++ 标准库算法，用现代随机数引擎 mt19937 打乱数字顺序。
    shuffle(digits.begin(), digits.end(), generator);

    string password;
    for (int i = 0; i < 4; ++i) {
        password.push_back(digits[i]);
    }
    return password;
}

/*
    函数功能：统计“数字存在但位置错误”的个数。
    只有 guess[i] 出现在 password[j] 且 i != j 时才计数。
*/
int countRightDigitWrongPosition(const string& password, const string& guess) {
    int count = 0;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            if (i != j && guess[i] == password[j]) {
                ++count;
            }
        }
    }
    return count;
}

int main() {
    string password = generatePassword();
    string guess;
    int guessCount = 0;

    cout << "========== 猜密码游戏 ==========" << endl;
    cout << "系统已经生成一个 4 位不重复数字密码。" << endl;
    cout << "请输入你的猜测，例如 1234。若密码首位为 0，本程序也能正确处理。" << endl;

    while (true) {
        cout << endl << "请输入 4 位不重复数字：";
        cin >> guess;

        if (!isValidGuess(guess)) {
            cout << "输入不合法：必须是 4 位数字，且各位数字不能重复。请重新输入。" << endl;
            continue;
        }

        ++guessCount;

        if (guess == password) {
            cout << "猜对了！恭喜你猜中了密码。" << endl;
            cout << "你一共猜了 " << guessCount << " 次。" << endl;
            break;
        }

        int wrongPositionCount = countRightDigitWrongPosition(password, guess);
        if (wrongPositionCount > 0) {
            cout << "有 " << wrongPositionCount << " 个数字正确但位置错误。" << endl;
        } else {
            cout << "无正确数字。" << endl;
        }
    }

    return 0;
}
