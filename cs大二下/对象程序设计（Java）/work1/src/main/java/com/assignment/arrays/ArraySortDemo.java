package com.assignment.arrays;

import java.util.Arrays;

/**
 * Q1.3 数组排序演示
 *
 * 创建长度为 100 的一维数组，填充 0~99 的随机整数，
 * 分别使用以下三种方式进行排序并对比耗时：
 *   1. Arrays.sort()       —— JDK 内置双轴快速排序（Dual-Pivot QuickSort, Yaroslavskiy 2009）
 *   2. selectionSort()     —— 选择排序（O(n²)）
 *   3. bubbleSort()        —— 冒泡排序（O(n²)）
 *
 * ──────────── 底层原理（C 语言视角） ────────────
 * 【C 语言对标】
 *   · Arrays.sort(int[])  ≈ C 的 qsort —— 但 Java 不需要函数指针回调
 *   · C 的 qsort 需要 int cmp(const void*a,const void*b) 回调 → 每次比较都是间接调用(call rax)
 *   · Java 对基本类型数组直接用 < 操作符比较 → JIT 编译为 cmp + jle/jg，零间接调用开销
 *   · Arrays.sort 底层是 Dual-Pivot QuickSort（两个 pivot 分三区），比单轴快排减少约 10% 比较
 *   · 小数组（<47 元素）自动切换到插入排序（Insertion Sort）—— 快排递归的终止条件
 * 【选择 vs 冒泡 vs 快排 性能差距】
 *   · O(n²) vs O(n log n)：n=100 时差距约 15~50 倍，n=10000 时差距可达 1000 倍以上
 *   · 选择排序交换次数少（O(n)次交换），但比较次数固定 O(n²)
 *   · 冒泡排序最优情况 O(n)（已有序+swapped优化），最坏 O(n²)，但交换次数多
 * ─────────────────────────────────────────
 *
 * @author JavaHomework
 * @version 1.0
 */
public class ArraySortDemo {

    public static void main(String[] args) {
        System.out.println("========== Q1.3 数组排序演示 ==========");
        System.out.println();

        // ==================== 步骤1：生成原始数组 ====================
        int SIZE = 100;
        // new int[SIZE] —— JVM 堆分配：对象头 + length(=100) + 100*4字节数据 = ~416 字节
        // 等价于 C: int* arr = calloc(100, sizeof(int));  ← JVM 保证元素初始化为 0
        int[] original = new int[SIZE];

        System.out.println("生成长度为 " + SIZE + " 的随机数组 (范围 0~99)：");
        for (int i = 0; i < SIZE; i++) {
            // Math.random() 返回 [0.0, 1.0) 的浮点数
            // 乘以 100 并强转为 int，得到 [0, 99] 的整数
            // 等价于 C: original[i] = (int)(rand() / (RAND_MAX + 1.0) * 100);
            original[i] = (int) (Math.random() * 100);
        }

        // 打印排序前的数组（只显示前20个元素以免输出过长）
        System.out.print("排序前 (前20个): ");
        printPartial(original, 20);
        System.out.println();

        // ==================== 步骤2：复制三份副本 ====================
        // 深拷贝——三种排序各用独立副本，避免相互干扰
        // copyOf 内部：new + System.arraycopy，保证三个副本在堆上独立
        int[] arrForSystemSort = Arrays.copyOf(original, SIZE);
        int[] arrForSelection  = Arrays.copyOf(original, SIZE);
        int[] arrForBubble     = Arrays.copyOf(original, SIZE);

        // ==================== 步骤3：三种排序 + 计时 ====================
        // --- 方式1：Arrays.sort() ---
        // 【底层】内部是 Dual-Pivot QuickSort + 小数组插入排序
        // 无需传入比较器！基本类型直接用 < 操作符，JIT 编译为内联的 cmp 指令
        // C 类比：无法用 qsort 做到"零回调开销"——因为 C 必须传函数指针
        long start1 = System.nanoTime();
        Arrays.sort(arrForSystemSort);
        long end1 = System.nanoTime();
        long time1 = end1 - start1;

        // --- 方式2：选择排序 ---
        // 【底层】每轮在未排序区间找到最小值，与当前位置交换
        // 比较次数恒为 n(n-1)/2 ≈ 4950 次（n=100），交换最多 n-1=99 次
        long start2 = System.nanoTime();
        selectionSort(arrForSelection);
        long end2 = System.nanoTime();
        long time2 = end2 - start2;

        // --- 方式3：冒泡排序 ---
        // 【底层】相邻元素两两比较交换，大值逐轮"冒泡"到末尾
        // 含 swapped 提前终止优化：若某轮无交换 → 已有序 → 提前 exit
        long start3 = System.nanoTime();
        bubbleSort(arrForBubble);
        long end3 = System.nanoTime();
        long time3 = end3 - start3;

        // ==================== 步骤4：结果对比 ====================
        System.out.println("========== 排序结果 ==========");
        System.out.println();

        System.out.println("[方式1] Arrays.sort() (JDK 双轴快排):");
        System.out.print("  排序后 (前20个): ");
        printPartial(arrForSystemSort, 20);
        System.out.println("  耗时: " + time1 + " ns (" + (time1 / 1_000_000.0) + " ms)");
        System.out.println();

        System.out.println("[方式2] 选择排序 (Selection Sort) O(n²):");
        System.out.print("  排序后 (前20个): ");
        printPartial(arrForSelection, 20);
        System.out.println("  耗时: " + time2 + " ns (" + (time2 / 1_000_000.0) + " ms)");
        System.out.println();

        System.out.println("[方式3] 冒泡排序 (Bubble Sort) O(n²):");
        System.out.print("  排序后 (前20个): ");
        printPartial(arrForBubble, 20);
        System.out.println("  耗时: " + time3 + " ns (" + (time3 / 1_000_000.0) + " ms)");
        System.out.println();

        // ==================== 步骤5：验证三种排序结果一致性 ====================
        // Arrays.equals 内部逐元素比较，等价于 memcmp 但带类型安全
        boolean allSame = Arrays.equals(arrForSystemSort, arrForSelection)
                       && Arrays.equals(arrForSystemSort, arrForBubble);
        System.out.println("========== 验证：三种排序结果是否一致？ " + (allSame ? "✅ 一致" : "❌ 不一致") + " ==========");
        System.out.println();

        // ==================== 性能对比分析 ====================
        System.out.println("========== 性能对比 ==========");
        System.out.println("Arrays.sort() 耗时     : " + String.format("%.3f", time1 / 1_000_000.0) + " ms");
        System.out.println("选择排序耗时            : " + String.format("%.3f", time2 / 1_000_000.0) + " ms");
        System.out.println("冒泡排序耗时            : " + String.format("%.3f", time3 / 1_000_000.0) + " ms");
        System.out.println();
        //System.out.println("结论: Arrays.sort() (O(n log n)) 远快于 O(n²) 的选择排序和冒泡排序。");
    }

    /**
     * 选择排序
     * 每一轮从未排序部分选出最小元素，与未排序部分的第一个元素交换
     * 时间复杂度: O(n²)，空间复杂度: O(1)，不稳定
     *
     * 【底层】等价于 C 中：for(i=0;i<n-1;i++) { int min=i; for(j=i+1;j<n;j++) if(a[j]<a[min]) min=j; swap(a+i,a+min); }
     * JIT 编译后：外层循环 → 条件跳转；内层循环 → 紧密的 cmp + cmov（条件移动）序列
     * 不稳定：因为交换可能跨越相等的元素（例：[5, 5, 2] → [2, 5, 5] 第二个5跑到了第一个5前面）
     *
     * @param arr 待排序数组
     */
    public static void selectionSort(int[] arr) {
        int n = arr.length;
        for (int i = 0; i < n - 1; i++) {
            // 假设当前第 i 个元素为最小
            int minIndex = i;
            // 在 [i+1, n-1] 中寻找更小的元素
            for (int j = i + 1; j < n; j++) {
                if (arr[j] < arr[minIndex]) {
                    minIndex = j;
                }
            }
            // 如果找到了比 arr[i] 更小的元素，交换
            if (minIndex != i) {
                int temp = arr[i];
                arr[i] = arr[minIndex];
                arr[minIndex] = temp;
            }
        }
    }

    /**
     * 冒泡排序
     * 每一轮从前往后两两比较，将较大的元素逐步"冒泡"到数组末尾
     * 时间复杂度: O(n²)，空间复杂度: O(1)，稳定
     *
     * 【底层】稳定排序——相等元素不交换，保持原有相对顺序（类似 C: if(a[j] > a[j+1]) swap）
     * swapped 优化相当于提前终止标记——最优情况 O(n)（已有序数组只需一轮扫描）
     *
     * @param arr 待排序数组
     */
    public static void bubbleSort(int[] arr) {
        int n = arr.length;
        for (int i = 0; i < n - 1; i++) {
            // 优化：如果某轮没有发生交换，说明数组已有序，可提前结束
            boolean swapped = false;
            for (int j = 0; j < n - 1 - i; j++) {
                if (arr[j] > arr[j + 1]) {
                    // 交换相邻元素
                    int temp = arr[j];
                    arr[j] = arr[j + 1];
                    arr[j + 1] = temp;
                    swapped = true;
                }
            }
            if (!swapped) {
                break;  // 没有交换，已有序
            }
        }
    }

    /**
     * 打印数组的部分元素（用于简化长数组的显示）
     *
     * @param arr   数组
     * @param limit 显示前几个元素
     */
    private static void printPartial(int[] arr, int limit) {
        System.out.print("[");
        int show = Math.min(arr.length, limit);
        for (int i = 0; i < show; i++) {
            System.out.print(arr[i]);
            if (i < show - 1) {
                System.out.print(", ");
            }
        }
        if (arr.length > limit) {
            System.out.print(", ... (共" + arr.length + "个)");
        }
        System.out.println("]");
    }
}
