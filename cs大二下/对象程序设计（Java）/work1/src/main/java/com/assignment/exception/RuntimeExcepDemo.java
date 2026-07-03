package com.assignment.exception;

/*
 * ╔══════════════════════════════════════════════════════════════════════════════╗
 * ║                                                                              ║
 * ║   Q8 · 核心理论问题：                                                         ║
 * ║   "如果没有被 Java 强制要求处理异常的代码块，是否就不会出现异常了？为什么？"      ║
 * ║                                                                              ║
 * ╚══════════════════════════════════════════════════════════════════════════════╝
 *
 * ============================================================================
 * 一、直接回答
 * ============================================================================
 *
 *   不会！即使 Java 编译器没有强制要求处理异常，代码仍然可能在运行时抛出异常。
 *   原因在于 Java 的异常体系分为两大类：
 *
 *   ┌─────────────────────────────────────────────────────────────────────────┐
 *   │                          Java 异常体系                                   │
 *   │                                                                         │
 *   │   Throwable                                                            │
 *   │   ├── Error (严重错误，不应试图处理)                                      │
 *   │   │   ├── OutOfMemoryError / StackOverflowError / NoClassDefFoundError  │
 *   │   │                                                                     │
 *   │   └── Exception                                                         │
 *   │       ├── Checked Exception (编译时异常) ← ★ 编译器强制要求处理          │
 *   │       │   ├── IOException / SQLException / FileNotFoundException        │
 *   │       │   特点：不 try-catch 或 throws → 编译不通过                      │
 *   │       │                                                                 │
 *   │       └── RuntimeException (运行时异常/非受检异常) ← ★ 编译器不强制      │
 *   │           ├── NullPointerException      空指针（类似 C 的 SIGSEGV）      │
 *   │           ├── ArrayIndexOutOfBoundsException 越界（类似 C 的 越界访问）  │
 *   │           ├── ArithmeticException       除零（类似 CPU 的 #DE 陷阱）     │
 *   │           ├── ClassCastException        非法转型                         │
 *   │           └── NumberFormatException     解析失败                         │
 *   │           特点：编译时不检查，运行到问题代码时抛出                          │
 *   └─────────────────────────────────────────────────────────────────────────┘
 *
 * ============================================================================
 * 二、Checked vs Unchecked 的本质区别
 * ============================================================================
 *
 *   Checked Exception（受检异常）：
 *     - 代表"外部环境问题"（文件不存在、网络中断、DB 宕机）
 *     - 不是程序员的 bug，是运行时环境变化导致的
 *     - 编译器强制处理 = 强制你思考"外部条件不满足时该怎么办"
 *     - C 语言类比：fopen 返回 NULL → 你必须检查 errno → 这是"强制"的（不检查后果自负）
 *
 *   Unchecked Exception（非受检异常）：
 *     - 代表"代码逻辑错误"（数组越界、空指针、除零）
 *     - 是程序员的 bug，应通过修正代码逻辑来消除
 *     - 编译器不强制 → 假设程序员"知道自己在做什么"
 *     - C 语言类比：arr[10]=42 越界写入 → 编译器不阻止 → 运行时 segfault/脏数据
 *
 * ============================================================================
 * 三、从硬件陷阱到 Java 异常对象的完整路径
 * ============================================================================
 *
 *   NullPointerException 的硬件→软件链路：
 *     [CPU] mov eax, [0x0 + 12]  ; 解引用 null 地址 + 12 偏移
 *           → MMU 查页表：虚拟地址 0xC 所在页的 Present bit = 0
 *           → 触发 #PF (Page Fault, 中断向量 14)
 *     [OS]  Linux 内核 #PF handler → 判断地址在零页保护区 → 发送 SIGSEGV
 *     [JVM] HotSpot 的 SIGSEGV 信号处理器：
 *           → 检查故障地址 < 64KB（零页区域）→ 判断为 NPE
 *           → 创建 NullPointerException 对象 + 填充堆栈跟踪
 *           → 沿当前线程调用栈的异常表派发
 *           → 找到匹配的 catch → 跳转；未找到 → 线程终止
 *
 *   ArithmeticException 的硬件→软件链路：
 *     [CPU] idiv ecx (ecx=0) → 触发 #DE (Divide Error, 中断向量 0)
 *     [OS]  Linux #DE handler → 发送 SIGFPE
 *     [JVM] HotSpot SIGFPE 处理器 → 创建 ArithmeticException("/ by zero")
 *           注意：浮点数 100.0/0.0 不会抛异常（IEEE 754: Infinity）
 *
 * ============================================================================
 * 四、为什么不强制处理所有异常？
 * ============================================================================
 *
 *   如果每个数组访问、每个对象方法调用、每次除法都要求 try-catch：
 *     for (int i = 0; i < arr.length; i++) {
 *         try { arr[i] = i * 2; }
 *         catch (ArrayIndexOutOfBoundsException e) { ... }
 *     }
 *   代码将变得不可读且不可维护。
 *
 *   Java 的设计取舍：
 *     - Checked Exception   → "你必须预见并处理这个问题"
 *     - Unchecked Exception → "你不应该写出会触发这个异常的代码"
 *     关键认知：「不强制处理」≠「不会发生」≠「可以不考虑」
 *
 * ============================================================================
 * 五、结论
 * ============================================================================
 *
 *   问：如果没有被 Java 强制要求处理异常的代码块，是否就不会出现异常了？
 *   答：否！Unchecked Exception（RuntimeException 子类）仍然会在运行时抛出。
 *       编译器不强制处理 ≠ 运行时不会触发。
 *       不强制处理的代码块同样可能出现 NullPointerException、数组越界等异常。
 *       "编译器不报错" ≠ "运行时没问题" ≠ "不会出异常"
 *
 * @author JavaHomework
 * @version 1.0
 */
public class RuntimeExcepDemo {

    public static void main(String[] args) {
        System.out.println("========== Q8 运行时异常（Unchecked Exception）演示 ==========");
        System.out.println();
        System.out.println("以下 4 个演示，编译器都不会报错（不强制处理），");
        System.out.println("但运行时都会抛出 RuntimeException 的某个子类。");
        System.out.println();
        System.out.println("这证明了：「不强制处理 ≠ 不会抛出异常」");
        System.out.println();

        // ==================== 演示1：NullPointerException ====================
        System.out.println("========== 演示1：NullPointerException（空指针异常） ==========");
        System.out.println();
        System.out.println("代码：String s = null; s.length();");
        System.out.println("编译：✅ 通过（编译器不强制处理）");
        System.out.println("运行：");
        System.out.println();

        try {
            String s = null;
            // 运行时：s 是 null → JVM 尝试通过 null 引用访问对象头中的 Klass Pointer
            // → 解引用地址 0x0 + offset → MMU 触发 Page Fault → OS 发送 SIGSEGV
            // → HotSpot 信号处理器检测到零页访问 → 创建 NullPointerException
            int len = s.length();
            System.out.println("字符串长度: " + len);
        } catch (NullPointerException e) {
            System.out.println("  ❌ 抛出异常: " + e.getClass().getSimpleName());
            System.out.println("     异常信息: " + e.getMessage());
            System.out.println("     说明: 对 null 引用调用实例方法，触发 NullPointerException");
        }
        System.out.println();

        // ==================== 演示2：ArrayIndexOutOfBoundsException ====================
        System.out.println("========== 演示2：ArrayIndexOutOfBoundsException（数组越界） ==========");
        System.out.println();
        System.out.println("代码：int[] arr = {1,2,3}; arr[5] = 99;");
        System.out.println("编译：✅ 通过（编译器不强制处理）");
        System.out.println("运行：");
        System.out.println();

        try {
            int[] arr = {1, 2, 3};
            // 运行时：JVM 在每次数组访问前执行边界检查（隐式）
            // 5 < 0? 否。5 >= arr.length(=3)? 是 → 抛出 ArrayIndexOutOfBoundsException
            // C 对比：arr[5]=99 直接计算 *(arr+5*4) 写入 → 无检查 → 脏数据/segfault
            arr[5] = 99;
            System.out.println("arr[5] = " + arr[5]);
        } catch (ArrayIndexOutOfBoundsException e) {
            System.out.println("  ❌ 抛出异常: " + e.getClass().getSimpleName());
            System.out.println("     异常信息: " + e.getMessage());
            System.out.println("     说明: 访问数组时索引超出 [0, length-1] 范围");
        }
        System.out.println();

        // ==================== 演示3：ArithmeticException ====================
        System.out.println("========== 演示3：ArithmeticException（除零异常） ==========");
        System.out.println();
        System.out.println("代码：int result = 100 / 0;");
        System.out.println("编译：✅ 通过（编译器不强制处理）");
        System.out.println("运行：");
        System.out.println();

        try {
            // 运行时：CPU 执行 idiv 指令 → 除数为 0
            // → CPU 触发 #DE (Divide Error, 中断向量 0)
            // → OS 发送 SIGFPE → HotSpot 创建 ArithmeticException
            // IEEE 754 浮点数除零：100.0/0.0 = +Infinity（不抛异常）
            int result = 100 / 0;
            System.out.println("100 / 0 = " + result);
        } catch (ArithmeticException e) {
            System.out.println("  ❌ 抛出异常: " + e.getClass().getSimpleName());
            System.out.println("     异常信息: " + e.getMessage());
            System.out.println("     说明: 整数除法中除数为零，触发 ArithmeticException");
            System.out.println("     扩展：浮点数 100.0/0.0 = Infinity（不抛异常，IEEE 754 标准）");
        }
        System.out.println();

        // ==================== 演示4：NumberFormatException ====================
        System.out.println("========== 演示4：NumberFormatException（数字格式异常） ==========");
        System.out.println();
        System.out.println("代码：int num = Integer.parseInt(\"abc\");");
        System.out.println("编译：✅ 通过（编译器不强制处理）");
        System.out.println("运行：");
        System.out.println();

        try {
            // 运行时：parseInt 内部逐个字符解析 → 发现 'a' 不是数字字符
            // → 抛出 NumberFormatException（纯 Java 逻辑，不涉及硬件陷阱）
            int num = Integer.parseInt("abc");
            System.out.println("解析结果: " + num);
        } catch (NumberFormatException e) {
            System.out.println("  ❌ 抛出异常: " + e.getClass().getSimpleName());
            System.out.println("     异常信息: " + e.getMessage());
            System.out.println("     说明: 字符串无法解析为目标数值类型时触发");
        }
        System.out.println();

        // ==================== 附加演示：Checked Exception ====================
        System.out.println("========== 附加演示：Checked Exception 对比 ==========");
        System.out.println();
        System.out.println("以下代码如果取消注释，编译器会直接报错（必须 try-catch 或 throws）：");
        System.out.println();
        System.out.println("// FileReader fr = new FileReader(\"不存在的文件.txt\");");
        System.out.println("// ↑ 编译错误：未报告的异常错误 java.io.FileNotFoundException");
        System.out.println();
        System.out.println("这就是 Checked Exception —— 编译器强制你处理。");
        System.out.println("而上面 4 个演示的 Unchecked Exception —— 编译器不强制，但运行时一样会抛。");
        System.out.println();

        // ==================== 总结 ====================
        System.out.println("========== Q8 总结 ==========");
        System.out.println();
        System.out.println("┌──────────────────────────────────────────────────────────────┐");
        System.out.println("│ Q: 如果没有被 Java 强制要求处理异常的代码块，                 │");
        System.out.println("│    是否就不会出现异常了？                                    │");
        System.out.println("│                                                              │");
        System.out.println("│ A: 否！                                                     │");
        System.out.println("│    Java 编译器不强制处理 RuntimeException 及其子类，         │");
        System.out.println("│    但这不代表这些异常不会出现。                              │");
        System.out.println("│    它们被称为 Unchecked Exception（非受检异常），            │");
        System.out.println("│    触发条件客观存在（如数组越界、空指针、除零等），          │");
        System.out.println("│    一旦条件满足，运行时照样抛出。                            │");
        System.out.println("│                                                              │");
        System.out.println("│    关键认知：「不强制处理」≠「不会发生」                     │");
        System.out.println("│    类比 C 语言：编译器不强制检查指针越界，                   │");
        System.out.println("│    但越界操作依然会导致 segfault 或脏数据。                  │");
        System.out.println("└──────────────────────────────────────────────────────────────┘");
        System.out.println();
        System.out.println("========== Q8 演示完成 ==========");
    }
}
