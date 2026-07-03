package com.assignment.arrays;

import java.util.Arrays;

/**
 * Q1.7 二维数组排序（核心难点）⭐
 *
 * 创建一个 10×10 的二维数组，填充 0~99 的随机整数。
 * 要求排序后 arr[0] 的十个元素全部 ≤ arr[1] 的十个元素 ≤ ... ≤ arr[9] 的十个元素，
 * 即所有 100 个元素按全局升序排列。
 *
 * ──────────── 底层原理（C 语言视角） ────────────
 *
 * 【核心问题】能否直接用 Arrays.sort(int[][]) 对二维数组排序？
 *
 *   答：绝对不能！
 *
 * 【底层原因——C 语言视角的关键差异】
 *
 *   ★ C 语言的 int arr[10][10] 是一块连续的物理内存（400 字节 = 100 * sizeof(int)）：
 *     arr[i][j] 等价于 *( (int*)arr + i*10 + j )
 *     因此 C 中可以直接 qsort(arr, 100, sizeof(int), cmp) 对整个块排序！
 *
 *   ★ Java 的 int[][] 完全不是连续内存！
 *     它是"数组的数组"，底层等价于 C 语言的 int**（指针数组）：
 *       int** matrix = malloc(10 * sizeof(int*));  // 外部数组：10个指针
 *       for(i=0;i<10;i++) matrix[i] = malloc(10 * sizeof(int));  // 每行单独分配
 *
 *     物理布局：1个外部数组对象 + 10个行数组对象 = 共 11 个堆对象，各不连续！
 *
 *   ★ int[][] 的元素类型是 int[]，而 int[] 没有实现 Comparable：
 *     - 因此不能直接调用 Arrays.sort(matrix) 使用自然顺序排序
 *     - 即使提供 Comparator<int[]>，也只能比较并重排“整行”
 *     - 它仍然不能把 100 个 int 元素作为一个整体直接排序
 *
 * 【正确方案】三步法：展平 → 一维排序 → 回填
 *   步骤1：将 10×10 (100个元素) 通过 System.arraycopy 展平到连续的 int[100]
 *          此时相当于 C 中的：memcpy(flat, matrix[i], 10*sizeof(int)) 逐行拼接
 *   步骤2：对展平后的一维 int[] 使用 Arrays.sort() 排序
 *          此时 flat 是连续的堆内存块，sort 可正常工作
 *   步骤3：将排序后的一维数组通过 System.arraycopy 逐行回填到二维数组
 *          matrix[i] 的每个 int[] 对象内部是连续内存，可以独立排序后回填
 *
 * ─────────────────────────────────────────────────────────
 *
 * @author JavaHomework
 * @version 1.0
 */
public class TwoDArraySortDemo {

    private static final int ROWS = 10;  // 行数
    private static final int COLS = 10;  // 列数

    public static void main(String[] args) {
        System.out.println("========== Q1.7 二维数组排序演示 ==========");
        System.out.println();

        // ==================== 步骤1：创建 10×10 二维数组并填充随机数 ====================
        // ★ 底层：这创建了 1 + 10 = 11 个堆对象！
        //   matrix        → 一个 int[][] 对象（外部数组，含10个 int[] 引用）
        //   matrix[0..9]  → 10 个独立的 int[] 对象（每行10个int）
        //   这些对象在堆上的物理地址是不连续的！类似 C 的 int** + 逐行 malloc
        int[][] matrix = new int[ROWS][COLS];

        System.out.println("创建 " + ROWS + "×" + COLS + " 二维数组，填充 0~99 随机整数...");
        for (int i = 0; i < ROWS; i++) {
            for (int j = 0; j < COLS; j++) {
                // Math.random() 返回 [0.0, 1.0)，乘以 100 得到 [0, 99]
                matrix[i][j] = (int) (Math.random() * 100);
            }
        }

        // 打印排序前的二维数组
        System.out.println();
        System.out.println("========== 排序前 ==========");
        printMatrix(matrix);

        // ==================== 步骤2：说明为什么不能直接排序 int[][] ====================
        System.out.println();
        System.out.println("--- 为什么不能直接调用 Arrays.sort(matrix)？ ---");
        System.out.println("int[][] 的每个元素都是 int[] 行数组，而 int[] 未实现 Comparable，");
        System.out.println("所以 Arrays.sort(matrix) 没有可用的自然顺序，不能直接编译通过。");
        System.out.println("即使提供 Comparator<int[]>，也只能重排行对象，");
        System.out.println("不能直接把二维数组中的 100 个整数作为整体排序。");
        System.out.println("因此：不能用 Arrays.sort() 直接对二维数组按元素值排序！");
        System.out.println();
        System.out.println("正确做法：先将二维展平为一维 → 对一维排序 → 再回填到二维。");
        System.out.println();

        // ==================== 步骤3：正确方案 —— 三步法排序 ====================
        // 调用 flattenSort —— 内部实现：展平→排序→回填
        flattenSort(matrix);

        // ==================== 步骤4：打印排序后的二维数组 ====================
        System.out.println("========== 排序后 ==========");
        printMatrix(matrix);

        // ==================== 步骤5：验证排序结果 ====================
        System.out.println();
        System.out.println("========== 验证排序结果 ==========");
        boolean sorted = verifySorted(matrix);
        System.out.println("每一行的最大值 ≤ 下一行的最小值: " + (sorted ? "✅ 通过" : "❌ 未通过"));
        System.out.println();

        // 额外验证：展示跨行边界元素
        System.out.println("--- 跨行边界元素验证 ---");
        for (int i = 0; i < ROWS - 1; i++) {
            int maxOfRow = matrix[i][COLS - 1];      // 当前行最后一个（最大值）
            int minOfNext = matrix[i + 1][0];         // 下一行第一个（最小值）
            System.out.printf("  arr[%d][9] = %2d  ≤  arr[%d][0] = %2d   %s%n",
                    i, maxOfRow, i + 1, minOfNext,
                    maxOfRow <= minOfNext ? "✅" : "❌");
        }
        System.out.println();

        // ==================== 结论 ====================
        System.out.println("========== 结论 ==========");
        System.out.println("1. 不能用 Arrays.sort(int[][]) 直接对二维数组按元素值排序。");
        System.out.println("   int[] 不具备自然顺序；提供比较器也只能重排行对象。");
        System.out.println("2. 正确做法：展平→一维排序→回填（三步法）。");
        System.out.println("3. System.arraycopy 对基本类型 int 是值拷贝，不存在引用共享问题。");
        System.out.println("4. 如果使用 Integer[][]（包装类型），则需要额外注意引用问题。");
    }

    /**
     * 三步法：对二维数组进行全局排序
     *
     * 步骤1：展平 —— 将 ROWS×COLS 的二维数组复制到长度为 ROWS*COLS 的一维数组
     * 步骤2：排序 —— 对一维数组使用 Arrays.sort()
     * 步骤3：回填 —— 将排序后的一维数组按行复制回二维数组
     *
     * 【核心底层逻辑】
     *   Java 的 int[][] 各行是独立的堆对象，内存不连续 → 无法整体排序
     *   必须先把分散在 10 个对象中的 100 个元素"临时迁移"到一个连续的一维 int[100] 中
     *   等价于 C 语言中：
     *     int* flat = malloc(100 * sizeof(int));
     *     for(i=0;i<10;i++) memcpy(flat + i*10, matrix[i], 10*sizeof(int));
     *     qsort(flat, 100, sizeof(int), cmp);
     *     for(i=0;i<10;i++) memcpy(matrix[i], flat + i*10, 10*sizeof(int));
     *     free(flat);
     *
     * @param matrix 待排序的二维数组（直接修改原数组）
     */
    public static void flattenSort(int[][] matrix) {
        int rows = matrix.length;
        int cols = matrix[0].length;
        int total = rows * cols;

        // ===== 步骤1：展平 —— 二维数组 → 一维数组 =====
        // 使用 System.arraycopy 将每一行复制到一维数组的对应位置
        // 第 i 行在展平后位于一维数组的 i * cols 到 (i+1) * cols - 1 位置
        //
        // 【内存语义】new int[total] 在堆上分配连续 100*4 = 400 字节 + 对象头
        // 等价于 C 中的 malloc(total * sizeof(int))
        int[] flat = new int[total];
        for (int i = 0; i < rows; i++) {
            // System.arraycopy(matrix[i], 0, flat, i*cols, cols)
            // 等价于 C: memcpy(flat + i*cols, matrix[i], cols * sizeof(int))
            // i*cols 是目标偏移量——将每行放在展平数组的对应位置
            System.arraycopy(matrix[i], 0, flat, i * cols, cols);
        }

        // ===== 步骤2：一维数组排序 =====
        // 此时可以使用 Arrays.sort() 了！
        // 因为 flat 是一个 int[] 类型——堆上连续内存，Arrays.sort(int[]) 按元素值排序
        // 等价于 C: qsort(flat, total, sizeof(int), compare_int)
        Arrays.sort(flat);

        // ===== 步骤3：回填 —— 一维数组 → 二维数组 =====
        // 将排序后的一维数组按行逐段复制回二维数组
        // 每行回填 cols 个元素，相当于把排序好的连续数据"切段分发"回各行
        // 等价于 C: for(i=0;i<rows;i++) memcpy(matrix[i], flat + i*cols, cols*sizeof(int));
        for (int i = 0; i < rows; i++) {
            System.arraycopy(flat, i * cols, matrix[i], 0, cols);
        }
    }

    /**
     * 格式化打印二维数组（4位对齐，方便观察排序效果）
     *
     * @param matrix 二维数组
     */
    public static void printMatrix(int[][] matrix) {
        for (int i = 0; i < matrix.length; i++) {
            System.out.print("arr[" + i + "]: [");
            for (int j = 0; j < matrix[i].length; j++) {
                // 使用 printf 格式化，每个元素占4个字符宽度，右对齐
                System.out.printf("%4d", matrix[i][j]);
                if (j < matrix[i].length - 1) {
                    System.out.print(",");
                }
            }
            System.out.println(" ]");
        }
    }

    /**
     * 验证二维数组是否已按全局升序排列
     * 检查标准：每一行的最后一个元素（该行最大值）≤ 下一行的第一个元素（下一行最小值）
     *
     * @param matrix 待验证的二维数组
     * @return 排序正确返回 true
     */
    public static boolean verifySorted(int[][] matrix) {
        // 跨行检查：当前行最大值 ≤ 下一行最小值
        for (int i = 0; i < matrix.length - 1; i++) {
            // 当前行最大值（最后一个元素）
            int maxOfCurrentRow = matrix[i][matrix[i].length - 1];
            // 下一行最小值（第一个元素）
            int minOfNextRow = matrix[i + 1][0];
            if (maxOfCurrentRow > minOfNextRow) {
                return false;
            }
        }
        // 额外检查：每行内部是否升序
        for (int i = 0; i < matrix.length; i++) {
            for (int j = 0; j < matrix[i].length - 1; j++) {
                if (matrix[i][j] > matrix[i][j + 1]) {
                    return false;
                }
            }
        }
        return true;
    }
}
