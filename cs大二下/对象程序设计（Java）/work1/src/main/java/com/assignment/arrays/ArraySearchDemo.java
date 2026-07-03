package com.assignment.arrays;

import java.util.Arrays;

/**
 * Q1.4 数组搜索演示
 *
 * 实现两种查询指定数据在数组中下标的方法：
 *   1. linearSearch()       —— 直接遍历（无需排序，任何情况可用）
 *   2. binarySearch()       —— 二分查找（必须先用 Arrays.sort() 排序）
 *
 * 注意：
 *   - 使用 binarySearch 之前必须对数组进行排序。
 *   - 如果数组中有多个相同的指定数据，binarySearch 查找结果是不确定的。
 *   - binarySearch 返回值：找到则返回下标；未找到则返回 -(插入点) - 1（负值）。
 *
 * ──────────── 底层原理（C 语言视角） ────────────
 * 【C 语言对标】
 *   · linearSearch  ≈ C 的 for(i=0;i<n;i++) if(arr[i]==target) return i;
 *   · binarySearch  ≈ C 的 bsearch(&key, arr, n, sizeof(int), cmp)
 *     — 但 C 的 bsearch 同样不保证重复元素中的"第一个"
 *   · Java 的 binarySearch 返回 -(insertionPoint)-1（负值）而非简单的 -1
 *     这样调用者可以从负值反推出"如果插入应该放在什么位置"
 *   · 时间：线性 O(n) vs 二分 O(log n) — 但二分需要 O(n log n) 的排序前置成本
 *   · 如果只查一次：线性更快（O(n) < O(n log n)）
 *   · 如果查多次：排序 + 多次二分更优（O(n log n) + k*O(log n)）
 * ─────────────────────────────────────────
 *
 * @author JavaHomework
 * @version 1.0
 */
public class ArraySearchDemo {

    public static void main(String[] args) {
        System.out.println("========== Q1.4 数组搜索演示 ==========");
        System.out.println();

        // 准备测试数组（包含重复元素以验证 binarySearch 不确定性）
        int[] arr = {45, 12, 78, 34, 12, 90, 56, 12, 23, 67};
        System.out.println("[原始数组] " + Arrays.toString(arr));
        System.out.println();

        int target = 12;  // 故意选择重复出现的元素（出现3次：索引1,4,7）

        // ==================== 方式1：线性查找 ====================
        System.out.println("--- 方式1：线性查找 (线性遍历) ---");
        System.out.println("查找目标: " + target + "（注：数组中有多个 " + target + "）");

        // 线性查找 —— 从头开始逐个比较，找到即返回
        // 等价于 C: for(i=0;i<n;i++) if(arr[i]==target) return i; return -1;
        // 优点：无需预处理，找到第一个匹配即停止
        // 缺点：最坏情况需要遍历整个数组 O(n)
        int linearIndex = linearSearch(arr, target);
        if (linearIndex != -1) {
            System.out.println("线性查找结果: 第一个 " + target + " 位于下标 " + linearIndex);
        } else {
            System.out.println("线性查找结果: 未找到 " + target);
        }
        System.out.println("说明: 线性查找无需排序，直接从前到后遍历，找到第一个即返回。");
        System.out.println();

        // 查找一个不存在的元素 —— 验证返回 -1 的约定
        int notFoundTarget = 999;
        int notFoundIndex = linearSearch(arr, notFoundTarget);
        System.out.println("查找不存在元素 " + notFoundTarget + ": 返回 " + notFoundIndex + " (表示未找到)");
        System.out.println();

        // ==================== 方式2：二分查找 ====================
        System.out.println("--- 方式2：二分查找 (binarySearch) ---");

        // ★ 关键步骤：二分查找前必须先排序！
        // 【底层原因】二分查找的核心是不变量：左半区所有元素 ≤ 中间元素 ≤ 右半区所有元素
        // 如果数组无序，这个不变量不成立，二分查找的结果无意义
        System.out.println("★ 注意：使用 binarySearch 前必须先用 Arrays.sort() 排序！");
        // copyOf 创建副本后再排序，保留原始数组供对比
        int[] sortedArr = Arrays.copyOf(arr, arr.length);
        Arrays.sort(sortedArr);
        System.out.println("[排序后的数组] " + Arrays.toString(sortedArr));
        System.out.println();

        // 使用 Arrays.binarySearch
        // 底层实现：迭代式二分查找（非递归），避免递归栈开销
        // 等价于 C: while(lo<=hi){ mid=(lo+hi)/2; if(key<arr[mid]) hi=mid-1; else if(key>arr[mid]) lo=mid+1; else return mid; }
        int binaryIndex = binarySearch(sortedArr, target);
        if (binaryIndex >= 0) {
            System.out.println("二分查找结果: " + target + " 位于下标 " + binaryIndex);
        } else {
            System.out.println("二分查找结果: 未找到 " + target + " (返回值=" + binaryIndex + ")");
        }

        // 验证：如果数组中有多个相同元素，binarySearch 结果不确定
        System.out.println();
        System.out.println("★ 重要提示：数组中有多个 " + target + " 时，binarySearch 可能返回任意一个的下标。");
        System.out.println("  原始数组中 12 出现在索引 1, 4, 7");
        System.out.println("  排序后 12 出现在索引 0, 1, 2（连续）");
        System.out.println("  binarySearch 返回了索引 " + binaryIndex + "（可能是其中任意一个）");
        System.out.println();

        // 查找不存在的元素
        int binaryNotFound = binarySearch(sortedArr, notFoundTarget);
        System.out.println("查找不存在元素 " + notFoundTarget + ": binarySearch 返回 " + binaryNotFound);
        System.out.println("  说明: binarySearch 未找到时返回负值，即为 -(插入点) - 1");
        System.out.println();

        // ==================== 对比总结 ====================
        System.out.println("========== 对比总结 ==========");
        System.out.println("+------------------+----------------------+----------------------+");
        System.out.println("|      维度        |    线性查找          |    二分查找          |");
        System.out.println("+------------------+----------------------+----------------------+");
        System.out.println("| 是否要求有序     | 不要求               | 必须有序（先排序）    |");
        System.out.println("| 时间复杂度       | O(n)                 | O(log n)             |");
        System.out.println("| 重复元素结果     | 返回第一个匹配下标   | 结果不确定            |");
        System.out.println("| 适用场景         | 小数组 / 无序数据    | 大数组 / 有序数据    |");
        System.out.println("+------------------+----------------------+----------------------+");
    }

    /**
     * 线性查找：逐个遍历数组，找到目标值即返回其下标
     *
     * 【底层】等价于 C 语言中：
     *   for (int i = 0; i < n; i++) if (arr[i] == target) return i;
     *   return -1;
     *
     * JIT 可能会对此循环做"循环展开"优化（每次迭代处理4~8个元素）
     * 并利用 SIMD 指令进行向量化比较（如 pcmpgtd 一次比较4个 int）
     *
     * @param arr    待搜索的数组
     * @param target 要查找的目标值
     * @return 目标值的下标；未找到返回 -1
     */
    public static int linearSearch(int[] arr, int target) {
        // arr.length 从对象头读取（一条 mov 指令），不占用循环内的计算
        for (int i = 0; i < arr.length; i++) {
            if (arr[i] == target) {
                return i;  // 找到，返回下标
            }
        }
        return -1;  // 遍历完未找到 —— 等价于 C 中 return -1 的哨兵值
    }

    /**
     * 二分查找：调用 Arrays.binarySearch
     * ★ 前提：数组必须已经按升序排序
     *
     * 【底层】Arrays.binarySearch 内部实现是标准的迭代式二分查找（无递归开销）
     * 时间复杂度 O(log n)，每次迭代将搜索范围减半
     * 类比 C: bsearch(&key, arr, n, sizeof(int), cmp)
     * 区别：Java 的 binarySearch 返回 -(insertionPoint)-1 而非 NULL/nullptr
     *
     * @param sortedArr 已排序的数组
     * @param target    要查找的目标值
     * @return 目标值的下标；未找到返回 -1
     */
    public static int binarySearch(int[] sortedArr, int target) {
        // Arrays.binarySearch 返回:
        //   - 找到 → 返回下标（≥ 0）
        //   - 未找到 → 返回 -(插入点) - 1（< 0）
        int result = Arrays.binarySearch(sortedArr, target);
        return result >= 0 ? result : -1;
    }
}
