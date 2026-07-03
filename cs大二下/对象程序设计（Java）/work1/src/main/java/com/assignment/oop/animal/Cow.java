package com.assignment.oop.animal;

/**
 * Q2 · Cow 子类
 *
 * 继承抽象类 Animal，实现具体的牛叫声（"哞哞"）。
 * 包含独立的 main 方法，用于单独测试 Cow 类。
 *
 * ──────────── 底层原理（C 语言视角） ────────────
 * 【C 语言对标】
 *   与 Dog、Cat 完全相同的模式——三者的差异仅在于：
 *   vtable 同一槽位填入不同的函数指针 → 导致相同的 invokevirtual 指令跳转到不同的代码地址
 *
 * 【"独立 main"的底层意义】
 *   每个子类的 main 是独立的 static 方法 —— 等价于三个不同的 C 函数
 *   它们之间没有调用关系，各自独立测试各自的类
 *   这验证了：子类可以完全独立使用，无需通过父类引用
 *
 * @author JavaHomework
 * @version 1.0
 */
public class Cow extends Animal {

    /**
     * 构造器：创建一头牛
     *
     * @param name 牛的名字
     */
    public Cow(String name) {
        super(name);  // 调用父类 Animal 的构造器，传递姓名
        System.out.println("[构造器] Cow(String name) 执行，Cow 对象创建完成: " + name);
    }

    /**
     * 实现父类的抽象方法：牛叫声
     */
    @Override
    public void makeSound() {
        System.out.println("哞哞");
    }

    /**
     * 独立测试入口：单独创建若干个 Cow 对象进行测试
     */
    public static void main(String[] args) {
        System.out.println("========== Cow 独立测试 (Cow.main) ==========");
        System.out.println();

        // 创建多头牛对象，传入不同的姓名
        Cow cow1 = new Cow("花花");
        Cow cow2 = new Cow("大壮");
        Cow cow3 = new Cow("哞哞");

        // 模拟测试数组
        Cow[] cows = {cow1, cow2, cow3};

        System.out.println("--- 遍历测试 Cow 对象 ---");
        for (Cow cow : cows) {
            System.out.print("种类: " + cow.getSpecies() + ", 姓名: " + cow.getName() + " → 叫声: ");
            cow.makeSound();
        }

        System.out.println();
        System.out.println("Cow 独立测试完成。");
    }
}
