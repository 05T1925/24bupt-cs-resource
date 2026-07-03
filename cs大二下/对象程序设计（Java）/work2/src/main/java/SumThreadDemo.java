import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * 小作业二第 2 题：使用 10 个线程分段累加 1 到 100。
 *
 * 任务划分：
 *   Thread-1 计算 1~10，Thread-2 计算 11~20，依此类推，
 *   Thread-10 计算 91~100。十个区间互不重叠且完整覆盖 1~100。
 *
 * 多线程同步：
 * 每个线程先计算自己的局部和，再通过 AtomicInteger.addAndGet() 将局部和
 * 原子地汇总到共享 sum。主线程使用 join() 等待所有线程完成后才输出最终结果。
 *
 * 如果直接让多个线程执行普通 int 类型的 sum += localSum，这个操作实际包含
 * “读取旧值、计算新值、写回新值”三个步骤。线程交错执行时可能相互覆盖结果，
 * 产生竞态条件。因此这里使用 AtomicInteger 保证每次汇总不可被其它线程打断。
 *
 *
 *   10 个线程的输出顺序可能不同，这是线程调度的正常现象；
 *   每个区间只计算一次，最终总和必须稳定为 5050。
 *
 * 整体思路：
 *   先分段并行计算局部和，再原子汇总；主线程通过 join 等待全部完成。
 */
public class SumThreadDemo {
    public static void main(String[] args) {
        /*
         * sum 是十个任务共同持有的共享对象。
         * AtomicInteger 的原子方法可以安全地处理多个线程同时更新的情况。
         */
        AtomicInteger sum = new AtomicInteger(0);

        // 保存线程对象，后面主线程需要逐个调用 join() 等待它们结束。
        List<Thread> threads = new ArrayList<>();

        for (int i = 1; i <= 10; i++) {
            /*
             * 根据线程编号计算区间：
             * i=1 时 start=1、end=10；i=10 时 start=91、end=100。
             */
            int start = (i - 1) * 10 + 1;
            int end = i * 10;

            /*
             * 同一个 sum 引用传给所有 SumTask，所以各任务最终汇总到同一个总和。
             * Thread 名称主要用于观察和区分控制台输出。
             */
            Thread thread = new Thread(new SumTask(i, start, end, sum), "Thread-" + i);
            threads.add(thread);

            /*
             * start() 会创建新的线程执行 run()。
             * 如果在这里直接调用 thread.run()，任务只会在 main 线程中顺序执行，
             * 不能体现多线程并发。
             */
            thread.start();
        }

        /*
         * join() 使 main 线程等待指定子线程结束。
         * 不能用 sleep() 代替，因为无法准确预测每个子线程需要多长时间；
         * 如果提前输出，共享 sum 可能尚未包含全部十个区间。
         */
        for (Thread thread : threads) {
            try {
                thread.join();
            } catch (InterruptedException e) {
                // 恢复中断标志，使上层代码仍能知道当前线程曾收到中断请求。
                System.out.println("主线程等待子线程时被中断。");
                Thread.currentThread().interrupt();
                return;
            }
        }

        // 1 到 100 的正确总和为 5050，可用来核对多线程汇总结果。
        System.out.println("10个线程累加完毕，最终 sum 结果为: " + sum.get());
    }

    /**
     * 每个 SumTask 是一个可由 Thread 执行的任务。
     *
     * Runnable 把“要执行的任务”和 Thread 线程对象分开，便于把线程编号、
     * 计算区间及共享总和作为任务状态传入。
     */
    static class SumTask implements Runnable {
        // 以下字段在构造后不再改变，final 可以明确任务参数是只读的。
        private final int threadNumber;
        private final int start;
        private final int end;
        private final AtomicInteger sum;

        SumTask(int threadNumber, int start, int end, AtomicInteger sum) {
            this.threadNumber = threadNumber;
            this.start = start;
            this.end = end;
            this.sum = sum;
        }

        @Override
        public void run() {
            /*
             * localSum 是 run() 内的局部变量，每个线程都有自己的独立副本，
             * 不会发生线程间竞争，也不需要加锁。
             */
            int localSum = 0;

            for (int i = start; i <= end; i++) {
                localSum += i;
            }

            /*
             * 每个线程只汇总一次，比每加一个数字就修改共享 sum 更简单，
             * 也减少了对共享变量的原子操作次数。
             */
            sum.addAndGet(localSum);

            /*
             * 各线程由 JVM 调度，完成顺序不固定，所以这些输出可能每次顺序不同；
             * 这不影响最终结果的正确性。
             */
            System.out.println("Thread-" + threadNumber + " 完成 " + start + " 到 " + end
                    + " 的计算，局部和为: " + localSum);
        }
    }
}
