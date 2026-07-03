package com.assignment.arrays;

import java.util.Arrays;

/**
 * Q1.1 数组复制演示
 *
 * 演示四种数组复制方式：
 *   1. 手动 for 循环复制
 *   2. System.arraycopy()
 *   3. Arrays.copyOf()
 *   4. Arrays.copyOfRange()
 *
 * 重点验证 copyOfRange 的第三个参数 to 是"不包含"的 —— 即 [from, to) 左闭右开区间。
 *
 * ──────────── 底层原理（C 语言视角） ────────────
 * 【C 语言对标】
 *   · System.arraycopy ≈ C 的 memcpy / memmove（块级内存拷贝，JIT 编译为 rep movsd）
 *   · Arrays.copyOf   ≈ C 的 malloc + memcpy（封装版，自动分配目标内存）
 *   · copyOfRange      ≈ C 的 memcpy(dest, src+from, (to-from)*sizeof(T))
 *   · Java 数组在堆上自带 length 元数据，C 数组需要手动传递 n
 *   · [from, to) 左闭右开区间是 Java 库的统一惯例（与 C++ STL 的 begin/end 一致）
 * ─────────────────────────────────────────
 *
 * @author JavaHomework
 * @version 1.0
 */
public class ArrayCopyDemo {

    public static void main(String[] args) {
        System.out.println("========== Q1.1 数组复制演示 ==========");
        System.out.println();

        // 创建源数组 —— 等价于 C: int src[] = {10,20,...,100}; 栈上初始化
        // Java 中这是语法糖，实际在堆上分配 int[] 对象（含对象头+length字段+数据）
        int[] src = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
        System.out.println("[源数组] " + Arrays.toString(src));
        System.out.println();

        // ==================== 方式1：手动 for 循环复制 ====================
        System.out.println("--- 方式1：手动 for 循环复制 ---");
        // new int[src.length] —— 等价于 C: (int*)malloc(src.length * sizeof(int))
        // JVM 堆分配：对象头(8~12字节) + length字段(4字节) + 10*4字节数据 + 对齐填充
        // JVM 保证新数组所有元素初始化为 0（类似 calloc 而非 malloc）
        int[] manualCopy = new int[src.length];
        // 逐一赋值 —— 等价于 C: for(i=0;i<n;i++) dst[i]=src[i];
        // C2 JIT 会将此循环编译为带边界检查消除的标量循环或向量化 SIMD 指令
        for (int i = 0; i < src.length; i++) {
            manualCopy[i] = src[i];
        }
        System.out.println("手动复制结果: " + Arrays.toString(manualCopy));
        System.out.println();

        // ==================== 方式2：System.arraycopy ====================
        System.out.println("--- 方式2：System.arraycopy ---");
        // System.arraycopy(源数组, 源起始位置, 目标数组, 目标起始位置, 复制长度)
        // 注意：目标数组需要事先创建并分配好长度
        //
        // 【底层映射】C: memmove(dest, src+srcPos, length*sizeof(int))
        // HotSpot 将其识别为 intrinsic（内建方法），C2 JIT 直接生成块拷贝指令：
        //   - 小数组：rep movsd（x86-64 的字符串操作指令，一次拷贝 4 字节）
        //   - 大数组：SSE/AVX 向量化拷贝（一次 16/32 字节）
        //   - 重叠检测：自动选择拷贝方向（即 memmove 语义，非 memcpy 语义）
        int[] arraycopyResult = new int[src.length];
        System.arraycopy(src, 0, arraycopyResult, 0, src.length);
        System.out.println("arraycopy 复制全部: " + Arrays.toString(arraycopyResult));

        // 演示部分复制：从源数组索引 3 开始，复制 4 个元素
        // 等价于 C: memcpy(dst, src+3, 4*sizeof(int))
        int[] partialArraycopy = new int[4];
        System.arraycopy(src, 3, partialArraycopy, 0, 4);
        System.out.println("arraycopy 部分复制 (从索引3开始取4个): " + Arrays.toString(partialArraycopy));
        System.out.println();

        // ==================== 方式3：Arrays.copyOf ====================
        System.out.println("--- 方式3：Arrays.copyOf ---");
        // Arrays.copyOf(源数组, 新长度)
        // 不需要事先准备目标数组，通过返回值获得目标数组 —— 封装了 new + arraycopy
        // 等价于 C: int* dst = malloc(newLen*sizeof(int)); memcpy(dst, src, min(oldLen,newLen)*sizeof(int));
        int[] copyOfResult = Arrays.copyOf(src, src.length);
        System.out.println("copyOf 复制全部: " + Arrays.toString(copyOfResult));

        // copyOf 可以改变长度：截断 —— 等价于 C 中只复制前5个元素
        int[] copyOfShorter = Arrays.copyOf(src, 5);
        System.out.println("copyOf 截断(前5个): " + Arrays.toString(copyOfShorter));

        // copyOf 可以改变长度：扩容（多出的位置填充默认值 0）
        // 等价于 C: calloc(12, sizeof(int)) 然后 memcpy 前10个
        // Java 对基本类型数组的默认值：int→0, boolean→false, 引用→null
        int[] copyOfLonger = Arrays.copyOf(src, 12);
        System.out.println("copyOf 扩容(12个): " + Arrays.toString(copyOfLonger));
        System.out.println();

        // ==================== 方式4：Arrays.copyOfRange ====================
        System.out.println("--- 方式4：Arrays.copyOfRange ---");
        // ★ 重点：copyOfRange(src, from, to) 的 to 参数是"不包含"的
        // 即复制区间为 [from, to)，左闭右开 —— 与 C++ STL 的 begin/end 迭代器语义一致
        // copyOfRange(src, 2, 5) 复制索引 2, 3, 4，共 3 个元素
        // 等价于 C: memcpy(dst, src+2, (5-2)*sizeof(int)) // 长度 = to - from
        int[] rangeResult1 = Arrays.copyOfRange(src, 2, 5);
        System.out.println("copyOfRange(src, 2, 5) → 索引[2, 5)即2,3,4: " + Arrays.toString(rangeResult1));

        int[] rangeResult2 = Arrays.copyOfRange(src, 0, 3);
        System.out.println("copyOfRange(src, 0, 3) → 索引[0, 3)即0,1,2: " + Arrays.toString(rangeResult2));

        // 从中间复制到末尾 —— 等价于 C: memcpy(dst, src+7, (len-7)*sizeof(int))
        int[] rangeResult3 = Arrays.copyOfRange(src, 7, src.length);
        System.out.println("copyOfRange(src, 7, 10)→ 索引[7,10)即7,8,9: " + Arrays.toString(rangeResult3));
        System.out.println();

        // ==================== 结论 ====================
        System.out.println("========== 结论 ==========");
        System.out.println("1. copyOf / copyOfRange 不需要事先准备目标数组，通过返回值即可获得。");
        System.out.println("2. System.arraycopy 需要事先创建目标数组并分配好长度。");
        System.out.println("3. copyOfRange 的第三个参数 to 表示结束位置（不包含），区间为 [from, to)。");
        System.out.println("4. copyOf 可同时完成截断或扩容（多出元素为默认值0）。");
    }
}
