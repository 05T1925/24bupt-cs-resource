package com.assignment.oop.animal;

/**
 * Q2 · Cat 子类
 *
 * 继承抽象类 Animal，实现具体的猫叫声（"喵喵"）。
 * 包含独立的 main 方法，用于单独测试 Cat 类。
 *
 * ──────────── 底层原理（C 语言视角） ────────────
 * 【C 语言对标】
 *   与 Dog 完全相同的模式：
 *   Cat_vtable.makeSound = &Cat_makeSound;  // vtable 的同一槽位，不同的函数指针
 *
 * 【vtable 槽位一致性——多态的基石】
 *   Animal 定义 makeSound → 分配 vtable 槽位 #3（假设）
 *   Dog.makeSound  → 在 Dog_vtable 的槽位 #3 填上 Dog 的实现
 *   Cat.makeSound  → 在 Cat_vtable 的槽位 #3 填上 Cat 的实现
 *   invokevirtual 时：不管实际对象是 Dog 还是 Cat，都查 vtable[3]
 *   → 同一槽位号，不同的函数指针 → 这就是"同一行代码产生不同行为"的底层原因
 *
 * @author JavaHomework
 * @version 1.0
 */
public class Cat extends Animal {

    /**
     * 构造器：创建一只猫
     *
     * @param name 猫的名字
     */
    public Cat(String name) {
        super(name);  // 调用父类 Animal 的构造器，传递姓名
                      // ★ 必须放在第一行——JVM 保证父类字段先于子类字段初始化
        System.out.println("[构造器] Cat(String name) 执行，Cat 对象创建完成: " + name);
    }

    /**
     * 实现父类的抽象方法：猫叫声
     *
     * 【底层】此方法的地址在类加载时被填入 Cat_Klass 的 vtable
     * 与 Dog.makeSound 占据同一个槽位（但指向不同的代码地址）
     * 类比 C: Cat_vtable.slots[3] = &Cat_makeSound; // 地址 0x401200（假设）
     *          Dog_vtable.slots[3] = &Dog_makeSound; // 地址 0x401100（假设）
     */
    @Override
    public void makeSound() {
        System.out.println("喵喵");
    }

    /**
     * 独立测试入口：单独创建若干个 Cat 对象进行测试
     */
    public static void main(String[] args) {
        System.out.println("========== Cat 独立测试 (Cat.main) ==========");
        System.out.println();

        // 创建多只猫对象，传入不同的姓名
        Cat cat1 = new Cat("咪咪");
        Cat cat2 = new Cat("小花");
        Cat cat3 = new Cat("雪球");

        // 模拟测试数组
        Cat[] cats = {cat1, cat2, cat3};

        System.out.println("--- 遍历测试 Cat 对象 ---");
        for (Cat cat : cats) {
            System.out.print("种类: " + cat.getSpecies() + ", 姓名: " + cat.getName() + " → 叫声: ");
            cat.makeSound();
        }

        System.out.println();
        System.out.println("Cat 独立测试完成。");
    }
}
