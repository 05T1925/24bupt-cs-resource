#include <iostream>
#include <limits>
#include <sstream>
#include <string>

using namespace std;

/*
    实验 5.2：Point 类运算符重载

    要求：
    1. Point 类属性为坐标 x、y。
    2. 重载 +：
       - Point + Point => (x1 + x2, y1 + y2)
       - Point + int   => (x + n, y + n)
    3. 重载 -：
       - Point - Point => (x1 - x2, y1 - y2)
       - Point - int   => (x - n, y - n)
    4. 主函数创建多个 Point 对象并测试不同运算。

    补充说明：
    p1 + p2 本质上会调用 p1.operator+(p2)，因此左操作数是 Point 时，
    可以把运算符重载写成成员函数。

    number + p1 的左操作数是 int，不是 Point 对象，无法调用 Point 的成员函数，
    所以这里额外提供一个类外友元函数来支持这种写法。

    参数使用 const Point& 可以避免复制对象，const 也能保证函数内部不修改实参。
*/

/*
    函数功能：读取一个整数，并处理非法输入。
    参数 prompt：输入提示文字。
    返回值：用户输入的合法整数。

    坐标和测试整数限制在 [-1000000, 1000000] 内，避免极端输入影响
    运算结果的可读性和实验演示效果。
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

class Point {
private:
    int x;
    int y;

public:
    Point(int xValue = 0, int yValue = 0) : x(xValue), y(yValue) {}

    /*
        函数功能：重载 Point + Point。
        返回值：两个点坐标分别相加后形成的新 Point。
    */
    Point operator+(const Point& other) const {
        // p1 + p2：当前对象是 p1，other 是 p2。
        return Point(x + other.x, y + other.y);
    }

    /*
        函数功能：重载 Point + int。
        返回值：当前点的 x、y 坐标都加上 number 后形成的新 Point。
    */
    Point operator+(int number) const {
        // p1 + number：两个坐标都加上同一个整数。
        return Point(x + number, y + number);
    }

    /*
        函数功能：重载 Point - Point。
        返回值：两个点坐标分别相减后形成的新 Point。
    */
    Point operator-(const Point& other) const {
        return Point(x - other.x, y - other.y);
    }

    /*
        函数功能：重载 Point - int。
        返回值：当前点的 x、y 坐标都减去 number 后形成的新 Point。
    */
    Point operator-(int number) const {
        return Point(x - number, y - number);
    }

    /*
        函数功能：输出点的坐标。
        参数 name：输出时显示的点名或表达式名称。
    */
    void print(const string& name) const {
        // 最后的 const 表示 print 只输出，不改变当前点的 x、y。
        cout << name << " = (" << x << ", " << y << ")" << endl;
    }

    // 题目只要求 Point + int，但补充 int + Point 可以让测试更完整。
    friend Point operator+(int number, const Point& p);
    //友元函数   friend：让外部函数能访问类的私有 x、y  
	//  eg.为了支持：5 + p1，否则只能 p1 + 5
};

/*
    函数功能：支持 int + Point 的写法。
    因为左操作数是 int，不能写成 Point 的成员函数，所以写成类外友元函数。
*/
Point operator+(int number, const Point& p) {
    return Point(number + p.x, number + p.y);
}

int main() {
    cout << "========== 实验 5.2：Point 运算符重载 ==========" << endl;

    int x1, y1;
    int x2, y2;
    int number;

    x1 = readInt("请输入 p1 的 x：");
    y1 = readInt("请输入 p1 的 y：");
    x2 = readInt("请输入 p2 的 x：");
    y2 = readInt("请输入 p2 的 y：");
    number = readInt("请输入一个整数 number，用于测试 Point 与整数的加减运算：");

    Point p1(x1, y1);
    Point p2(x2, y2);

    Point p3 = p1 + p2;
    Point p4 = p1 + number;
    Point p5 = p2 - p1;
    Point p6 = p2 - number;
    Point p7 = number + p1;

    p1.print("p1");
    p2.print("p2");
    p3.print("p1 + p2");
    p4.print("p1 + number");
    p5.print("p2 - p1");
    p6.print("p2 - number");
    p7.print("number + p1");

    return 0;
}
