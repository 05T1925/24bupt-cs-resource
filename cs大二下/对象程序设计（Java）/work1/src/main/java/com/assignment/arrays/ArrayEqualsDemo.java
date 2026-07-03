package com.assignment.arrays;

import java.util.Arrays;

/**
 * Q1.5 判断数组是否相同
 *
 * 手动实现数组内容比较方法 arrayEquals()，逻辑如下：
 *   1. 先判断两个引用是否相同（==）→ 快速返回 true
 *   2. 任一为 null → 返回 false
 *   3. 长度不同 → 返回 false
 *   4. 逐元素比较 → 全部相同返回 true，否则 false
 *
 * 并与 Arrays.equals() 结果交叉验证。
 *
 * ──────────── 底层原理（C 语言视角） ────────────
 * 【C 语言对标】
 *   · 手动 arrayEquals ≈ C 的 memcmp(a, b, n*sizeof(int)) == 0
 *   · a == b 的引用比较 ≈ C 中 a == b 的指针比较（同一块内存 → 直接返回真）
 *   · null 检查 ≈ C 中 if(!a || !b) return false（野指针保护——Java 把这做成了语言级保证）
 *   · 长度比较 ≈ C 中 if(len_a != len_b) return false（Java 从对象头读取，C 需手动传参）
 *   · JIT 可能将整个循环向量化：一次比较 4~8 个 int（SSE: pcmpeqd + pmovmskb）
 * ─────────────────────────────────────────
 *
 * @author JavaHomework
 * @version 1.0
 */
public class ArrayEqualsDemo {

    /*
    1. 两个引用相同，直接返回 `true`。
    2. 任意一个为 `null`，返回 `false`。
    3. 长度不同，返回 `false`。
    4. 逐项比较。
     */

    public static void main(String[] args) {
        System.out.println("========== Q1.5 判断数组是否相同 ==========");
        System.out.println();

        // 准备测试数据
        // 每个 int[] 都是独立的堆对象（对象头 + length + 数据），即使内容相同也是不同对象
        int[] arr1 = {1, 2, 3, 4, 5};
        int[] arr2 = {1, 2, 3, 4, 5};  // 与 arr1 内容完全相同，但是堆上不同的对象！
        int[] arr3 = {1, 2, 3, 4, 6};  // 最后一个元素不同
        int[] arr4 = {1, 2, 3, 4};     // 长度不同
        int[] arr5 = null;              // null 引用 —— 等价于 C: int* arr5 = NULL;

        System.out.println("测试数据:");
        System.out.println("  arr1 = " + Arrays.toString(arr1));
        System.out.println("  arr2 = " + Arrays.toString(arr2));
        System.out.println("  arr3 = " + Arrays.toString(arr3));
        System.out.println("  arr4 = " + Arrays.toString(arr4));
        System.out.println("  arr5 = null");
        System.out.println();

        // ==================== 测试用例1：内容完全相同 ====================
        System.out.println("--- 测试1：内容完全相同的两个数组 ---");
        System.out.println("  arr1 vs arr2:");
        // arr1 == arr2 是 false！因为它们是堆上两个不同的对象（类似 C 中两个不同的 malloc 地址）
        // 但 arrayEquals 和 Arrays.equals 按内容比较 → 应该返回 true
        System.out.println("    手动 arrayEquals:     " + arrayEquals(arr1, arr2) + " (期望: true)");
        System.out.println("    Arrays.equals 验证:   " + Arrays.equals(arr1, arr2) + " (期望: true)");
        System.out.println();

        // ==================== 测试用例2：长度相同但元素不同 ====================
        System.out.println("--- 测试2：长度相同但最后一个元素不同 ---");
        System.out.println("  arr1 vs arr3:");
        System.out.println("    手动 arrayEquals:     " + arrayEquals(arr1, arr3) + " (期望: false)");
        System.out.println("    Arrays.equals 验证:   " + Arrays.equals(arr1, arr3) + " (期望: false)");
        System.out.println();

        // ==================== 测试用例3：长度不同 ====================
        System.out.println("--- 测试3：长度不同的两个数组 ---");
        System.out.println("  arr1 vs arr4:");
        System.out.println("    手动 arrayEquals:     " + arrayEquals(arr1, arr4) + " (期望: false)");
        System.out.println("    Arrays.equals 验证:   " + Arrays.equals(arr1, arr4) + " (期望: false)");
        System.out.println();

        // ==================== 测试用例4：与 null 比较 ====================
        System.out.println("--- 测试4：与 null 比较 ---");
        System.out.println("  arr1 vs arr5 (null):");
        // Java 的 null 引用 ≈ C 的 NULL 指针：不指向任何有效内存
        // 调用 arr5.length 会触发 NullPointerException（类似 C 中解引用 NULL 指针 → segfault）
        // 但 arrayEquals 内部先检查了 null，避免了 NPE
        System.out.println("    手动 arrayEquals:     " + arrayEquals(arr1, arr5) + " (期望: false)");
        System.out.println("    Arrays.equals 验证:   " + Arrays.equals(arr1, arr5) + " (期望: false)");
        System.out.println();

        // ==================== 测试用例5：同一引用（自己跟自己比） ====================
        System.out.println("--- 测试5：同一引用（arr1 vs arr1） ---");
        // arr1 == arr1 是 true —— 同一个引用指向同一块堆内存
        // 等价于 C 中：ptr_a == ptr_b 比较的是内存地址而非内容
        System.out.println("    手动 arrayEquals:     " + arrayEquals(arr1, arr1) + " (期望: true)");
        System.out.println("    Arrays.equals 验证:   " + Arrays.equals(arr1, arr1) + " (期望: true)");
        System.out.println();

        // ==================== 测试用例6：两个 null ====================
        System.out.println("--- 测试6：null vs null ---");
        // 两个 null 应该被视为"相等"——都表示"没有数组"
        // 等价于 C 中：两个 NULL 指针不指向任何数据，约定上视为一致
        System.out.println("    手动 arrayEquals:     " + arrayEquals(null, null) + " (期望: true)");
        System.out.println("    Arrays.equals 验证:   " + Arrays.equals((int[]) null, (int[]) null) + " (期望: true)");
        System.out.println();

        // ==================== 交叉验证结论 ====================
        System.out.println("========== 交叉验证 ==========");
        // 逐项验证手动实现与 JDK 实现的一致性
        boolean allMatch = (arrayEquals(arr1, arr2) == Arrays.equals(arr1, arr2))
                        && (arrayEquals(arr1, arr3) == Arrays.equals(arr1, arr3))
                        && (arrayEquals(arr1, arr4) == Arrays.equals(arr1, arr4))
                        && (arrayEquals(arr1, arr5) == Arrays.equals(arr1, arr5))
                        && (arrayEquals(arr1, arr1) == Arrays.equals(arr1, arr1))
                        && (arrayEquals(null, null) == Arrays.equals((int[]) null, (int[]) null));
        System.out.println("手动实现的 arrayEquals 与 Arrays.equals 结果完全一致: " + (allMatch ? "✅ 通过" : "❌ 不通过"));
    }

    /**
     * 手动实现：判断两个 int 数组的内容是否完全相同
     *
     * 【比较逻辑】按优先级排序，类似短路求值：
     *   1. a == b（引用相同 → 包括两者都为 null）→ 直接 true
     *   2. 任一为 null → false
     *   3. 长度不同 → false
     *   4. 逐元素 == 比较
     *
     * 【底层对标】
     *   等价于 C: return memcmp(a, b, n * sizeof(int)) == 0;
     *   但 memcmp 也是逐字节比较，逻辑等价
     *
     * @param a 第一个数组
     * @param b 第二个数组
     * @return 两个数组内容完全相同返回 true，否则 false
     */
    public static boolean arrayEquals(int[] a, int[] b) {
        // 步骤1：引用相同（包括两者都为 null 的情况）
        // 【底层】a == b 比较的是栈上两个引用变量的值（即堆地址）
        // 如果地址相同 → 同一个对象 → 内容必然相同
        if (a == b) {
            return true;
        }

        // 步骤2：任一为 null（此时两者不可能都为 null，因为步骤1已处理）
        // 【底层】保护性检查，防止后续 a.length 触发 NullPointerException
        // 类似 C: if (!a || !b) return false;
        if (a == null || b == null) {
            return false;
        }

        // 步骤3：比较长度 —— 读取对象头中的 length 字段
        // 【底层】等价于 C: if(len_a != len_b) return false;
        // Java 的 .length 是对象内字段直接读取（offset固定），一次 mov 指令即可
        if (a.length != b.length) {
            return false;
        }

        // 步骤4：逐个比较每个元素
        // 【底层】等价于 C: for(i=0;i<n;i++) if(a[i]!=b[i]) return false;
        // JIT 可能将此循环编译为：
        //   movdqu xmm0, [a+i]  ; 加载 a 的 4 个 int（16 字节）
        //   movdqu xmm1, [b+i]  ; 加载 b 的 4 个 int
        //   pcmpeqd xmm0, xmm1  ; 逐 int 比较
        //   pmovmskb eax, xmm0  ; 提取比较结果位掩码
        //   cmp eax, 0xFFFF     ; 全部相等？
        //   jne .not_equal
        for (int i = 0; i < a.length; i++) {
            if (a[i] != b[i]) {
                return false;  // 发现不同元素，立即返回 false（短路优化）
            }
        }

        // 所有元素都相同
        return true;
    }
}
