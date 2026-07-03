package com.assignment.arrays;

import java.util.Arrays;

/**
 * Q1.2 打印数组内容演示
 *
 * 对比两种打印数组的方式：
 *   1. 使用 for 循环逐个遍历，手动拼接打印
 *   2. 使用 Arrays.toString() 直接转换为字符串打印
 *
 * ──────────── 底层原理（C 语言视角） ────────────
 * 【C 语言对标】
 *   · arr.toString()  ≈ C 中自定义的 print_array(arr, n) —— 但 Java 自带实现
 *   · 直接打印 arr    ≈ C 中 printf("%p", arr) —— 输出的是引用地址而非内容
 *   · Java 数组是对象，Object.toString() 返回 类名@hashCode（类似 C 的指针值）
 *   · deepToString 相当于递归解析指针数组的内容（类似对 int** 每个 int* 做 toString）
 *   · C 中没法从 int* 获取数组长度，Java 可从对象头读取 .length
 * ─────────────────────────────────────────
 *
 * @author JavaHomework
 * @version 1.0
 */
public class ArrayPrintDemo {

    public static void main(String[] args) {
        System.out.println("========== Q1.2 打印数组内容演示 ==========");
        System.out.println();

        // 准备测试数组
        // Java 的 int[] 是堆上的数组对象（对象头 + length + 数据）
        // C 的 int arr[] 是栈上连续内存 或 malloc 堆块（无元数据）
        int[] arr = {15, 28, 37, 42, 56, 63, 79, 81, 94, 10};
        // String[] —— 引用类型数组，每个元素是指向堆上 String 对象的引用（类似 C 的 char**）
        String[] strArr = {"苹果", "香蕉", "橙子", "葡萄", "西瓜"};

        // ==================== 方式1：for 循环手动打印 ====================
        System.out.println("--- 方式1：使用 for 循环手动打印 ---");

        // 打印 int 数组
        // arr.length 读取的是数组对象头中存储的长度元数据（offset +12 或 +16）
        // 等价于 C 中手动维护的 n 变量，但 Java 由 JVM 强制保证正确性
        System.out.print("int 数组内容: ");
        System.out.print("[");
        for (int i = 0; i < arr.length; i++) {
            System.out.print(arr[i]);
            if (i < arr.length - 1) {
                System.out.print(", ");  // 元素之间加逗号分隔
            }
        }
        System.out.println("]");

        // 打印 String 数组
        System.out.print("String 数组内容: ");
        System.out.print("[");
        for (int i = 0; i < strArr.length; i++) {
            System.out.print(strArr[i]);
            if (i < strArr.length - 1) {
                System.out.print(", ");
            }
        }
        System.out.println("]");
        System.out.println();

        // ==================== 方式2：Arrays.toString() ====================
        System.out.println("--- 方式2：使用 Arrays.toString() 打印 ---");

        // Arrays.toString() 自动将数组转换为 "[元素1, 元素2, ...]" 格式的字符串
        // 底层等价于：StringBuilder sb; for(i) sb.append(arr[i]); return sb.toString();
        System.out.println("int 数组内容: " + Arrays.toString(arr));
        System.out.println("String 数组内容: " + Arrays.toString(strArr));
        System.out.println();

        // 演示：如果直接打印数组变量名，输出的是哈希值而非内容
        // 【底层原理】Object.toString() 实现：类名 + "@" + Integer.toHexString(hashCode())
        // 数组的 hashCode() 继承自 Object（基于对象地址），不是基于元素内容
        // C 类比：printf("%p", arr) —— 输出指针值而非数组内容
        System.out.println("--- 对比：直接打印数组变量（非 toString） ---");
        System.out.println("直接打印 arr:     " + arr);       // 输出类似 [I@15db9742 —— [I=int[], @=分隔, hex哈希
        System.out.println("Arrays.toString:  " + Arrays.toString(arr));  // 输出实际内容
        System.out.println();

        // ==================== 多维数组的打印 ====================
        System.out.println("--- 扩展：二维数组的打印 ---");
        // ★ 重点：Java 的 int[][] 是"数组的数组"，类似 C 的 int**（指针数组）
        // matrix 是一个以 int[] 为元素的数组，每个 matrix[i] 又是一个 int[] 对象
        // 物理上：1个外部数组对象 + 3个内部行数组对象 = 4个堆对象（不连续！）
        int[][] matrix = {
            {1, 2, 3},
            {4, 5, 6},
            {7, 8, 9}
        };
        // 二维数组需要使用 Arrays.deepToString()
        // Arrays.toString(matrix) 会对每个元素调用 toString()——
        // 而 matrix 的元素是 int[] 对象，它们的 toString() 又是哈希值
        System.out.println("使用 Arrays.toString(matrix):   " + Arrays.toString(matrix));
        System.out.println("     ↑ 输出的是每行数组对象的哈希值，不是元素内容");
        // deepToString 会递归解析：如果元素是数组，则继续展开
        System.out.println("使用 Arrays.deepToString(matrix): " + Arrays.deepToString(matrix));
        System.out.println("     ↑ deepToString 才能正确显示多维数组的元素内容");
        System.out.println();

        // ==================== 结论 ====================
        System.out.println("========== 结论 ==========");
        System.out.println("1. for 循环打印灵活但繁琐，适合自定义输出格式。");
        System.out.println("2. Arrays.toString() 一行代码搞定，方便调试和观察数组内容。");
        System.out.println("3. 直接打印数组变量名输出的是类名+哈希值，无法看到元素内容。");
        System.out.println("4. 多维数组请使用 Arrays.deepToString()。");
    }
}
