#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>

using namespace std;

/*
    实验 2.1：类与对象实验 - 三角形

    要求：
    1. 定义 Point 类，属性为点坐标 x、y。
    2. 用户不输入坐标时，Point 默认初始化为 (1, 1)。
    3. Point 类提供计算两点距离的成员函数。
    4. 定义 Triangle 类，包含三个 Point 顶点。
    5. Triangle 类提供计算周长和面积的成员函数。
    6. 创建两个 Triangle 对象，输入顶点坐标，输出周长和面积。
    7. 判断两个三角形是否全等。
    8. 观察构造函数和析构函数调用顺序。

    补充说明：
    Triangle 对象中包含三个 Point 成员对象，所以创建 Triangle 时会先构造
    Point 成员，再执行 Triangle 自己的构造函数体。对象销毁时顺序相反，
    先执行 Triangle 的析构函数体，再销毁内部的 Point 成员。

    边长由 sqrt 计算得到，属于 double 浮点数。浮点数直接比较可能受到极小
    误差影响，所以判断三角形成立时使用 EPS 作为容差。
*/

void waitPause(const string& message) {
    cout << message << endl;
#ifdef _WIN32
    // 题目要求使用 system("pause") 观察构造和析构调用。
    system("pause");
#endif
}

/*
    函数功能：读取一个 double 类型数字，并处理非法输入。
    参数 prompt：输入提示文字。
    返回值：用户输入的合法数字。

    坐标值限制在 [-1000000, 1000000] 内，防止极端输入导致距离、面积
    计算结果失去普通实验测试意义。
*/
double readDouble(const string& prompt) {
    const double MIN_VALUE = -1000000.0;
    const double MAX_VALUE = 1000000.0;
    string line;
    while (true) {
        cout << prompt;
        getline(cin >> ws, line);

        stringstream ss(line);
        double value;
        char extra;
        if (ss >> value && !(ss >> extra) &&
            isfinite(value) && value >= MIN_VALUE && value <= MAX_VALUE) {
            return value;
        }

        cout << "输入不合法，请输入 " << MIN_VALUE << " 到 " << MAX_VALUE << " 之间的数字。" << endl;
    }
}

class Point {
private:
    double x;
    double y;
    string objectName;

public:
    Point(double xValue = 1, double yValue = 1, const string& name = "Point")
        : x(xValue), y(yValue), objectName(name) {
        cout << "[构造] Point 对象 " << objectName
             << "，坐标为 (" << x << ", " << y << ")" << endl;
    }

    ~Point() {
        cout << "[析构] Point 对象 " << objectName
             << "，坐标为 (" << x << ", " << y << ")" << endl;
    }

    void set(double xValue, double yValue) {
        x = xValue;
        y = yValue;
    }

    /*
        函数功能：计算当前点到另一个点的距离。
        参数 other：另一个 Point 对象。
        返回值：两点之间的欧几里得距离。
    */
    double distanceTo(const Point& other) const {
        // const Point& 避免复制整个对象，最后的 const 表示本函数不修改当前 Point。
        double dx = x - other.x;
        double dy = y - other.y;
        return sqrt(dx * dx + dy * dy);
    }
};

class Triangle {
private:
    Point a;
    Point b;
    Point c;
    string objectName;

public:
    Triangle(const string& name = "Triangle")
        : a(1, 1, name + ".A"),
          b(1, 1, name + ".B"),
          c(1, 1, name + ".C"),
          objectName(name) {
        cout << "[构造] Triangle 对象 " << objectName << endl;
    }

    ~Triangle() {
        cout << "[析构] Triangle 对象 " << objectName << endl;
    }

    /*
        函数功能：输入三角形三个顶点坐标。
        每个坐标都通过 readDouble 读取，避免非数字输入破坏程序流程。
    */
    void input() {
        double x, y;
        cout << endl << "请输入 " << objectName << " 的三个顶点坐标：" << endl;

        x = readDouble("顶点 A 的 x：");
        y = readDouble("顶点 A 的 y：");
        a.set(x, y);

        x = readDouble("顶点 B 的 x：");
        y = readDouble("顶点 B 的 y：");
        b.set(x, y);

        x = readDouble("顶点 C 的 x：");
        y = readDouble("顶点 C 的 y：");
        c.set(x, y);
    }

    double sideAB() const {
        return a.distanceTo(b);
    }

    double sideBC() const {
        return b.distanceTo(c);
    }

    double sideCA() const {
        return c.distanceTo(a);
    }

    /*
        函数功能：计算三角形周长。
        返回值：三条边长之和。
    */
    double perimeter() const {
        return sideAB() + sideBC() + sideCA();
    }

    /*
        函数功能：使用海伦公式计算面积。
        返回值：三角形面积；若浮点误差导致根号内出现极小负数，则修正为 0。
    */
    double area() const {
        double ab = sideAB();
        double bc = sideBC();
        double ca = sideCA();
        double p = (ab + bc + ca) / 2.0;
        double value = p * (p - ab) * (p - bc) * (p - ca);

        if (value < 0 && value > -1e-9) {
            value = 0;
        }
        return sqrt(value);
    }

    /*
        函数功能：判断三个顶点是否能构成真正的三角形。
        返回值：能构成三角形返回 true，否则返回 false。
    */
    bool isValid() const {
        double ab = sideAB();
        double bc = sideBC();
        double ca = sideCA();

        /*
            三角形成立条件：
                任意两边之和必须“大于”第三边。

            注意：这里不能直接写 ab + bc > ca。
            例如点 (0,0)、(1,1)、(3,3) 共线，理论上：
                AB + BC == CA
            但 double 浮点计算时可能出现极小误差，使得
                AB + BC 比 CA 大 0.000000000000001 左右，
            于是程序会误判为可以构成三角形。

            因此使用 EPS 作为误差范围。只有当两边之和明显大于第三边时，
            才认为三角形成立。
        */
        const double EPS = 1e-6;
        return ab + bc > ca + EPS &&
               ab + ca > bc + EPS &&
               bc + ca > ab + EPS;
    }

    /*
        函数功能：输出三角形的边长、周长和面积。
        如果三点不能构成三角形，只输出提示信息，不再计算周长和面积。
    */
    void printInfo() const {
        cout << fixed << setprecision(2);
        
		

        if (!isValid()) {
            cout << objectName << " 的三个点不能构成三角形。" << endl;
            return;
        }
        else {
			cout << objectName << " 的三边长分别为："
			             << sideAB() << ", " << sideBC() << ", " << sideCA() << endl;
			        cout << objectName << " 的周长为：" << perimeter() << endl;
			        cout << objectName << " 的面积为：" << area() << endl;
		}

    }

    /*
        函数功能：判断当前三角形与另一个三角形是否全等。
        参数 other：另一个 Triangle 对象。
        返回值：三边对应相等时返回 true。
    */
    bool congruentTo(const Triangle& other) const {
        /*
            三边分别相等可以判断两个三角形全等（SSS）。
            这里先排序，是为了避免用户输入顶点顺序不同导致边的顺序不同。
        */
        double s1[3] = {sideAB(), sideBC(), sideCA()};
        double s2[3] = {other.sideAB(), other.sideBC(), other.sideCA()};
        sort(s1, s1 + 3);
        sort(s2, s2 + 3);

        const double EPS = 1e-6;
        for (int i = 0; i < 3; ++i) {
            if (fabs(s1[i] - s2[i]) > EPS) {
                return false;
            }
        }
        return true;
    }
};

int main() {
    cout << "========== 实验 2.1：三角形 ==========" << endl;

    Triangle T1("T1");
    Triangle T2("T2");

    waitPause("两个 Triangle 对象已经创建。先构造成员 Point，再构造 Triangle 本身。");

    T1.input();
    T2.input();

    T1.printInfo();
    T2.printInfo();

    if (T1.isValid() && T2.isValid() && T1.congruentTo(T2)) {
        cout << "两个三角形全等。" << endl;
    } else {
        cout << "两个三角形不全等。" << endl;
    }

    cout << endl;
    cout << "构造与析构顺序说明：" << endl;
    cout << "1. 创建 Triangle 对象时，会先构造它的成员对象 Point A、B、C。" << endl;
    cout << "2. 成员对象都构造完成后，才执行 Triangle 构造函数体。" << endl;
    cout << "3. 离开 main 函数时析构顺序与构造顺序相反。" << endl;
    cout << "4. 局部对象 T2 后创建，所以通常会比 T1 先析构。" << endl;

    waitPause("程序即将结束。请继续观察析构函数输出。");
    return 0;
}
