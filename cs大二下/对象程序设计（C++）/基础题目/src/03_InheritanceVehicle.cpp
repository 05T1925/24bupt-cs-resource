#include <iostream>
#include <cmath>
#include <limits>
#include <sstream>
#include <string>

using namespace std;

/*
    题目 3：继承与派生实验

    类结构：
        Vehicle
        ├── Car
        │   └── Truck
        └── Ship

    本题中的 Vehicle::calcFuelConsumption 没有具体实现。
    因为本题暂时没有要求虚函数，所以这里先写成普通成员函数。
    第 5 题会进一步比较“非虚函数、虚函数、纯虚函数”的区别。

    补充说明：
    派生类对象创建时，会先构造基类部分，再构造派生类部分。Truck 继承自 Car，
    Car 又继承自 Vehicle，所以 Truck 的构造顺序是 Vehicle -> Car -> Truck。
    析构顺序与构造顺序相反。

    protected 成员不能被类外直接访问，但派生类可以访问，适合保存希望由子类
    继续使用、又不想完全公开的数据。本题中 Truck 需要使用 Car 的油箱容量和
    百公里油耗，所以这些成员放在 protected 区域。

    本题主要展示普通继承与派生，虚函数放在实验 5.1 中单独比较。
*/

/*
    函数功能：读取一个大于 0 的 double 类型数字。
    用于行驶距离、海里数和载重等必须为正数的输入。

    同时设置上限，避免极大的输入值导致油耗计算失去实际意义。
*/
double readPositiveDouble(const string& prompt) {
    const double MAX_VALUE = 1000000.0;
    string line;
    while (true) {
        cout << prompt;
        getline(cin >> ws, line);

        stringstream ss(line);
        double value;
        char extra;
        if (ss >> value && !(ss >> extra) && isfinite(value) && value > 0 && value <= MAX_VALUE) {
            return value;
        }

        cout << "输入不合法，请输入 0 到 " << MAX_VALUE << " 之间的正数。" << endl;
    }
}

/*
    函数功能：输出观察提示，并在 Windows 环境下暂停程序。
    用于查看构造函数和析构函数的调用顺序。
*/
void pauseForObserve(const string& message) {
    cout << message << endl;
#ifdef _WIN32
    // 题目要求观察构造、析构顺序时使用 system("pause")。
    system("pause");
#endif
}

class Vehicle {
protected:
    string name;

public:
    explicit Vehicle(const string& vehicleName = "Vehicle") : name(vehicleName) {
        cout << "[构造] Vehicle 基类部分：" << name << endl;
    }

    ~Vehicle() {
        cout << "[析构] Vehicle 基类部分：" << name << endl;
    }

    double calcFuelConsumption(double distance) const {
        (void)distance;
        cout << "Vehicle 是泛指交通工具，无法直接计算油耗。" << endl;
        return 0.0;
    }
};

class Car : public Vehicle {
protected:
    double tankCapacity;       // 油箱容量，单位：升
    double fuelPer100Km;       // 百公里油耗，单位：升 / 100 公里
    // protected：Truck 是 Car 的派生类，可以直接使用 fuelPer100Km。

public:
    Car(const string& carName, double tank, double fuel)
        : Vehicle(carName), tankCapacity(tank), fuelPer100Km(fuel) {
        cout << "[构造] Car 对象：" << name << endl;
    }

    ~Car() {
        cout << "[析构] Car 对象：" << name << endl;
    }

    double calcFuelConsumption(double kilometers) const {
        // 汽车油耗公式：行驶公里数 / 100 * 百公里油耗。
        return kilometers / 100.0 * fuelPer100Km;
    }

    double getTankCapacity() const {
        return tankCapacity;
    }

    void printInfo() const {
        cout << name << "：油箱容量 " << tankCapacity
             << " L，百公里油耗 " << fuelPer100Km << " L" << endl;
    }
};

class Ship : public Vehicle {
private:
    double tankCapacity;       // 油箱容量，单位：升
    double fuelPerNauticalMile;// 每海里油耗，单位：升 / 海里

public:
    Ship(const string& shipName, double tank, double fuel)
        : Vehicle(shipName), tankCapacity(tank), fuelPerNauticalMile(fuel) {
        cout << "[构造] Ship 对象：" << name << endl;
    }

    ~Ship() {
        cout << "[析构] Ship 对象：" << name << endl;
    }

    double calcFuelConsumption(double nauticalMiles) const {
        // 轮船油耗公式：海里数 * 每海里油耗。
        return nauticalMiles * fuelPerNauticalMile;
    }

    double getTankCapacity() const {
        return tankCapacity;
    }

    void printInfo() const {
        cout << name << "：油箱容量 " << tankCapacity
             << " L，每海里油耗 " << fuelPerNauticalMile << " L" << endl;
    }
};

class Truck : public Car {
private:
    double loadLimit;          // 载重限制，单位：吨

public:
    Truck(const string& truckName, double tank, double fuel, double load)
        : Car(truckName, tank, fuel), loadLimit(load) {
        cout << "[构造] Truck 对象：" << name << endl;
    }

    ~Truck() {
        cout << "[析构] Truck 对象：" << name << endl;
    }

    double calcFuelConsumption(double kilometers) const {
        // 这里使用和 Car 相同的计算方式，也可以根据载重进一步扩展。
        return kilometers / 100.0 * fuelPer100Km;
    }

    bool isLoadAllowed(double actualLoad) const {
        return actualLoad <= loadLimit;
    }

    double getLoadLimit() const {
        return loadLimit;
    }

    void printInfo() const {
        cout << name << "：油箱容量 " << tankCapacity
             << " L，百公里油耗 " << fuelPer100Km
             << " L，载重限制 " << loadLimit << " 吨" << endl;
    }
};

/*
    函数功能：输出某种交通工具的油耗结果，并检查是否超过油箱容量。
    如果油耗大于油箱容量，说明单箱油无法完成本次行驶。
*/
void printFuelResult(const string& name, double distance, const string& unit, double fuel, double tankCapacity) {
    cout << name << "行驶 " << distance << " " << unit << " 的油耗为：" << fuel << " L" << endl;
    if (fuel <= tankCapacity) {
        cout << "油耗未超过油箱容量 " << tankCapacity << " L，可以完成本次行驶。" << endl;
    } else {
        cout << "油耗超过油箱容量 " << tankCapacity << " L，单箱油无法完成本次行驶。" << endl;
    }
}

int main() {
    cout << "========== 交通工具继承与派生实验 ==========" << endl;

    Car car("家用汽车", 50, 7.5);
    Ship ship("远洋轮船", 5000, 30);
    Truck truck("运输卡车", 120, 18, 10);

    pauseForObserve("三个对象已经创建。请观察构造顺序：先基类，后派生类。");

    car.printInfo();
    ship.printInfo();
    truck.printInfo();

    double carKilometers = readPositiveDouble("\n请输入汽车行驶公里数：");
    double shipNauticalMiles = readPositiveDouble("请输入轮船行驶海里数：");
    double truckKilometers = readPositiveDouble("请输入卡车行驶公里数：");
    double truckActualLoad = readPositiveDouble("请输入卡车实际载重（吨）：");

    cout << endl;
    printFuelResult("汽车", carKilometers, "公里",
                    car.calcFuelConsumption(carKilometers), car.getTankCapacity());
    printFuelResult("轮船", shipNauticalMiles, "海里",
                    ship.calcFuelConsumption(shipNauticalMiles), ship.getTankCapacity());

    if (!truck.isLoadAllowed(truckActualLoad)) {
        cout << "卡车实际载重 " << truckActualLoad << " 吨超过载重限制 "
             << truck.getLoadLimit() << " 吨，不能出车。" << endl;
    } else {
        cout << "卡车实际载重 " << truckActualLoad << " 吨，未超过载重限制。" << endl;
        printFuelResult("卡车", truckKilometers, "公里",
                        truck.calcFuelConsumption(truckKilometers), truck.getTankCapacity());
    }

    cout << endl;
    cout << "构造与析构顺序说明：" << endl;
    cout << "1. 构造派生类对象时，先构造基类部分，再构造派生类部分。" << endl;
    cout << "2. Truck 继承自 Car，Car 又继承自 Vehicle，所以 Truck 的构造顺序是 Vehicle -> Car -> Truck。" << endl;
    cout << "3. 析构顺序与构造顺序相反，所以 Truck 的析构顺序是 Truck -> Car -> Vehicle。" << endl;
    cout << "4. 局部对象按创建的相反顺序析构，因此 main 结束时通常先析构 truck，再析构 ship，最后析构 car。" << endl;

    pauseForObserve("程序即将结束。请继续观察析构函数输出。");
    return 0;
}
