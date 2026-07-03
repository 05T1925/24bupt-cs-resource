package com.assignment.interfaces;

/**
 * Q5 · Swimmable 接口
 *
 * 定义"可游泳"的行为规范。
 *
 * ──────────────────────────────────────────────
 * 底层原理（C 语言视角）
 * ──────────────────────────────────────────────
 * 【多接口 vs 单继承】
 *   Java 不允许 extends 多个类（避免 C++ 的菱形继承问题），
 *   但允许 implements 多个接口。因为接口不含实例字段，
 *   不会引起"字段从哪条继承路径来"的二义性。
 *
 *   类比 C：
 *     typedef struct { void (*swim)(void* this_); } SwimmableVTable;
 *     Amphibian 的 itable 中同时有 Walkable 条目和 Swimmable 条目。
 *
 * @author JavaHomework
 * @version 1.0
 */
public interface Swimmable {
    void swim();
}
