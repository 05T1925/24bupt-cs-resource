import java.time.LocalTime;
import java.time.format.DateTimeFormatter;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Watchdog 看门狗线程监控程序。
 *
 * 核心知识点：
 *   1. 使用 AtomicInteger 保证 watchdog 在线程之间的可见性和原子读写。
 *   2. Thread 对象只能启动一次，重启 A 线程时必须创建新的 Thread 对象。
 *   3. 使用运行标志和 interrupt() 安全结束线程，不使用危险的 Thread.stop()。
 *
 *
 *   控制台应依次出现正常心跳、连续 10 次无心跳、报警、停止第一代 A 线程、
 *   启动第二代 A 线程，以及第二代线程恢复稳定心跳。
 *
 * 运行示例：
 *   javac WatchdogDemo.java
 *   java WatchdogDemo
 *
 * 运行流程：
 *   1. A线程正常工作，并每 3 秒发送一次心跳，将 watchdog 设置为 1。
 *   2. B线程每 2 秒检查一次 watchdog。
 *   3. 第一代 A线程发送 5 次心跳后模拟故障，不再发送心跳。
 *   4. B线程连续 10 次检查不到心跳后报警，并安全停止原 A线程。
 *   5. B线程重新创建并启动第二代 A线程，确认心跳恢复后结束程序。
 */
public class WatchdogDemo {
    // A 线程每 3 秒发送一次心跳，B 线程每 2 秒检查一次，检查频率高于发送频率。
    private static final int HEARTBEAT_INTERVAL_MS = 3000;
    private static final int CHECK_INTERVAL_MS = 2000;
    // 连续 10 次没有观察到心跳才报警，避免一次调度延迟就误判故障。
    private static final int MAX_MISSING_COUNT = 10;
    private static final int FIRST_WORKER_FAIL_AFTER_HEARTBEATS = 5;
    private static final int RECOVERY_HEARTBEATS_BEFORE_EXIT = 3;

    // AtomicInteger 保证 A 线程写入的心跳能被 B 线程及时看到，并支持原子读写。
    private static final AtomicInteger watchdog = new AtomicInteger(0);
    private static final DateTimeFormatter TIME_FORMATTER =
            DateTimeFormatter.ofPattern("HH:mm:ss");

    public static void main(String[] args) {
        log("主线程: 启动 Watchdog 演示程序。");

        // B 线程负责管理 A 线程的创建、监控、停止和重启。
        MonitorTask monitorTask = new MonitorTask();
        Thread monitorThread = new Thread(monitorTask, "B-Monitor-Thread");
        monitorThread.start();

        try {
            monitorThread.join();
        } catch (InterruptedException e) {
            log("主线程: 等待 B 线程时被中断，准备退出。");
            Thread.currentThread().interrupt();
        }

        log("主线程: Watchdog 演示程序结束。");
    }

    /**
     * B线程任务：定时检查 watchdog，发现 A线程长期无心跳后执行重启。
     */
    private static class MonitorTask implements Runnable {
        // 同时保存任务对象和线程对象：前者用于修改运行标志，后者用于 interrupt/join。
        private WorkerTask currentWorkerTask;
        private Thread currentWorkerThread;
        private int workerGeneration = 0;
        private int missingCount = 0;
        private int recoveredHeartbeatCount = 0;

        @Override
        public void run() {
            startNewWorker(true);

            while (!Thread.currentThread().isInterrupted()) {
                if (!sleepSafely(CHECK_INTERVAL_MS)) {
                    break;
                }

                /*
                 * getAndSet(0) 相当于一次“取走心跳”：
                 * 读取到 1 表示本周期收到心跳，同时清零供下一检查周期使用。
                 * 读取和清零作为一个原子操作完成，避免两个步骤之间被其它线程插入。
                 */
                int oldValue = watchdog.getAndSet(0);

                if (oldValue == 1) {
                    missingCount = 0;
                    log("B线程: 监测到心跳，重置 watchdog 为 0。");

                    if (workerGeneration >= 2) {
                        recoveredHeartbeatCount++;
                        if (recoveredHeartbeatCount >= RECOVERY_HEARTBEATS_BEFORE_EXIT) {
                            log("B线程: 重启后的 A线程已稳定恢复心跳，准备结束演示。");
                            stopCurrentWorker();
                            return;
                        }
                    }
                } else {
                    missingCount++;
                    log("B线程: 未检测到心跳，连续丢包次数: " + missingCount);

                    if (missingCount >= MAX_MISSING_COUNT) {
                        log("!!! B线程报警: A线程发生异常，准备执行重启步骤 !!!");
                        restartWorker();
                    }
                }
            }

            stopCurrentWorker();
        }

        /**
         * 启动新的 A线程。
         *
         * @param simulateFailure 是否让本代 A线程发送若干次心跳后模拟故障
         */
        private void startNewWorker(boolean simulateFailure) {
            workerGeneration++;
            currentWorkerTask = new WorkerTask(workerGeneration, simulateFailure);
            // Thread 对象结束后不能再次 start，重启时必须创建新的任务和线程对象。
            currentWorkerThread = new Thread(currentWorkerTask, "A-Worker-Thread-" + workerGeneration);
            currentWorkerThread.start();
            log("B线程: 已启动第 " + workerGeneration + " 代 A线程。");
        }

        private void restartWorker() {
            // 先结束旧线程，再清空旧心跳并创建全新的线程对象。
            stopCurrentWorker();
            watchdog.set(0);
            missingCount = 0;
            recoveredHeartbeatCount = 0;

            // 第二代 A线程用于验证重启后的恢复过程，因此不再主动模拟故障。
            startNewWorker(false);
        }

        /**
         * 安全停止当前 A线程：请求退出、发送中断，并等待线程生命周期结束。
         * Thread.stop() 可能在线程修改共享状态时强制终止，所以这里不使用该方法。
         */
        private void stopCurrentWorker() {
            if (currentWorkerTask == null || currentWorkerThread == null) {
                return;
            }

            log("B线程: 正在安全停止原 A线程，禁止使用 Thread.stop()。");
            // running=false 用于正常循环检查，interrupt() 用于立即唤醒正在 sleep 的线程。
            currentWorkerTask.requestStop();
            currentWorkerThread.interrupt();

            try {
                currentWorkerThread.join(5000);
                if (currentWorkerThread.isAlive()) {
                    log("B线程: 原 A线程仍未退出，但程序不会调用危险的 Thread.stop()。");
                } else {
                    log("B线程: 原 A线程已经安全退出。");
                }
            } catch (InterruptedException e) {
                log("B线程: 等待 A线程退出时被中断。");
                Thread.currentThread().interrupt();
            }
        }
    }

    /**
     * A线程任务：执行主要工作，并周期性发送心跳。
     */
    private static class WorkerTask implements Runnable {
        private final int generation;
        private final boolean simulateFailure;
        // volatile 保证 B 线程修改 running 后，A 线程能够看到新的值。
        private volatile boolean running = true;

        WorkerTask(int generation, boolean simulateFailure) {
            this.generation = generation;
            this.simulateFailure = simulateFailure;
        }

        void requestStop() {
            running = false;
        }

        @Override
        public void run() {
            int heartbeatCount = 0;

            log("A线程-" + generation + ": 开始执行主要工作。");

            while (running && !Thread.currentThread().isInterrupted()) {
                try {
                    Thread.sleep(HEARTBEAT_INTERVAL_MS);
                } catch (InterruptedException e) {
                    log("A线程-" + generation + ": 收到中断信号，准备安全退出。");
                    Thread.currentThread().interrupt();
                    break;
                }

                if (!running || Thread.currentThread().isInterrupted()) {
                    break;
                }

                watchdog.set(1);
                heartbeatCount++;
                log("A线程-" + generation + ": 正在执行工作并发送心跳 (watchdog=1)，"
                        + "本代心跳次数: " + heartbeatCount);

                if (simulateFailure && heartbeatCount >= FIRST_WORKER_FAIL_AFTER_HEARTBEATS) {
                    // 只让第一代线程进入故障状态，以便完整展示检测和重启流程。
                    enterSimulatedFailure();
                    break;
                }
            }

            log("A线程-" + generation + ": 生命周期结束。");
        }

        /**
         * 模拟故障：线程仍然存活，但不再发送心跳。
         *
         * 该逻辑用于触发报警和重启流程，并保留中断检查以便安全结束线程。
         */
        private void enterSimulatedFailure() {
            log("A线程-" + generation + ": 模拟故障开始，将停止发送心跳。");

            while (running && !Thread.currentThread().isInterrupted()) {
                try {
                    Thread.sleep(1000);
                } catch (InterruptedException e) {
                    log("A线程-" + generation + ": 故障状态中收到中断信号，准备安全退出。");
                    Thread.currentThread().interrupt();
                    break;
                }
            }
        }
    }

    /**
     * 线程安全休眠方法。
     *
     * @return 正常睡满时间返回 true；被中断时恢复中断标志并返回 false。
     */
    private static boolean sleepSafely(int millis) {
        try {
            Thread.sleep(millis);
            return true;
        } catch (InterruptedException e) {
            log("B线程: 监控休眠被中断。");
            Thread.currentThread().interrupt();
            return false;
        }
    }

    private static void log(String message) {
        System.out.println("[" + LocalTime.now().format(TIME_FORMATTER) + "] " + message);
    }
}
