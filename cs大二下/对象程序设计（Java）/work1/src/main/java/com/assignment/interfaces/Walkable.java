package com.assignment.interfaces;

/**
 * Q5 · Walkable 接口
 *
 * 定义"可行走"的行为规范。
 * 接口中的方法默认是 public abstract 的。
 *
 * ──────────────────────────────────────────────
 * 底层原理（C 语言视角）
 * ──────────────────────────────────────────────
 * 【接口的本质】
 *   Java 的 interface 在编译后生成一个特殊的 class 文件（含 ACC_INTERFACE 标志）。
 *   在 JVM 底层，每个接口对应一个 Klass 对象，其中包含一张"接口方法表"。
 *
 *   类比 C 语言：
 *     // Walkable 接口 ≈ 一个只含函数指针的结构体
 *     typedef struct {
 *         void (*walk)(void* this_);
 *     } WalkableVTable;
 *
 *   实现 Walkable 的类，其 itable（接口方法表）中会注册 Walkable 条目，
 *   指向该类对 walk() 的具体实现。
 *
 * @author JavaHomework
 * @version 1.0
 */
public interface Walkable {
    // 编译后 class 文件访问标志：ACC_INTERFACE | ACC_ABSTRACT
    // 方法默认：ACC_PUBLIC | ACC_ABSTRACT
    void walk();
}
