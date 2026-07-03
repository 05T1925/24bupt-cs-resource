# C++ 基础实验解答说明

本目录包含《面向对象程序设计实践（C++）》基础实验的独立解答文件。

## 文件说明

1. `01_ArrayDynamic.cpp`
   - 题目 1：动态二维数组
   - 包含输入、输出、求和、最大值查找、多个最大值统计、动态内存释放

2. `02_1_Triangle.cpp`
   - 题目 2.1：三角形
   - 包含 Point 类、Triangle 类、周长面积计算、全等判断、构造析构观察

3. `02_2_Vector.cpp`
   - 题目 2.2：Vector 向量类
   - 包含动态数组、构造函数、析构函数、拷贝构造函数、赋值运算符、向量相加、点乘

4. `03_InheritanceVehicle.cpp`
   - 题目 3：继承与派生
   - 包含 Vehicle、Car、Ship、Truck 类
   - 行驶距离和卡车载重由用户输入，并检查油耗是否超过油箱容量

5. `04_IOPasswordGame.cpp`
   - 题目 4：I/O 流猜密码游戏
   - 使用字符串读取输入，支持类似 `0123` 的密码

6. `05_1_VirtualFunction.cpp`
   - 题目 5.1：虚函数
   - 基于题目三的 Vehicle、Car、Ship、Truck 完整交通工具结构
   - 包含非虚函数、虚函数、纯虚函数与抽象类对比
   - 非虚函数、虚函数、抽象类三段测试都会分别输入汽车公里数、轮船海里数、卡车公里数和卡车载重

7. `05_2_PointOperatorOverload.cpp`
   - 题目 5.2：Point 运算符重载
   - 包含 Point 的 `+`、`-` 运算符重载测试

## 程序说明

所有程序都加入了基础输入合法性检查。输入类型错误、不符合题意，或数值明显超出普通实验范围时，程序会提示重新输入。

源码注释中补充了相关知识点，例如：

- `new` / `delete[]` 的配对释放
- 构造函数和析构函数调用顺序
- 深拷贝、拷贝构造函数、赋值运算符
- `private`、`protected`、`public` 的区别
- 非虚函数、虚函数、纯虚函数、抽象类的区别
- 运算符重载中成员函数和友元函数的区别

## VS Code 编译方式

在 VS Code 终端中进入本目录后，可以使用如下命令分别编译：

```powershell
g++ -std=c++17 01_ArrayDynamic.cpp -o 01_ArrayDynamic.exe
g++ -std=c++17 02_1_Triangle.cpp -o 02_1_Triangle.exe
g++ -std=c++17 02_2_Vector.cpp -o 02_2_Vector.exe
g++ -std=c++17 03_InheritanceVehicle.cpp -o 03_InheritanceVehicle.exe
g++ -std=c++17 04_IOPasswordGame.cpp -o 04_IOPasswordGame.exe
g++ -std=c++17 05_1_VirtualFunction.cpp -o 05_1_VirtualFunction.exe
g++ -std=c++17 05_2_PointOperatorOverload.cpp -o 05_2_PointOperatorOverload.exe
```

然后运行对应的 `.exe` 文件即可。

例如：

```powershell
.\01_ArrayDynamic.exe
```
