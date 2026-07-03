#include <iostream>
#include <limits>
#include <string>

using namespace std;

/*
    实验 5.1：虚函数

    本文件严格关联题目三“交通工具”类，仍然使用下面的继承结构：

        Vehicle
        ├── Car
        │   └── Truck
        └── Ship

    本题要比较三件事：

    1. 非虚函数：
       基类指针调用 calcFuelConsumption 时，只看“指针声明类型”。
       即使 Vehicle* 实际指向 Car / Ship / Truck，也会调用 Vehicle 的函数。

    2. 虚函数：
       基类指针调用 calcFuelConsumption 时，看“对象真实类型”。
       Vehicle* 指向 Car，就调用 Car 的函数；指向 Ship，就调用 Ship 的函数。

    3. 纯虚函数与抽象类：
       Vehicle 只规定“交通工具必须会计算油耗”这个接口，自己不提供公式。
       含有纯虚函数的 Vehicle 不能创建对象，只能让 Car / Ship / Truck 继承并实现。


*/

struct TravelInput {
    // 一组通行测试数据，供某一段测试函数使用。
    double carKilometers;
    double shipNauticalMiles;
    double truckKilometers;
    double truckActualLoad;
};

/*
    函数功能：读取一个大于 0 的 double 类型数字。
    参数 prompt：输入提示文字。
    返回值：用户输入的合法正数。

    cin.fail() 用来判断输入类型是否错误，例如本应输入数字却输入了字母。
    clear() 清除错误状态，ignore() 丢弃本行剩余内容，然后重新输入。
*/
double readPositiveDouble(const string& prompt) {
    double value;
    while (true) {
        cout << prompt;
        cin >> value;
        if (!cin.fail() && value > 0) {
            return value;
        }

        cout << "输入不合法，请输入大于 0 的数字。" << endl;
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
}

/*
    函数功能：集中读取一次交通工具通行测试数据。
    参数 title：当前测试部分的标题，例如“虚函数测试”。
    返回值：TravelInput 结构体，包含汽车公里数、轮船海里数、卡车公里数和卡车实际载重。

    三种交通工具的单位不同：汽车和卡车按公里计算，轮船按海里计算。
    卡车额外输入实际载重，用来与载重限制比较。
*/
TravelInput readTravelInput(const string& title) {
    cout << endl << "请输入" << title << "所需的通行数据：" << endl;
    TravelInput input;
    input.carKilometers = readPositiveDouble("请输入汽车行驶公里数：");
    input.shipNauticalMiles = readPositiveDouble("请输入轮船行驶海里数：");
    input.truckKilometers = readPositiveDouble("请输入卡车行驶公里数：");
    input.truckActualLoad = readPositiveDouble("请输入卡车实际载重（吨）：");
    return input;
}

/*
    函数功能：输出某种交通工具的油耗结果，并检查是否超过油箱容量。
    参数 name：交通工具名称。
    参数 distance：行驶距离。
    参数 unit：距离单位，汽车/卡车是“公里”，轮船是“海里”。
    参数 fuel：根据公式计算出的油耗。
    参数 tankCapacity：油箱容量，油耗超过该值时提示单箱油无法完成。
*/
void printFuelResult(const string& name, double distance, const string& unit, double fuel, double tankCapacity) {
    cout << name << "行驶 " << distance << " " << unit << " 的油耗为：" << fuel << " L" << endl;
    if (fuel <= tankCapacity) {
        cout << "油耗未超过油箱容量 " << tankCapacity << " L，可以完成本次行驶。" << endl;
    } else {
        cout << "油耗超过油箱容量 " << tankCapacity << " L，单箱油无法完成本次行驶。" << endl;
    }
}

// ============================================================
// 第一部分：非虚函数版本
// ============================================================

class NonVirtualVehicle {
protected:
    string name;

public:
    /*
        函数功能：NonVirtualVehicle 构造函数。
        参数 vehicleName：交通工具名称，用于输出时区分对象。
    */
    explicit NonVirtualVehicle(const string& vehicleName) : name(vehicleName) {}

    /*
        这里故意不写 virtual。

        非虚函数是静态绑定。即使 NonVirtualVehicle* 实际指向 NonVirtualCar、
        NonVirtualShip 或 NonVirtualTruck 对象，编译器仍会根据指针声明类型
        NonVirtualVehicle* 决定调用 NonVirtualVehicle::calcFuelConsumption。
    */
    double calcFuelConsumption(double distance) const {
        (void)distance;
        cout << "[非虚函数] " << name
             << " 调用了 NonVirtualVehicle 的通用函数，无法得到具体油耗。" << endl;
        return 0.0;
    }
};

class NonVirtualCar : public NonVirtualVehicle {
protected:
    double tankCapacity;
    double fuelPer100Km;

public:
    /*
        函数功能：NonVirtualCar 构造函数。
        参数 carName：汽车名称。
        参数 tank：油箱容量。
        参数 fuel：百公里油耗。
    */
    NonVirtualCar(const string& carName, double tank, double fuel)
        : NonVirtualVehicle(carName), tankCapacity(tank), fuelPer100Km(fuel) {}

    /*
        函数功能：按照汽车百公里油耗公式计算油耗。
        参数 kilometers：汽车行驶公里数。
        返回值：本次行驶需要消耗的燃油升数。
    */
    double calcFuelConsumption(double kilometers) const {
        cout << "[非虚函数] " << name << " 调用了 NonVirtualCar 的汽车公式。" << endl;
        return kilometers / 100.0 * fuelPer100Km;
    }

    /*
        函数功能：获取汽车油箱容量。
        返回值：油箱容量，单位为升。
    */
    double getTankCapacity() const {
        return tankCapacity;
    }
};

class NonVirtualShip : public NonVirtualVehicle {
private:
    double tankCapacity;
    double fuelPerNauticalMile;

public:
    /*
        函数功能：NonVirtualShip 构造函数。
        参数 shipName：轮船名称。
        参数 tank：油箱容量。
        参数 fuel：每海里油耗。
    */
    NonVirtualShip(const string& shipName, double tank, double fuel)
        : NonVirtualVehicle(shipName), tankCapacity(tank), fuelPerNauticalMile(fuel) {}

    /*
        函数功能：按照轮船每海里油耗公式计算油耗。
        参数 nauticalMiles：轮船行驶海里数。
        返回值：本次行驶需要消耗的燃油升数。
    */
    double calcFuelConsumption(double nauticalMiles) const {
        cout << "[非虚函数] " << name << " 调用了 NonVirtualShip 的轮船公式。" << endl;
        return nauticalMiles * fuelPerNauticalMile;
    }

    /*
        函数功能：获取轮船油箱容量。
        返回值：油箱容量，单位为升。
    */
    double getTankCapacity() const {
        return tankCapacity;
    }
};

class NonVirtualTruck : public NonVirtualCar {
private:
    double loadLimit;

public:
    /*
        函数功能：NonVirtualTruck 构造函数。
        参数 truckName：卡车名称。
        参数 tank：油箱容量。
        参数 fuel：百公里油耗。
        参数 load：载重限制。
    */
    NonVirtualTruck(const string& truckName, double tank, double fuel, double load)
        : NonVirtualCar(truckName, tank, fuel), loadLimit(load) {}

    /*
        函数功能：按照卡车百公里油耗公式计算油耗。
        参数 kilometers：卡车行驶公里数。
        返回值：本次行驶需要消耗的燃油升数。
    */
    double calcFuelConsumption(double kilometers) const {
        (void)loadLimit;
        cout << "[非虚函数] " << name << " 调用了 NonVirtualTruck 的卡车公式。" << endl;
        return kilometers / 100.0 * fuelPer100Km;
    }

    /*
        函数功能：判断实际载重是否不超过卡车载重限制。
        参数 actualLoad：用户输入的实际载重。
        返回值：未超过限制返回 true，否则返回 false。
    */
    bool isLoadAllowed(double actualLoad) const {
        return actualLoad <= loadLimit;
    }

    /*
        函数功能：获取卡车载重限制。
        返回值：载重限制，单位为吨。
    */
    double getLoadLimit() const {
        return loadLimit;
    }
};

/*
    函数功能：演示非虚函数版本。
    重点现象：直接用对象调用时会执行各派生类函数；通过基类指针调用时，
    因为 calcFuelConsumption 不是 virtual，所以统一调用基类版本。
*/
void testNonVirtualFunction() {
    cout << "========== 一、非虚函数测试 ==========" << endl;

    TravelInput input = readTravelInput("非虚函数测试");

    NonVirtualCar car("汽车", 50, 7.5);
    NonVirtualShip ship("轮船", 5000, 30);
    NonVirtualTruck truck("卡车", 120, 18, 10);

    cout << "1. 直接用对象调用，能调用各自类里的函数：" << endl;
    printFuelResult("汽车", input.carKilometers, "公里",
                    car.calcFuelConsumption(input.carKilometers), car.getTankCapacity());
    printFuelResult("轮船", input.shipNauticalMiles, "海里",
                    ship.calcFuelConsumption(input.shipNauticalMiles), ship.getTankCapacity());
    if (!truck.isLoadAllowed(input.truckActualLoad)) {
        cout << "卡车实际载重 " << input.truckActualLoad << " 吨超过载重限制 "
             << truck.getLoadLimit() << " 吨，不能出车。" << endl;
    } else {
        printFuelResult("卡车", input.truckKilometers, "公里",
                        truck.calcFuelConsumption(input.truckKilometers), truck.getTankCapacity());
    }

    cout << endl << "2. 用基类指针调用，因为函数不是 virtual，所以都调用基类版本：" << endl;
    NonVirtualVehicle* vehicles[3] = {&car, &ship, &truck};
    double distances[3] = {input.carKilometers, input.shipNauticalMiles, input.truckKilometers};

    for (int i = 0; i < 3; ++i) {
        cout << "基类指针调用结果：" << vehicles[i]->calcFuelConsumption(distances[i]) << " L" << endl;
    }

    cout << "结论：非虚函数通过基类指针调用时发生静态绑定，看的是指针类型。" << endl;
}

// ============================================================
// 第二部分：虚函数版本
// ============================================================

class VirtualVehicle {
protected:
    string name;

public:
    /*
        函数功能：VirtualVehicle 构造函数。
        参数 vehicleName：交通工具名称。
    */
    explicit VirtualVehicle(const string& vehicleName) : name(vehicleName) {}

    /*
        多态基类建议写虚析构函数。

        如果以后写 Vehicle* p = new Car(...); delete p;
        虚析构函数能保证先执行 Car 析构，再执行 Vehicle 析构。
        只要一个类准备用作多态基类，析构函数通常也应声明为 virtual。
    */
    virtual ~VirtualVehicle() {}

    /*
        virtual 是本题关键字。
        它表示这个函数支持“运行时多态”：
        通过基类指针/引用调用时，会根据对象真实类型决定调用哪个函数。
    */
    virtual double calcFuelConsumption(double distance) const {
        (void)distance;
        cout << "[虚函数] " << name
             << " 调用了 VirtualVehicle 的通用函数。" << endl;
        return 0.0;
    }
};

class VirtualCar : public VirtualVehicle {
protected:
    double tankCapacity;
    double fuelPer100Km;

public:
    /*
        函数功能：VirtualCar 构造函数。
        参数 carName：汽车名称。
        参数 tank：油箱容量。
        参数 fuel：百公里油耗。
    */
    VirtualCar(const string& carName, double tank, double fuel)
        : VirtualVehicle(carName), tankCapacity(tank), fuelPer100Km(fuel) {}

    /*
        override 不是必须的，但强烈推荐。
        它能让编译器检查：当前函数确实重写了基类虚函数。
    */
    double calcFuelConsumption(double kilometers) const override {
        cout << "[虚函数] " << name << " 调用了 VirtualCar 的汽车公式。" << endl;
        return kilometers / 100.0 * fuelPer100Km;
    }

    /*
        函数功能：获取汽车油箱容量。
        返回值：油箱容量，单位为升。
    */
    double getTankCapacity() const {
        return tankCapacity;
    }
};

class VirtualShip : public VirtualVehicle {
private:
    double tankCapacity;
    double fuelPerNauticalMile;

public:
    /*
        函数功能：VirtualShip 构造函数。
        参数 shipName：轮船名称。
        参数 tank：油箱容量。
        参数 fuel：每海里油耗。
    */
    VirtualShip(const string& shipName, double tank, double fuel)
        : VirtualVehicle(shipName), tankCapacity(tank), fuelPerNauticalMile(fuel) {}

    /*
        函数功能：按照轮船每海里油耗公式计算油耗。
        参数 nauticalMiles：轮船行驶海里数。
        返回值：本次行驶需要消耗的燃油升数。
    */
    double calcFuelConsumption(double nauticalMiles) const override {
        cout << "[虚函数] " << name << " 调用了 VirtualShip 的轮船公式。" << endl;
        return nauticalMiles * fuelPerNauticalMile;
    }

    /*
        函数功能：获取轮船油箱容量。
        返回值：油箱容量，单位为升。
    */
    double getTankCapacity() const {
        return tankCapacity;
    }
};

class VirtualTruck : public VirtualCar {
private:
    double loadLimit;

public:
    /*
        函数功能：VirtualTruck 构造函数。
        参数 truckName：卡车名称。
        参数 tank：油箱容量。
        参数 fuel：百公里油耗。
        参数 load：载重限制。
    */
    VirtualTruck(const string& truckName, double tank, double fuel, double load)
        : VirtualCar(truckName, tank, fuel), loadLimit(load) {}

    /*
        函数功能：按照卡车百公里油耗公式计算油耗。
        参数 kilometers：卡车行驶公里数。
        返回值：本次行驶需要消耗的燃油升数。
    */
    double calcFuelConsumption(double kilometers) const override {
        (void)loadLimit;
        cout << "[虚函数] " << name << " 调用了 VirtualTruck 的卡车公式。" << endl;
        return kilometers / 100.0 * fuelPer100Km;
    }

    /*
        函数功能：判断实际载重是否不超过卡车载重限制。
        参数 actualLoad：用户输入的实际载重。
        返回值：未超过限制返回 true，否则返回 false。
    */
    bool isLoadAllowed(double actualLoad) const {
        return actualLoad <= loadLimit;
    }

    /*
        函数功能：获取卡车载重限制。
        返回值：载重限制，单位为吨。
    */
    double getLoadLimit() const {
        return loadLimit;
    }
};

/*
    函数功能：演示虚函数版本。
    重点现象：基类指针分别指向 Car、Ship、Truck 对象时，调用同名虚函数会
    根据对象真实类型执行不同公式，这就是运行时多态。
*/
void testVirtualFunction() {
    cout << endl << "========== 二、虚函数测试 ==========" << endl;

    TravelInput input = readTravelInput("虚函数测试");

    VirtualCar car("汽车", 50, 7.5);
    VirtualShip ship("轮船", 5000, 30);
    VirtualTruck truck("卡车", 120, 18, 10);

    cout << "同样使用基类指针数组，但因为 calcFuelConsumption 是 virtual：" << endl;
    VirtualVehicle* carPtr = &car;
    VirtualVehicle* shipPtr = &ship;
    VirtualVehicle* truckPtr = &truck;

    printFuelResult("汽车", input.carKilometers, "公里",
                    carPtr->calcFuelConsumption(input.carKilometers), car.getTankCapacity());
    printFuelResult("轮船", input.shipNauticalMiles, "海里",
                    shipPtr->calcFuelConsumption(input.shipNauticalMiles), ship.getTankCapacity());
    if (!truck.isLoadAllowed(input.truckActualLoad)) {
        cout << "卡车实际载重 " << input.truckActualLoad << " 吨超过载重限制 "
             << truck.getLoadLimit() << " 吨，不能出车。" << endl;
    } else {
        printFuelResult("卡车", input.truckKilometers, "公里",
                        truckPtr->calcFuelConsumption(input.truckKilometers), truck.getTankCapacity());
    }

    cout << "结论：虚函数通过基类指针调用时发生动态绑定，看的是对象真实类型。" << endl;
}

// ============================================================
// 第三部分：纯虚函数与抽象类版本
// ============================================================

class AbstractVehicle {
protected:
    string name;

public:
    /*
        函数功能：AbstractVehicle 构造函数。
        参数 vehicleName：交通工具名称。
    */
    explicit AbstractVehicle(const string& vehicleName) : name(vehicleName) {}

    /*
        函数功能：AbstractVehicle 虚析构函数。
        抽象类也可以作为多态基类使用，因此析构函数同样声明为 virtual。
    */
    virtual ~AbstractVehicle() {}

    /*
        = 0 表示纯虚函数。

        普通虚函数可以有默认实现；纯虚函数没有可直接使用的默认实现，
        它要求派生类必须给出具体实现。含有纯虚函数的类叫抽象类。
    */
    virtual double calcFuelConsumption(double distance) const = 0;
};

class AbstractCar : public AbstractVehicle {
protected:
    double tankCapacity;
    double fuelPer100Km;

public:
    /*
        函数功能：AbstractCar 构造函数。
        参数 carName：汽车名称。
        参数 tank：油箱容量。
        参数 fuel：百公里油耗。
    */
    AbstractCar(const string& carName, double tank, double fuel)
        : AbstractVehicle(carName), tankCapacity(tank), fuelPer100Km(fuel) {}

    /*
        函数功能：实现抽象基类规定的油耗计算接口。
        参数 kilometers：汽车行驶公里数。
        返回值：本次行驶需要消耗的燃油升数。
    */
    double calcFuelConsumption(double kilometers) const override {
        cout << "[纯虚函数] " << name << " 调用了 AbstractCar 的汽车公式。" << endl;
        return kilometers / 100.0 * fuelPer100Km;
    }

    /*
        函数功能：获取汽车油箱容量。
        返回值：油箱容量，单位为升。
    */
    double getTankCapacity() const {
        return tankCapacity;
    }
};

class AbstractShip : public AbstractVehicle {
private:
    double tankCapacity;
    double fuelPerNauticalMile;

public:
    /*
        函数功能：AbstractShip 构造函数。
        参数 shipName：轮船名称。
        参数 tank：油箱容量。
        参数 fuel：每海里油耗。
    */
    AbstractShip(const string& shipName, double tank, double fuel)
        : AbstractVehicle(shipName), tankCapacity(tank), fuelPerNauticalMile(fuel) {}

    /*
        函数功能：实现抽象基类规定的油耗计算接口。
        参数 nauticalMiles：轮船行驶海里数。
        返回值：本次行驶需要消耗的燃油升数。
    */
    double calcFuelConsumption(double nauticalMiles) const override {
        cout << "[纯虚函数] " << name << " 调用了 AbstractShip 的轮船公式。" << endl;
        return nauticalMiles * fuelPerNauticalMile;
    }

    /*
        函数功能：获取轮船油箱容量。
        返回值：油箱容量，单位为升。
    */
    double getTankCapacity() const {
        return tankCapacity;
    }
};

class AbstractTruck : public AbstractCar {
private:
    double loadLimit;

public:
    /*
        函数功能：AbstractTruck 构造函数。
        参数 truckName：卡车名称。
        参数 tank：油箱容量。
        参数 fuel：百公里油耗。
        参数 load：载重限制。
    */
    AbstractTruck(const string& truckName, double tank, double fuel, double load)
        : AbstractCar(truckName, tank, fuel), loadLimit(load) {}

    /*
        函数功能：实现抽象基类规定的油耗计算接口。
        参数 kilometers：卡车行驶公里数。
        返回值：本次行驶需要消耗的燃油升数。
    */
    double calcFuelConsumption(double kilometers) const override {
        (void)loadLimit;
        cout << "[纯虚函数] " << name << " 调用了 AbstractTruck 的卡车公式。" << endl;
        return kilometers / 100.0 * fuelPer100Km;
    }

    /*
        函数功能：判断实际载重是否不超过卡车载重限制。
        参数 actualLoad：用户输入的实际载重。
        返回值：未超过限制返回 true，否则返回 false。
    */
    bool isLoadAllowed(double actualLoad) const {
        return actualLoad <= loadLimit;
    }

    /*
        函数功能：获取卡车载重限制。
        返回值：载重限制，单位为吨。
    */
    double getLoadLimit() const {
        return loadLimit;
    }
};

/*
    函数功能：演示纯虚函数和抽象类版本。
    重点现象：AbstractVehicle 只定义统一接口，不能直接创建对象；
    具体油耗公式由 AbstractCar、AbstractShip、AbstractTruck 分别实现。
*/
void testAbstractClass() {
    cout << endl << "========== 三、抽象类测试 ==========" << endl;

    TravelInput input = readTravelInput("抽象类测试");

    /*
        下面这一行如果取消注释，会编译失败：
            AbstractVehicle vehicle("交通工具");

        原因：
            AbstractVehicle 有纯虚函数 calcFuelConsumption，
            所以它是抽象类，不能直接创建对象。
    */

    AbstractCar car("汽车", 50, 7.5);
    AbstractShip ship("轮船", 5000, 30);
    AbstractTruck truck("卡车", 120, 18, 10);

    cout << "抽象基类不能实例化，但可以定义基类指针统一管理派生类对象：" << endl;
    AbstractVehicle* carPtr = &car;
    AbstractVehicle* shipPtr = &ship;
    AbstractVehicle* truckPtr = &truck;

    printFuelResult("汽车", input.carKilometers, "公里",
                    carPtr->calcFuelConsumption(input.carKilometers), car.getTankCapacity());
    printFuelResult("轮船", input.shipNauticalMiles, "海里",
                    shipPtr->calcFuelConsumption(input.shipNauticalMiles), ship.getTankCapacity());
    if (!truck.isLoadAllowed(input.truckActualLoad)) {
        cout << "卡车实际载重 " << input.truckActualLoad << " 吨超过载重限制 "
             << truck.getLoadLimit() << " 吨，不能出车。" << endl;
    } else {
        printFuelResult("卡车", input.truckKilometers, "公里",
                        truckPtr->calcFuelConsumption(input.truckKilometers), truck.getTankCapacity());
    }

    cout << "结论：抽象类适合做统一接口，具体公式由 Car / Ship / Truck 实现。" << endl;
}

int main() {
    cout << "========== 实验 5.1：交通工具虚函数测试 ==========" << endl;
    testNonVirtualFunction();
    testVirtualFunction();
    testAbstractClass();
    return 0;
}
