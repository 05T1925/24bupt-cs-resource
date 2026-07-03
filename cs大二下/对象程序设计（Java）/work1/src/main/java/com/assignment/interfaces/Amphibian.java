package com.assignment.interfaces;

/**
 * Q5 · Amphibian 两栖动物类
 *
 * 同时实现 Walkable 和 Swimmable 两个接口，
 * 额外包含一个 jump() 方法（不在任何接口中定义），
 * 为 Q6 的"接口方法越权调用"问题提供演示基础。
 *
 * ──────────────────────────────────────────────
 * 底层原理（C 语言视角）
 * ──────────────────────────────────────────────
 * 【itable 的物理布局】
 *   Amphibian 的 Klass 对象中，itable 包含两个条目：
 *     itable[0]: { interface_klass = Walkable_Klass, methods = [Amphibian.walk] }
 *     itable[1]: { interface_klass = Swimmable_Klass, methods = [Amphibian.swim] }
 *
 *   jump() 不在任何 itable 中，也不在继承的 vtable 中。
 *   只有通过 Amphibian 引用（或向下转型后）才能调用。
 *
 * 【invokeinterface vs invokevirtual】
 *   w.walk() → invokeinterface Walkable.walk()V
 *   需要在 itable 中线性搜索 Walkable 条目 → 比 invokevirtual 的固定槽位索引多一次间接寻址。
 *   但 C2 JIT 通过单态内联缓存可将热路径优化为直接跳转。
 *
 * @author JavaHomework
 * @version 1.0
 */
public class Amphibian implements Walkable, Swimmable {

    private String name;

    public Amphibian(String name) { this.name = name; }
    public String getName() { return name; }

    /**
     * 实现 Walkable 接口：行走。
     * 编译为 invokeinterface Walkable.walk()V → 运行时通过 itable 寻址。
     */
    @Override
    public void walk() {
        System.out.println(name + " 在陆地上缓慢爬行...");
    }

    /**
     * 实现 Swimmable 接口：游泳。
     */
    @Override
    public void swim() {
        System.out.println(name + " 在水中灵活游动...");
    }

    /**
     * 接口外的方法：跳跃。
     * 此方法不在 Walkable 也不在 Swimmable 中定义，
     * 通过接口变量无法调用，必须向下转型为 Amphibian。
     */
    public void jump() {
        System.out.println(name + " 猛地跳了起来！");
    }
}
