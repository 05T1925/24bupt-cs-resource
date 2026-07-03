package com.assignment.oop.animal;

/**
 * Q2 · Dog 子类
 *
 * 继承抽象类 Animal，实现具体的狗叫声（"汪汪"）。
 * 包含独立的 main 方法，用于单独测试 Dog 类。
 *
 * ──────────── 底层原理（C 语言视角） ────────────
 * 【C 语言对标——子类 = 扩展父类 struct + 填充 vtable 函数指针】
 *
 *   // C 模拟 Dog "类"：
 *   void Dog_makeSound(void* this_) {
 *       printf("汪汪\n");
 *   }
 *
 *   AnimalVTable Dog_vtable = {
 *       .makeSound = &Dog_makeSound,  // ★ 填充父类定义的虚函数槽位
 *   };
 *
 *   DogInstance* new_Dog(char* name) {
 *       DogInstance* dog = malloc(sizeof(DogInstance));
 *       dog->vtable = &Dog_vtable;    // ★ 设置虚函数表 → invokevirtual 时通过此表寻址
 *       dog->name = name;
 *       return dog;
 *   }
 *
 * 【@Override 的底层含义】
 *   编译器检查：父类/接口中是否真的有同名同签名的方法 → 防止手滑写错
 *   相当于 C 中：确保你填充 vtable 槽位时，函数签名与 vtable 声明的函数指针类型匹配
 *
 * @author JavaHomework
 * @version 1.0
 */
public class Dog extends Animal {
    // extends 编译后：class 文件中记录 super_class 指向 Animal
    // JVM 类加载时：Dog_Klass._super = Animal_Klass

    /**
     * 构造器：创建一只狗
     *
     * 【底层执行流】
     *   1. JVM 在堆上分配 Dog 对象内存（对象头 + 父类字段 + 子类字段 = ~16 字节）
     *   2. invokespecial Animal.<init> → 执行父类构造器，设置 name 字段
     *   3. 返回 Dog.<init> 继续执行（这里无额外逻辑）
     *   等价于 C: new_Dog(name) { super_ctor(dog, name); return dog; }
     *
     * @param name 狗的名字
     */
    public Dog(String name) {
        super(name);  // ★ 必须是第一条语句！JVM invokespecial 指令强制此顺序
                      // 等价于 C: Animal_ctor((Animal*)this_, name);
                      // 保证父类字段先于子类字段初始化（类似内核初始化在用户态之前）
        System.out.println("[构造器] Dog(String name) 执行，Dog 对象创建完成: " + name);
    }

    /**
     * 实现父类的抽象方法：狗叫声
     *
     * 【底层——多态调用的关键】
     *   当你写 Animal a = new Dog("旺财"); a.makeSound(); 时：
     *   1. JVM 执行 invokevirtual Animal.makeSound()V
     *   2. 从 a 的对象头读取 Klass Pointer → Dog_Klass
     *   3. 查 Dog_Klass.vtable[makeSound_slot] → 得到 Dog_makeSound 的机器码首地址
     *   4. call rax  ★ 间接跳转 —— 这就是多态的全部底层真相
     *   等价于 C: a->vtable->makeSound(a);
     */
    @Override
    public void makeSound() {
        System.out.println("汪汪");
    }

    /**
     * 独立测试入口：单独创建若干个 Dog 对象进行测试
     *
     * 【static main 的独特性】
     *   static 方法属于类而非实例 → 调用时不需要 this
     *   等价于 C 中一个不接受 Animal* this_ 的普通函数
     */
    public static void main(String[] args) {
        System.out.println("========== Dog 独立测试 (Dog.main) ==========");
        System.out.println();

        // new Dog("旺财") —— 每次 new 在堆上分配独立的对象
        // 等价于 C: DogInstance* dog1 = new_Dog("旺财");
        // 三个对象的 vtable 指针都指向同一个 Dog_vtable（在 Metaspace 中全局唯一）
        Dog dog1 = new Dog("旺财");
        Dog dog2 = new Dog("大黄");
        Dog dog3 = new Dog("阿福");

        // 模拟测试数组
        Dog[] dogs = {dog1, dog2, dog3};

        System.out.println("--- 遍历测试 Dog 对象 ---");
        for (Dog dog : dogs) {
            System.out.print("种类: " + dog.getSpecies() + ", 姓名: " + dog.getName() + " → 叫声: ");
            // dog.makeSound() 编译为 invokevirtual Animal.makeSound()V
            // 运行时通过 vtable 间接跳转到 Dog.makeSound 的代码
            dog.makeSound();
        }

        System.out.println();
        System.out.println("Dog 独立测试完成。");
    }
}
