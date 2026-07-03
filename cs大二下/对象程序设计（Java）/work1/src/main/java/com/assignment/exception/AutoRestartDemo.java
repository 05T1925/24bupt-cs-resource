package com.assignment.exception;

/**
 * Q7 · 异常处理自动重启演示
 *
 * 设计思路：
 *   使用 while 循环包裹 try-catch，当 doDangerousWork() 抛出异常时，
 *   捕获异常但不退出程序，而是打印错误信息后自动重新执行。
 *
 * ──────────────────────────────────────────────
 * 底层原理（C 语言视角）
 * ──────────────────────────────────────────────
 * 【try-catch 的控制流跃迁机制】
 *
 *   Java 的 try-catch 在概念上等价于 C 语言的 setjmp/longjmp，但更安全：
 *
 *   C 语言 setjmp/longjmp：
 *     jmp_buf env;
 *     if (setjmp(env) == 0) {           // 保存寄存器快照（%rbp, %rsp, %rip）
 *         doDangerousWork();            // 正常执行
 *     } else {                          // longjmp 跳回这里
 *         printf("错误，重启中...\n");   // 错误恢复
 *     }
 *     问题：longjmp 不会展开栈帧 → 局部 malloc 的内存泄漏
 *           不会调用析构函数 → 资源泄漏（文件句柄、锁等）
 *
 *   Java 的 try-catch：
 *     每个方法附有一张"异常表"（Exception Table），记录了：
 *       from_pc, to_pc, target_pc, exception_type
 *     当异常发生时，JVM 沿调用栈逐帧展开：
 *       1. 在当前方法的异常表中查找匹配条目
 *       2. 找到 → 跳转到 target_pc（catch 块首条指令）
 *       3. 未找到 → 弹出当前栈帧 → 回到调用者的异常表继续查找
 *       4. 到达栈底仍未捕获 → 线程终止 + 打印堆栈跟踪
 *
 *   栈帧展开过程中：JVM 自动清理局部变量引用（GC 安全），释放 synchronized 锁。
 *   这比 longjmp 跳过栈帧而不清理要安全得多。
 *
 * 【while + try-catch 的重启模式】
 *   异常发生 → catch 捕获 → 不退出循环 → 下次迭代重新执行 try 块。
 *   本质上相当于在栈帧展开后，控制流回到了 while 循环的开头。
 *
 * @author JavaHomework
 * @version 1.0
 */
public class AutoRestartDemo {

    private static final int MAX_RETRIES = 5;
    private static final int SIMULATED_FAILURES = 2;
    private static int attemptCount = 0;

    public static void main(String[] args) {
        System.out.println("========== Q7 异常自动重启演示 ==========");
        System.out.println();
        System.out.println("最大重试次数: " + MAX_RETRIES);
        System.out.println();

        runWithAutoRestart();

        System.out.println();
        System.out.println("========== Q7 演示完成 ==========");
    }

    /**
     * 核心方法：运行任务，出错自动重启。
     *
     * 控制流结构：
     *   while (retryCount < MAX_RETRIES) {
     *       try {
     *           doDangerousWork();   // 正常执行
     *           return;              // 成功 → 退出
     *       } catch (Exception e) {
     *           // 异常发生时：JVM 沿着调用栈展开到此处
     *           // doDangerousWork 的栈帧已弹出
     *           // 局部变量已清理
     *           retryCount++;        // 记录重试
     *           // continue 循环 → 重新进入 try 块
     *       }
     *   }
     */
    public static void runWithAutoRestart() {
        int retryCount = 0;
        attemptCount = 0;

        System.out.println("--- 开始执行任务（模拟可能失败的操作） ---");
        System.out.println();

        while (retryCount < MAX_RETRIES) {
            try {
                // try 块的字节码范围被记录在异常表中
                // 如果 doDangerousWork() 抛出异常 →
                //   JVM 在当前异常表中查找匹配的 catch 条目 → 跳转到 catch 块
                System.out.print("[尝试 " + (retryCount + 1) + "/" + MAX_RETRIES + "] ");
                doDangerousWork();

                // 执行成功 → 不触发异常 → 跳过 catch → 退出循环
                System.out.println("✅ 任务成功完成！");
                System.out.println();
                System.out.println("总尝试次数: " + (retryCount + 1));
                return;

            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                System.err.println("任务收到中断信号，停止自动重启。");
                return;
            } catch (Exception e) {
                // ★ 控制流跃迁到这里 —— 等价于 setjmp/longjmp 的"着陆点"
                // 但比 longjmp 安全：栈帧已正确展开，finally 块已执行，锁已释放
                retryCount++;
                System.err.println("❌ 发生异常: " + e.getMessage());

                if (retryCount < MAX_RETRIES) {
                    System.err.println("   [系统] 正在重启...（第 " + retryCount + " 次重试）");
                    try {
                        Thread.sleep(200);
                    } catch (InterruptedException ie) {
                        Thread.currentThread().interrupt();
                        System.err.println("重启等待期间收到中断信号，停止自动重启。");
                        return;
                    }
                } else {
                    System.err.println("   [系统] 已达到最大重试次数（" + MAX_RETRIES + "），任务失败。");
                }
                System.err.println();
            }
        }

        System.out.println("程序终止：无法在 " + MAX_RETRIES + " 次重试内完成任务。");
    }

    /**
     * 模拟一个可能失败的危险操作。
     * 为了使自动重试流程具有确定性，前两次固定失败，第三次固定成功。
     */
    private static void doDangerousWork() throws Exception {
        attemptCount++;

        if (attemptCount <= SIMULATED_FAILURES) {
            throw new RuntimeException(
                    "模拟故障：第 " + attemptCount + " 次执行失败");
        }

        System.out.print("操作执行中... ");
        Thread.sleep(100);
    }
}
