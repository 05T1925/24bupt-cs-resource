package com.assignment.oop.animal;

/**
 * Q2 · Animal 抽象类
 *
 * 定义所有动物的共性：
 *   - 姓名属性（name）：每只动物有自己的名字
 *   - 抽象方法 makeSound()：由子类各自实现不同的叫声
 *
 * 设计意图：体现面向对象的"抽象"与"多态"——
 *   父类只定义规范（"动物会叫"），具体怎么叫由子类决定。
 *
 * ──────────── 底层原理（C 语言视角） ────────────
 * 【C 语言对标——抽象类 ≈ 包含纯虚函数指针的 struct】
 *
 *   // C 语言模拟 abstract class Animal：
 *   typedef struct {
 *       void (*makeSound)(void* this_);  // ★ 纯虚函数指针 → 对应 abstract method
 *       char* name;                       // 实例字段
 *   } AnimalVTable;
 *
 *   typedef struct {
 *       AnimalVTable* vtable;   // ★ 虚函数表指针（保存在每个实例的对象头中）
 *       char* name;             // 实例数据
 *   } Animal;
 *
 * 【abstract 关键字的底层含义】
 *   · abstract class 不能 new → 就像 C 中你不能 malloc 一个"不完整的 struct"
 *   · abstract method 强制子类"填充"vtable 中的对应槽位 → 编译器保证 vtable 无 NULL 函数指针
 *   · 如果没有 abstract 强制，就相当于 C 中 vtable 某个槽位没赋值 → 运行时 segfault
 *
 * 【堆内存布局（以 Dog 为例）】
 *   new Dog("旺财") 在堆上分配：
 *   ┌─── 对象头 ───┬── 实例数据 ──┐
 *   │ Mark Word(8B)│Klass Ptr(4B)│ name(ref,4B) │
 *   └──────────────┴─────────────┴──────────────┘
 *   Klass Pointer → Metaspace 中的 Dog_Klass → vtable[slot_makeSound] → Dog.makeSound代码段地址
 *
 * @author JavaHomework
 * @version 1.0
 */
public abstract class Animal {
    // abstract 关键字编译后：类访问标志中设置 ACC_ABSTRACT(0x0400)
    // JVM 在 new 指令时会检查此标志 → abstract 类拒绝实例化

    /** 动物姓名 —— 实例字段，存储在堆上对象的实例数据区 */
    private String name;

    /**
     * 构造器：创建动物时必须指定姓名
     *
     * 【底层】编译为 invokespecial Animal.<init> —— 精确调用（非虚调用）
     * 等价于 C: void Animal_ctor(Animal* this_, char* name) { this_->name = name; }
     *
     * @param name 动物姓名
     */
    public Animal(String name) {
        // this.name = name —— 底层：mov [this + 12], name_ref （固定偏移量赋值）
        this.name = name;
        System.out.println("[构造器] Animal(String name) 执行，初始化姓名: " + name);
    }

    public String getName() { return name; }
    public void setName(String name) { this.name = name; }

    /**
     * 抽象方法：发出叫声
     * 子类必须重写此方法，提供各自的叫声实现
     *
     * 【底层】在 class 文件的方法表中标记为 abstract（无 Code 属性）
     * JVM 保证：任何非抽象子类必须在 vtable 中为此方法提供非 NULL 的函数指针
     * 等价于 C: vtable->makeSound = NULL; // ← 必须由子类"构造器"填充
     */
    public abstract void makeSound();

    /**
     * 获取动物种类名称
     * 使用反射获取运行时的实际类名
     *
     * 【底层】getClass() 返回对象头中 Klass Pointer 指向的 java.lang.Class 对象
     * 等价于 C: return this_->klass->class_name;
     *
     * @return 类名（如 "Dog", "Cat", "Cow"）
     */
    public String getSpecies() {
        return this.getClass().getSimpleName();
    }
}
