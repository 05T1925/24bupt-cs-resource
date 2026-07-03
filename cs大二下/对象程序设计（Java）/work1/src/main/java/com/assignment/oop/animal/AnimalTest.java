package com.assignment.oop.animal;

/**
 * Q2 · Animal 统一测试类
 *
 * 测试内容：
 *   1. 创建 Animal 类型的数组，存储不同子类对象（Dog, Cat, Cow）
 *   2. 遍历数组，调用 makeSound() —— 展示多态：
 *      同一个父类引用，指向不同子类对象时，调用同一方法产生不同行为
 *   3. 输出每只动物的种类和姓名，展示构造器调用情况
 *
 * 多态的核心本质：方法的调用取决于对象的"运行时类型"而非"编译时类型"
 *   编译时：Animal animal = new Dog("旺财");  → 编译器认为它是 Animal
 *   运行时：animal.makeSound()  → JVM 发现它实际是 Dog，调用 Dog 的 makeSound()
 *
 * ──────────── 底层原理（C 语言视角） ────────────
 * 【多态的汇编级真相】
 *
 *   Animal[] animals = new Animal[6];
 *   animals[0] = new Dog("旺财");  // 自动向上转型——Dog* 赋值给 Animal*
 *   // 等价于 C: Animal* a = (Animal*)dog; —— 但 Java 的转型是安全的（Klass Pointer 可追溯）
 *
 *   关键指令序列（C2 JIT 编译后的概念级产物）：
 *   for (Animal animal : animals) {
 *       animal.makeSound();
 *       // 汇编：
 *       //   mov  rcx, [rdi + 8]          ; 1. 读 Klass Pointer（对象头 offset 8）
 *       //   mov  rax, [rcx + vtable+24]  ; 2. 读 vtable[3]（Dog/Cat/Cow 各自不同）
 *       //   call rax                     ; 3. ★ 间接跳转 —— 这是唯一的差异点
 *       //                                   Dog: rax=0x401100, Cat: rax=0x401200
 *   }
 *
 * 【为什么这比 C 的 qsort 回调更快？】
 *   - qsort 每次比较都要 call [rbx]（函数指针在内存中 → 间接跳转）
 *   - Java invokevirtual 在热路径上被 C2 "去虚拟化"：
 *     如果 JIT 观察到 99% 的调用都是 Dog，则：
 *       cmp rcx, Dog_Klass_ptr    ; 快速检查：是不是 Dog？
 *       jne slow_path             ; 不是 → 走 vtable 查表
 *       call Dog_makeSound        ; ★ 是 → 直接跳转！（零间接开销）
 *   - 这就是 PGO（Profile-Guided Optimization） + JIT 的威力
 *
 * @author JavaHomework
 * @version 1.0
 */
public class AnimalTest {

    public static void main(String[] args) {
        System.out.println("========== Q2 Animal 多态体系统一测试 ==========");
        System.out.println();

        // ==================== 步骤1：创建 Animal 数组 ====================
        // 关键：数组声明类型是 Animal（父类），但实际存储的是不同的子类对象
        //
        // 【底层】Animal[] 是引用类型数组 —— 每个元素是一个指向 Animal 子类对象的引用（指针）
        // 等价于 C: Animal** animals = malloc(6 * sizeof(Animal*));
        // 存入子类对象时自动"向上转型"（upcasting）—— 子类指针赋值给父类指针变量
        Animal[] animals = new Animal[6];
        animals[0] = new Dog("旺财");   // Dog* → Animal*（安全，编译器自动完成）
        animals[1] = new Dog("大黄");
        animals[2] = new Cat("咪咪");   // Cat* → Animal*（同样安全）
        animals[3] = new Cat("小花");
        animals[4] = new Cow("花花");   // Cow* → Animal*
        animals[5] = new Cow("大壮");

        // ==================== 步骤2：多态遍历 ====================
        System.out.println("--- 多态遍历：Animal 引用调用 makeSound() ---");
        System.out.println("（同一行代码 animal.makeSound()，产生三种不同的输出）");
        System.out.println();

        for (Animal animal : animals) {
            // animal 的编译时类型是 Animal，运行时类型是 Dog/Cat/Cow
            // makeSound() 的实际调用版本由运行时类型决定 → 这就是多态
            //
            // 【汇编本质】这个 animal.makeSound() 在循环中每次执行时：
            //   - animal 引用（栈变量）每次都指向不同的堆对象
            //   - 不同堆对象 → 不同的 Klass Pointer → 不同的 vtable
            //   - 相同的槽位号（makeSound = vtable[3]）→ 不同的目标地址
            //   - call [rax + 24] 这种"间接跳转"就是多态的 CPU 级实现
            System.out.print("种类: " + animal.getSpecies()
                           + ", 姓名: " + animal.getName()
                           + " → 叫声: ");
            animal.makeSound();
        }

        // ==================== 步骤3：解释实际构造器日志 ====================
        System.out.println();
        System.out.println("--- 构造器调用链结论 ---");
        System.out.println("上方创建每个对象时已经由构造器实时打印调用日志。");
        System.out.println("每组日志均先出现 Animal 构造器，再出现 Dog/Cat/Cow 构造器，");
        System.out.println("证明创建子类对象时会先完成父类部分初始化，再执行子类构造器。");

        // ==================== 步骤4：多态验证 ====================
        System.out.println();
        System.out.println("--- 多态验证 ---");
        System.out.println("测试：是否所有对象都能通过 Animal 引用正确访问？");
        boolean allOk = true;
        for (Animal animal : animals) {
            if (animal.getName() == null || animal.getSpecies() == null) {
                allOk = false;
            }
        }
        System.out.println("所有动物的姓名和种类均可通过父类引用访问: " + (allOk ? "✅ 通过" : "❌ 失败"));
        System.out.println();

        // ==================== 总结 ====================
        System.out.println("========== 多态核心结论 ==========");
        System.out.println("1. 父类引用可以指向任意子类对象（向上转型，自动发生）。");
        System.out.println("2. 通过父类引用调用方法时，实际执行的是子类重写的版本。");
        System.out.println("3. 这种「同一接口，不同实现」的机制就是多态。");
        System.out.println("4. 新增动物种类（如 Sheep）时，无需修改 AnimalTest 的遍历逻辑。");
        System.out.println("   这是多态带来的最大好处——对扩展开放，对修改关闭（开闭原则）。");
        System.out.println();
        System.out.println("========== AnimalTest 完成，各子类独立测试入口见下方 ==========");
        System.out.println();

        // ==================== 步骤5：调用各子类的 main 进行独立测试 ====================
        System.out.println(">>> 依次调用 Dog、Cat、Cow 的独立 main 方法：");

        Dog.main(args);
        System.out.println();

        Cat.main(args);
        System.out.println();

        Cow.main(args);
    }
}
