#include <iostream>
#include <limits>
#include <sstream>
#include <string>

using namespace std;

/*
    实验 2.2：类与对象实验 - Vector 向量类

    要求：
    1. Vector 类包含向量维度 rows 和动态数组指针 data。
    2. 构造函数根据维度动态申请空间。
    3. 析构函数释放动态空间。
    4. 拷贝构造函数执行深拷贝。
    5. 输入函数从 cin 输入元素。
    6. 输出函数格式化输出向量。
    7. 实现同维向量相加，返回新的 Vector 对象。
    8. 实现同维向量点乘，返回整型结果。
    9. 重载赋值运算符 =，处理自赋值并执行深拷贝。

    补充说明：
    Vector 内部保存 int* data，涉及动态内存。编译器默认生成的拷贝方式只会
    复制指针地址，容易让多个对象指向同一块数组，析构时造成重复释放。
    因此这里手动编写拷贝构造函数和赋值运算符，让每个 Vector 都拥有自己的
    独立数组空间，这种方式称为深拷贝。

    operator= 中先判断 this == &other，用于处理 V1 = V1 这种自赋值情况。
*/

/*
    函数功能：读取一个整数，并处理输入类型错误。
    参数 prompt：输入提示文字。
    返回值：用户输入的合法整数。

    向量元素限制在 [-10000, 10000] 内，避免极端输入造成点乘结果
    不适合普通整型实验演示。
*/
int readInt(const string& prompt) {
    const long long MIN_VALUE = -10000;
    const long long MAX_VALUE = 10000;
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

class Vector {
private:
    int rows;
    int* data;

public:
    explicit Vector(int dimension = 0) : rows(dimension), data(nullptr) {
        if (rows > 0) {
            data = new int[rows];
            for (int i = 0; i < rows; ++i) {
                data[i] = 0;
            }
        }
        cout << "[构造] Vector，维度 = " << rows << endl;
    }

    ~Vector() {
        cout << "[析构] Vector，维度 = " << rows << endl;
        delete[] data;
    }

    Vector(const Vector& other) : rows(other.rows), data(nullptr) {
        // 拷贝构造常用于：Vector V4 = V1; 或函数返回 Vector 临时对象时。
        if (rows > 0) {
            data = new int[rows];
            for (int i = 0; i < rows; ++i) {
                data[i] = other.data[i];
            }
        }
        cout << "[拷贝构造] Vector，维度 = " << rows << endl;
    }

    Vector& operator=(const Vector& other) {
        // this 是当前对象的地址，&other 是右侧对象的地址。
        if (this == &other) {
            return *this;
        }

        delete[] data;
        rows = other.rows;
        data = nullptr;

        if (rows > 0) {
            data = new int[rows];
            for (int i = 0; i < rows; ++i) {
                data[i] = other.data[i];
            }
        }
        return *this;
    }

    /*
        函数功能：输入当前向量的所有元素。
        参数 name：向量名称，用来生成提示文字。
    */
    void input(const string& name) {
        cout << "请输入向量 " << name << " 的 " << rows << " 个元素：" << endl;
        for (int i = 0; i < rows; ++i) {
            data[i] = readInt(name + "[" + to_string(i) + "] = ");
        }
    }

    /*
        函数功能：按 [ a b c ] 的形式输出向量。
        最后的 const 表示该函数只读取数据，不修改对象。
    */
    void print(const string& name) const {
        cout << name << " = [ ";
        for (int i = 0; i < rows; ++i) {
            cout << data[i] << " ";
        }
        cout << "]" << endl;
    }

    /*
        函数功能：实现两个同维度向量相加。
        参数 other：另一个 Vector 对象。
        返回值：保存相加结果的新 Vector 对象。
    */
    Vector add(const Vector& other) const {
        // 返回值是一个新的 Vector 对象，不修改参与相加的两个原向量。
        if (rows != other.rows) {
            cout << "错误：两个向量维度不同，不能相加。" << endl;
            return Vector();
        }

        Vector result(rows);
        for (int i = 0; i < rows; ++i) {
            result.data[i] = data[i] + other.data[i];
        }
        return result;
    }

    /*
        函数功能：计算两个同维度向量的点乘。
        返回值：对应元素乘积的累加和。
    */
    int dot(const Vector& other) const {
        if (rows != other.rows) {
            cout << "错误：两个向量维度不同，不能点乘。" << endl;
            return 0;
        }

        int result = 0;
        for (int i = 0; i < rows; ++i) {
            result += data[i] * other.data[i];
        }
        return result;
    }
};

int main() {
    cout << "========== 实验 2.2：Vector 向量类 ==========" << endl;

    Vector V1(8);
    Vector V2(8);
    Vector V3(8);

    V1.input("V1");
    V2.input("V2");

    V3 = V1.add(V2);
    V3.print("V3 = V1 + V2");
    cout << "V1 和 V2 的点乘结果为：" << V1.dot(V2) << endl;

    cout << endl << "下面使用 new 动态创建 Vector 对象：" << endl;
    Vector* pV1 = new Vector(6);
    Vector* pV2 = new Vector(6);
    Vector* pV3 = new Vector(6);

    pV1->input("pV1");
    pV2->input("pV2");

    *pV3 = pV1->add(*pV2);
    pV3->print("*pV3 = *pV1 + *pV2");
    cout << "pV1 和 pV2 的点乘结果为：" << pV1->dot(*pV2) << endl;

    delete pV1;
    delete pV2;
    delete pV3;

    return 0;
}
