package com.assignment.arrays;

import java.util.Arrays;

/**
 * Q1.6 Arrays.fill 填充演示
 *
 * 演示 Arrays.fill() 的两种用法：
 *   1. 全部填充：Arrays.fill(数组, 值)         —— 将数组所有元素填充为指定值
 *   2. 部分填充：Arrays.fill(数组, from, to, 值) —— 将 [from, to) 区间填充为指定值
 *
 * ──────────── 底层原理（C 语言视角） ────────────
 * 【C 语言对标】
 *   · Arrays.fill(arr, val)      ≈ C: for(i=0;i<n;i++) arr[i]=val; 或 memset 的特化版本
 *   · Arrays.fill(arr, f, t, v)  ≈ C: for(i=f;i<t;i++) arr[i]=v; 区间填充
 *   · memset 用于字节级填充（char），fill 按元素类型填充（int/double/Object）
 *   · Java 的 fill 对基本类型是直接赋值，对引用类型是复制引用（类似 C 的浅拷贝）
 *   · JIT 可将填充循环向量化：STOSD 指令（rep stosd）或 SSE/AVX 的向量存储
 *   · 区间参数 [from, to) 与 copyOfRange 一致 —— Java 库的统一左闭右开惯例
 * ─────────────────────────────────────────
 *
 * @author JavaHomework
 * @version 1.0
 */
public class ArrayFillDemo {

    public static void main(String[] args) {
        System.out.println("========== Q1.6 Arrays.fill 填充演示 ==========");
        System.out.println();

        // ==================== 演示1：全部填充 ====================
        System.out.println("--- 演示1：Arrays.fill(数组, 值) 全部填充 ---");
        System.out.println();

        // 创建一个有初始值的数组
        int[] arr1 = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        System.out.println("[填充前] " + Arrays.toString(arr1));

        // 使用 Arrays.fill 将所有元素填充为 42
        // 【底层】等价于 C: for(i=0;i<10;i++) arr[i]=42;
        // JIT 会将此循环编译为高效的块填充指令（rep stosd 或向量化存储）
        // 对引用类型数组，fill 复制的是引用（指针），不是深拷贝对象
        Arrays.fill(arr1, 42);
        System.out.println("[填充后] Arrays.fill(arr1, 42) → " + Arrays.toString(arr1));
        System.out.println();

        // ==================== 演示2：部分填充 ====================
        System.out.println("--- 演示2：Arrays.fill(数组, from, to, 值) 部分填充 ---");
        System.out.println();

        // 重新创建数组
        int[] arr2 = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
        System.out.println("[填充前] " + Arrays.toString(arr2));

        // 将索引 [2, 5) 的元素填充为 99
        // 即索引 2, 3, 4 被替换为 99 —— 区间长度 = to - from = 5 - 2 = 3
        // 【底层】等价于 C: for(i=2;i<5;i++) arr[i]=99;
        Arrays.fill(arr2, 2, 5, 99);
        System.out.println("[填充后] Arrays.fill(arr2, 2, 5, 99) → " + Arrays.toString(arr2));
        System.out.println("说明: 区间 [2, 5) 即索引 2, 3, 4 被替换为 99");
        System.out.println();

        // ==================== 演示3：不同类型数组的填充 ====================
        System.out.println("--- 演示3：不同类型数组的填充 ---");
        System.out.println();

        // double 数组 —— new double[5] 等价于 C: calloc(5, sizeof(double))
        // JVM 保证元素初始化为 0.0（IEEE 754 正零）
        double[] doubleArr = new double[5];
        Arrays.fill(doubleArr, 3.14);
        System.out.println("double 数组填充: Arrays.fill(doubleArr, 3.14) → " + Arrays.toString(doubleArr));

        // boolean 数组 —— 初始化为 false
        boolean[] boolArr = new boolean[5];
        Arrays.fill(boolArr, true);
        System.out.println("boolean 数组填充: Arrays.fill(boolArr, true) → " + Arrays.toString(boolArr));

        // String 数组 —— 引用类型数组，元素是指向 String 对象的引用（类似 C 的 char**）
        // 初始化为 null（引用类型默认值）
        String[] strArr = new String[5];
        Arrays.fill(strArr, "Hello");
        System.out.println("String 数组填充:  Arrays.fill(strArr, \"Hello\") → " + Arrays.toString(strArr));
        System.out.println();

        // ==================== 演示4：全部填充 vs 部分填充对比 ====================
        System.out.println("--- 演示4：全部填充后重新初始化再部分填充的对比 ---");
        System.out.println();

        int[] arr3 = new int[8];
        // 先全部填充为基础值 —— 把所有8个元素设为0
        Arrays.fill(arr3, 0);
        System.out.println("初始（全部填充为0）: " + Arrays.toString(arr3));

        // 部分区域填充特殊值 —— 分段填充，模拟不同的数据区域
        Arrays.fill(arr3, 0, 3, 1);  // 前3个 → 1（区间[0,3)）
        Arrays.fill(arr3, 3, 6, 2);  // 中间3个 → 2（区间[3,6)）
        Arrays.fill(arr3, 6, 8, 3);  // 后2个 → 3（区间[6,8)）
        System.out.println("分段填充后          : " + Arrays.toString(arr3));
        System.out.println("  [0,3)→1  |  [3,6)→2  |  [6,8)→3");
        System.out.println();

        // ==================== 结论 ====================
        System.out.println("========== 结论 ==========");
        System.out.println("1. Arrays.fill(arr, value) 将数组所有元素填充为 value。");
        System.out.println("2. Arrays.fill(arr, from, to, value) 仅填充 [from, to) 区间。");
        System.out.println("3. fill 方法支持所有基本类型数组和引用类型数组。");
        System.out.println("4. fill 的区间参数同样遵循 [from, to) 左闭右开规则。");
    }
}
