import java.util.Scanner;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;

/**
 * 小作业二第 3 题：三个仓库、两个 fetch 线程、一个 save 线程。
 *
 * 核心设计：
 *   1. WarehouseManager 使用同一把对象锁保护三个仓库的整体库存状态。
 *   2. save 选择库存最少的仓库；fetch 选择库存最多的仓库。
 *   3. 条件不满足时在 while 循环中 wait()，成功操作后 notifyAll()。
 *   4. 主线程只读取和分发命令，实际存取操作由三个 Worker 线程完成。
 *   5. STOP_SIGNAL 位于已提交任务之后，使线程正常处理完队列再退出。
 *
 * 运行行为：
 *   库存不足时 fetch 会进入 wait；后续存货改变库存后，
 *   fetch 会被 notifyAll() 唤醒并在条件满足时完成取货。
 *
 * 运行示例：
 *   javac WarehouseDemo.java
 *   java WarehouseDemo
 *
 * 输入示例：
 *   3 5   表示 save 线程存入 5 个单位
 *   1 3   表示 fetch1 线程取出 3 个单位
 *   2 2   表示 fetch2 线程取出 2 个单位
 *
 * 结束输入：
 *   输入 q 后回车，或直接发送 EOF。
 */
public class WarehouseDemo {
    // 输入命令第一列使用 1、2、3 标识两个取货线程和一个存货线程。
    private static final int FETCH1 = 1;
    private static final int FETCH2 = 2;
    private static final int SAVE = 3;
    private static final long SHUTDOWN_TIMEOUT_MS = 5000;

    public static void main(String[] args) {
        // 三个 Worker 共享同一个 manager，因此访问的是同一组三个仓库库存。
        WarehouseManager manager = new WarehouseManager();

        Worker fetch1Worker = new Worker("fetch1", FETCH1, manager);
        Worker fetch2Worker = new Worker("fetch2", FETCH2, manager);
        Worker saveWorker = new Worker("save", SAVE, manager);

        Thread fetch1Thread = new Thread(fetch1Worker, "fetch1");
        Thread fetch2Thread = new Thread(fetch2Worker, "fetch2");
        Thread saveThread = new Thread(saveWorker, "save");

        fetch1Thread.start();
        fetch2Thread.start();
        saveThread.start();

        System.out.println("请输入操作指令：1 n 表示 fetch1 取货，2 n 表示 fetch2 取货，3 n 表示 save 存货。");
        System.out.println("n 建议为 1 到 5。输入 q 结束程序。");

        Scanner scanner = new Scanner(System.in);

        try {
            while (true) {
                System.out.print("> ");

                if (!scanner.hasNextLine()) {
                    break;
                }

                String line = scanner.nextLine().trim();
                if (line.length() == 0) {
                    continue;
                }

                if ("q".equalsIgnoreCase(line) || "quit".equalsIgnoreCase(line)) {
                    break;
                }

                // \\s+ 接受一个或多个空白字符，例如“1 3”或“1    3”。
                String[] parts = line.split("\\s+");
                if (parts.length != 2) {
                    System.out.println("输入格式错误，本条指令已忽略。正确格式示例：1 3");
                    continue;
                }

                int workerId;
                int amount;
                try {
                    workerId = Integer.parseInt(parts[0]);
                    amount = Integer.parseInt(parts[1]);
                } catch (NumberFormatException e) {
                    System.out.println("输入中包含非数字内容，本条指令已忽略。正确格式示例：1 3");
                    continue;
                }

                if (amount < 1 || amount > 5) {
                    System.out.println("存取数量应为 1 到 5，本条指令已忽略。");
                    continue;
                }

                // 主线程只分发指令，库存修改由对应的工作线程完成。
                switch (workerId) {
                    case FETCH1:
                        fetch1Worker.submit(amount);
                        break;
                    case FETCH2:
                        fetch2Worker.submit(amount);
                        break;
                    case SAVE:
                        saveWorker.submit(amount);
                        break;
                    default:
                        System.out.println("线程编号只能是 1、2 或 3，本条指令已忽略。");
                        break;
                }
            }
        } finally {
            scanner.close();

            // 结束标记排在已提交任务之后，正常情况下先处理完队列再退出。
            fetch1Worker.shutdown();
            fetch2Worker.shutdown();
            saveWorker.shutdown();

            Thread[] workerThreads = {fetch1Thread, fetch2Thread, saveThread};
            if (!waitForThreadsToFinish(workerThreads, SHUTDOWN_TIMEOUT_MS)) {
                System.out.println("仍有操作因库存条件无法完成，取消剩余任务并结束程序。");
                for (Thread thread : workerThreads) {
                    thread.interrupt();
                }
                waitForThreadsToFinish(workerThreads, SHUTDOWN_TIMEOUT_MS);
            }
        }

        System.out.println("主线程结束。");
    }

    /**
     * 主线程退出前等待工作线程结束，保证线程生命周期收尾清楚。
     */
    private static boolean waitForThreadsToFinish(Thread[] threads, long timeoutMillis) {
        // 使用统一截止时间，避免依次等待三个线程时把总超时时间放大三倍。
        long deadline = System.nanoTime() + timeoutMillis * 1_000_000L;

        for (Thread thread : threads) {
            long remainingNanos = deadline - System.nanoTime();
            if (remainingNanos <= 0) {
                return false;
            }

            try {
                long remainingMillis = Math.max(1, remainingNanos / 1_000_000L);
                thread.join(remainingMillis);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                return false;
            }

            if (thread.isAlive()) {
                return false;
            }
        }

        return true;
    }

    /**
     * 仓库管理器：统一管理三个仓库库存。
     *
     * 为什么绝对不能连续写 warehouse1.wait(); warehouse2.wait(); warehouse3.wait();？
     *
     * 1. wait() 必须在持有对应对象监视器锁的前提下调用。
     *    也就是说，只有进入 synchronized (warehouse1) 代码块后，才允许调用
     *    warehouse1.wait()。如果没有持有该对象的锁，直接调用 wait() 会抛出
     *    IllegalMonitorStateException。
     *
     * 2. 即使分别持有三个仓库对象的锁，连续等待多个对象也非常危险。
     *    线程一旦调用某个对象的 wait()，只会释放“这个对象”的监视器锁，
     *    不会自动释放它可能持有的其它对象锁。这样很容易出现锁顺序混乱、
     *    线程互相等待、甚至死锁的问题。
     *
     * 3. 本题的判断条件不是某一个仓库的独立状态，而是“三个仓库整体状态”。
     *    save 要寻找库存最少的仓库，fetch 要寻找库存最多的仓库，这都需要同时
     *    观察 stocks[0]、stocks[1]、stocks[2]。因此，正确做法是使用一个统一锁
     *    保护整个库存数组，把“是否可以存/取”作为同一个共享条件来判断。
     *
     * 4. 当任意一次存货或取货成功后，三个仓库的整体状态都可能改变。
     *    因此应调用 notifyAll() 唤醒所有等待线程，让它们重新在 while 循环中
     *    检查条件。不能使用 notify() 随机唤醒一个线程，否则可能唤醒的线程仍
     *    不满足条件，而真正满足条件的线程继续沉睡。
     */
    private static class WarehouseManager {
        private static final int WAREHOUSE_COUNT = 3;
        private static final int CAPACITY = 20;
        // stocks[0]、stocks[1]、stocks[2] 分别表示仓库 1、2、3 的当前库存。
        private final int[] stocks = new int[WAREHOUSE_COUNT];

        /**
         * save 线程存货：选择当前库存最少的仓库。
         */
        /*
         * synchronized 实例方法使用当前 WarehouseManager 对象作为监视器锁。
         * save、fetch 以及其中调用的库存查询因此不会同时修改同一个 stocks 数组。
         */
        public synchronized void save(String workerName, int amount) throws InterruptedException {
            // wait 可能被虚假唤醒，因此每次醒来都要重新选择最少库存仓库并检查容量。
            while (true) {
                int index = findMinStockIndex();

                if (stocks[index] + amount <= CAPACITY) {
                    stocks[index] += amount;
                    printState(workerName + " 存入 " + amount + " 个单位到仓库 "
                            + (index + 1));
                    // 状态变化后唤醒所有等待者，由它们各自在 while 中重新判断条件。
                    notifyAll();
                    return;
                }

                System.out.println(workerName + " 想存入 " + amount
                        + " 个单位，但库存最少的仓库 " + (index + 1)
                        + " 也会溢出，进入 wait 状态。当前库存: " + stockText());
                // wait() 会释放 manager 的锁；被唤醒并重新获得锁后才会继续执行。
                wait();
            }
        }

        /**
         * fetch 线程取货：选择当前库存最多的仓库。
         */
        public synchronized void fetch(String workerName, int amount) throws InterruptedException {
            // 库存变化后最大值位置可能改变，所以被唤醒后必须重新查找。
            while (true) {
                int index = findMaxStockIndex();

                if (stocks[index] >= amount) {
                    stocks[index] -= amount;
                    printState(workerName + " 从仓库 " + (index + 1)
                            + " 取出 " + amount + " 个单位");
                    notifyAll();
                    return;
                }

                System.out.println(workerName + " 想取出 " + amount
                        + " 个单位，但库存最多的仓库 " + (index + 1)
                        + " 也不够，进入 wait 状态。当前库存: " + stockText());
                wait();
            }
        }

        private int findMinStockIndex() {
            // 库存相等时保留较小下标，符合题目中“任选一个”的要求。
            int minIndex = 0;

            for (int i = 1; i < stocks.length; i++) {
                if (stocks[i] < stocks[minIndex]) {
                    minIndex = i;
                }
            }

            return minIndex;
        }

        private int findMaxStockIndex() {
            // 所有访问都发生在 WarehouseManager 的同一把监视器锁内。
            int maxIndex = 0;

            for (int i = 1; i < stocks.length; i++) {
                if (stocks[i] > stocks[maxIndex]) {
                    maxIndex = i;
                }
            }

            return maxIndex;
        }

        private void printState(String operation) {
            System.out.println(operation + "，操作成功。当前库存: " + stockText());
        }

        private String stockText() {
            return "仓库1=" + stocks[0] + ", 仓库2=" + stocks[1] + ", 仓库3=" + stocks[2];
        }
    }

    /**
     * 工作线程。主线程读取输入后，把数量提交到对应 Worker 的队列中。
     */
    private static class Worker implements Runnable {
        private static final int STOP_SIGNAL = Integer.MIN_VALUE;

        private final String workerName;
        private final int workerId;
        private final WarehouseManager manager;
        /*
         * 每个 Worker 有自己的阻塞队列。主线程负责放入命令，工作线程通过 take()
         * 按提交顺序取出命令；队列为空时 take() 自动等待，不需要忙循环。
         */
        private final BlockingQueue<Integer> tasks = new LinkedBlockingQueue<>();

        Worker(String workerName, int workerId, WarehouseManager manager) {
            this.workerName = workerName;
            this.workerId = workerId;
            this.manager = manager;
        }

        void submit(int amount) {
            // LinkedBlockingQueue 默认容量足够，offer 在本题中可立即放入任务。
            tasks.offer(amount);
        }

        void shutdown() {
            // BlockingQueue 保证结束标记排在此前提交的任务后面。
            tasks.offer(STOP_SIGNAL);
        }

        @Override
        public void run() {
            while (!Thread.currentThread().isInterrupted()) {
                try {
                    // 没有任务时阻塞，有任务时按 FIFO 顺序取出。
                    int amount = tasks.take();
                    if (amount == STOP_SIGNAL) {
                        break;
                    }

                    if (workerId == SAVE) {
                        manager.save(workerName, amount);
                    } else {
                        manager.fetch(workerName, amount);
                    }
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                    break;
                }
            }

            System.out.println(workerName + " 线程结束。");
        }
    }
}
