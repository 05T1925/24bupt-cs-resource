package com.assignment.interfaces;

/**
 * Q5 · AmphibianTest 测试类
 *
 * 测试内容：
 *   1. 创建 Amphibian 对象数组
 *   2. 遍历调用 walk() 和 swim()
 *   3. 展示一个对象可以同时作为 Walkable 和 Swimmable 使用
 *
 * ──────────────────────────────────────────────
 * 底层原理（C 语言视角）
 * ──────────────────────────────────────────────
 * 【多接口引用的指针语义】
 *   Walkable asWalkable = frog; → 向上转型为 Walkable 接口类型。
 *   编译器将 asWalkable 视为"实现了 Walkable 接口的某个对象"。
 *   asWalkable.walk() → invokeinterface Walkable.walk()V
 *   JVM 从 frog 对象的 Klass → itable 中查找 Walkable 条目 → 取出方法地址 → 间接跳转。
 *
 *   类比 C：同一个 void* frog 可以强转为 WalkableVTable* 或 SwimmableVTable*，
 *   只要这两个 vtable 结构体都指向正确的函数实现。
 *
 * @author JavaHomework
 * @version 1.0
 */
public class AmphibianTest {

    public static void main(String[] args) {
        System.out.println("========== Q5 多接口实现测试 ==========");
        System.out.println();

        Amphibian[] animals = new Amphibian[4];
        animals[0] = new Amphibian("青蛙");
        animals[1] = new Amphibian("蟾蜍");
        animals[2] = new Amphibian("蝾螈");
        animals[3] = new Amphibian("鲵鱼");

        System.out.println("创建了 " + animals.length + " 只两栖动物");
        System.out.println();

        // 调用接口方法 —— Amphibian 引用可直接调用所有已实现接口的方法
        System.out.println("--- 演示1：调用 walk() 和 swim() ---");
        for (Amphibian animal : animals) {
            System.out.print("[" + animal.getName() + "] ");
            animal.walk();
            System.out.print("[" + animal.getName() + "] ");
            animal.swim();
            System.out.println();
        }

        // 展示多接口身份 —— 同一对象可以被不同接口类型引用
        System.out.println("--- 演示2：一个对象，多种接口身份 ---");
        System.out.println();

        Amphibian frog = new Amphibian("青蛙");

        // 向上转型为 Walkable —— 编译器只"看到" walk()
        Walkable asWalkable = frog;
        System.out.print("作为 Walkable: ");
        asWalkable.walk();

        // 向上转型为 Swimmable —— 编译器只"看到" swim()
        Swimmable asSwimmable = frog;
        System.out.print("作为 Swimmable: ");
        asSwimmable.swim();

        // Amphibian 引用可以调用所有方法，包括接口外的 jump()
        System.out.print("作为 Amphibian: ");
        frog.walk();
        System.out.print("作为 Amphibian: ");
        frog.swim();
        System.out.print("作为 Amphibian: ");
        frog.jump();  // ← 只有 Amphibian 引用才能调用 jump()

        System.out.println();

        // instanceof 验证：对象身份检查通过 Klass Pointer 比对实现
        System.out.println("--- 演示3：instanceof 验证 ---");
        System.out.println("frog instanceof Walkable:   " + (frog instanceof Walkable));
        System.out.println("frog instanceof Swimmable:  " + (frog instanceof Swimmable));
        System.out.println("frog instanceof Amphibian:  " + (frog instanceof Amphibian));
        System.out.println("asWalkable instanceof Amphibian: " + (asWalkable instanceof Amphibian));
        System.out.println();

        System.out.println("========== Q5 结论 ==========");
        System.out.println("1. Java 中一个类可以实现多个接口（implements Walkable, Swimmable）。");
        System.out.println("2. 同一个对象可以以不同接口类型被引用（多态的高级形式）。");
        System.out.println("3. 接口使得不相关的类可以共享行为契约。");
        System.out.println("4. instanceof 可以检查对象是否实现了某个接口。");
    }
}
