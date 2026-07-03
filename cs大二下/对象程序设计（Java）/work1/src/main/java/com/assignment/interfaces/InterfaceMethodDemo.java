package com.assignment.interfaces;

/**
 * Q6 · 接口变量调用接口外方法的演示
 *
 * 【核心问题】在一个接口变量中，能否调用该实现类未定义在接口中的方法？
 * 【答案】不能！必须向下转型为具体类型。
 *
 * ──────────────────────────────────────────────
 * 底层原理（C 语言视角）
 * ──────────────────────────────────────────────
 * 【接口变量调用的编译时限制】
 *   编译器按变量的"声明类型"检查方法调用合法性，而非"运行时实际类型"。
 *   Walkable w = new Amphibian("青蛙");
 *     - 编译器认为 w 是 Walkable 类型
 *     - Walkable 的 itable 中只定义了 walk() 方法
 *     - w.swim() 在编译时即被拒绝（Walkable 的 itable 中没有 swim 条目）
 *
 *   类比 C 语言：
 *     WalkableVTable* w = (WalkableVTable*)amphibian;
 *     w->walk(amphibian);  // ✅ 可以 —— WalkableVTable 中有 walk 函数指针
 *     w->swim(amphibian);  // ❌ 编译错误 —— WalkableVTable 中没有 swim 函数指针
 *     // C 中你需要自己强转：(SwimmableVTable*)w，但这不安全
 *
 * 【instanceof + 向下转型的底层安全机制】
 *   与 C 的盲目指针强转不同，Java 的 (Amphibian) w 编译为 checkcast 指令：
 *     1. 从 w 指向对象的对象头中读取 Klass Pointer
 *     2. 比对 Klass Pointer 是否 == Amphibian_Klass，或沿 _super 链查找
 *     3. 不匹配 → 抛出 ClassCastException（而非 segfault）
 *
 *   类比 C：
 *     // C 的盲目强转：0 条 CPU 指令，编译器完全信任你
 *     Amphibian* a = (Amphibian*)w;  // 如果 w 实际不是 Amphibian → 未定义行为
 *
 *     // Java 的 checkcast：至少 1 次 Klass Pointer 比对（cmp 指令）
 *     // 加 1 次条件跳转（jne throw_ClassCastException）
 *     // 这多出来的几条指令换来了运行时的类型安全
 *
 * @author JavaHomework
 * @version 1.0
 */
public class InterfaceMethodDemo {

    public static void main(String[] args) {
        System.out.println("========== Q6 接口变量调用接口外方法演示 ==========");
        System.out.println();

        // ==================== 场景1：接口变量只能调用接口内方法 ====================
        System.out.println("--- 场景1：Walkable 接口变量 ---");
        System.out.println();

        // 声明为 Walkable 接口类型，实际指向 Amphibian 对象
        // 编译时类型 = Walkable → 只能调用 walk()
        Walkable w = new Amphibian("青蛙");
        System.out.println("声明: Walkable w = new Amphibian(\"青蛙\");");
        System.out.println();

        // walk() 在 Walkable 中定义 → 可通过接口变量调用
        System.out.print("w.walk()  → ✅ ");
        w.walk();

        // swim() 不在 Walkable 中定义 → 编译错误
        // 即使运行时 w 实际指向的 Amphibian 有 swim()，编译器也拒绝
        System.out.println("w.swim()  → ❌ 编译错误！");
        System.out.println("  原因: Walkable 接口中没有定义 swim() 方法");
        System.out.println("  编译器按 Walkable 类型检查 → 找不到 swim() → 拒绝编译");
        System.out.println();

        System.out.println("w.jump()  → ❌ 编译错误！");
        System.out.println("  原因: Walkable 接口中没有定义 jump() 方法");
        System.out.println();

        // ==================== 场景2：Swimmable 接口变量同样受限 ====================
        System.out.println("--- 场景2：Swimmable 接口变量 ---");
        System.out.println();

        Swimmable s = new Amphibian("青蛙");
        System.out.println("声明: Swimmable s = new Amphibian(\"青蛙\");");
        System.out.println();

        System.out.print("s.swim()  → ✅ ");
        s.swim();

        System.out.println("s.walk()  → ❌ 编译错误！");
        System.out.println("  原因: Swimmable 接口中没有定义 walk() 方法");
        System.out.println();

        // ==================== 场景3：解决方案 —— instanceof + 向下转型 ====================
        System.out.println("========== 解决方案：向下转型 ==========");
        System.out.println();
        System.out.println("如果确实需要调用接口外的方法，有两种安全的做法：");
        System.out.println();

        // 方案A：instanceof 检查 + 强制转型（推荐）
        // 底层：instanceof 比对 Klass Pointer → 确认类型 → checkcast 再次比对 → 安全转型
        System.out.println("--- 方案A：instanceof + 强制转型（推荐） ---");
        System.out.println();

        Walkable w2 = new Amphibian("蟾蜍");
        System.out.println("Walkable w2 = new Amphibian(\"蟾蜍\");");
        System.out.println();

        if (w2 instanceof Amphibian) {
            // instanceof 通过比对对象头中的 Klass Pointer 确认类型
            // 确认后转型绝对安全（JVM 仍然会执行 checkcast，但一定通过）
            System.out.println("w2 instanceof Amphibian → true，安全转型");
            Amphibian a = (Amphibian) w2;
            System.out.print("a.walk() → ");
            a.walk();
            System.out.print("a.swim() → ");
            a.swim();
            System.out.print("a.jump() → ");
            a.jump();
        }
        System.out.println();

        // 方案B：直接强转 —— 简洁但有风险
        // 编译为 checkcast Amphibian —— 如果 w3 实际不是 Amphibian → ClassCastException
        System.out.println("--- 方案B：直接强转（需确保类型正确，否则 ClassCastException） ---");
        System.out.println();

        Walkable w3 = new Amphibian("蝾螈");
        System.out.println("((Amphibian) w3).jump() → ");
        System.out.print("  ");
        ((Amphibian) w3).jump();
        System.out.println();
        System.out.println("警告：如果 w3 实际不是 Amphibian，((Amphibian) w3) 会抛出 ClassCastException");
        System.out.println("      checkcast 指令比对 Klass Pointer 失败 → 抛出异常（而非 C 的 segfault）");
        System.out.println();

        // ==================== 场景4：错误演示 ====================
        System.out.println("--- 场景4：错误强转的后果（伪代码） ---");
        System.out.println("// Walkable wrong = new SomeOtherClass();");
        System.out.println("// ((Amphibian) wrong).jump();");
        System.out.println("// → 运行时 checkcast 发现 Klass Pointer 不匹配 → ClassCastException");
        System.out.println("// C 对比：(Amphibian*)wrong 不会报错，直接解引用脏数据 → segfault/UB");
        System.out.println();

        System.out.println("========== Q6 结论 ==========");
        System.out.println("1. 接口变量只能调用接口中声明的方法，不能调用实现类的其他方法。");
        System.out.println("2. 原因：Java 是静态类型语言，编译器按声明类型检查方法调用。");
        System.out.println("3. 解决方案：使用 instanceof 判断后向下转型，即可调用任何方法。");
        System.out.println("4. checkcast 指令通过对象头 Klass Pointer 比对保证转型安全。");
    }
}
