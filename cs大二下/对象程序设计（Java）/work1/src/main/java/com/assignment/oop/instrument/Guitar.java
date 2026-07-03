package com.assignment.oop.instrument;

/**
 * Q4 · Guitar 吉他子类
 *
 * 继承 Instrument，拥有独立的 static 计数器 guitarCount，
 * 用于追踪 Guitar 类内部对象的创建顺序（独立于父类的全局编号）。
 *
 * ──────────────────────────────────────────────
 * 底层原理（C 语言视角）
 * ──────────────────────────────────────────────
 * 【独立 static 计数器的物理隔离】
 *
 *   Instrument.instrumentCount → Metaspace 中 Instrument_Klass 的偏移量 X
 *   Guitar.guitarCount         → Metaspace 中 Guitar_Klass 的偏移量 Y
 *   地址 X ≠ 地址 Y，物理上完全独立，互不影响。
 *
 *   类比 C 语言：两个不同 .c 文件中的 static 全局变量：
 *     // instrument.c       // guitar.c
 *     static int count = 0;  static int count = 0;
 *     地址: 0x601040         地址: 0x601044  ← 不同的内存地址
 *
 * 【构造器栈帧执行时序】
 *   new Guitar("古典吉他") 的栈帧轨迹：
 *     [main 栈帧]
 *       → [Guitar.<init> 栈帧]         ← 压栈
 *           → [Instrument.<init> 栈帧]  ← 再压栈（在 Guitar 栈帧之上）
 *              执行: this.no = ++instrumentCount  (全局: 0→1)
 *           ← Instrument.<init> 返回    ← 出栈
 *          执行: this.guitarNo = ++guitarCount    (吉他: 0→1)
 *       ← Guitar.<init> 返回           ← 出栈
 *   结论：父类构造器帧先压栈先执行，保证全局编号在子类编号之前完成赋值。
 *
 * @author JavaHomework
 * @version 1.0
 */
public class Guitar extends Instrument {

    /** 吉他独立编号 —— 实例字段，存储在堆上对象中 */
    private int guitarNo;

    /**
     * 吉他独立计数器（private static）
     *
     * 物理位置：Metaspace 中 Guitar_Klass 对象内。
     * 与 Instrument.instrumentCount 位于不同 Klass 对象的不同偏移量。
     * 仅在 Guitar 构造器中被递增，Piano/Violin 的构造器不访问此变量。
     */
    private static int guitarCount = 0;

    /**
     * 构造器：创建一把吉他。
     *
     * 执行顺序（JVM 强制）：
     *   步骤1：super(name) → invokespecial Instrument.<init>
     *         在 Instrument 栈帧中：no = ++instrumentCount（全局+1）
     *   步骤2：guitarNo = ++guitarCount（吉他独立+1）
     *   super() 必须在第一行，JVM 通过 invokespecial 指令强制此约束。
     *
     * @param name 吉他名称
     */
    public Guitar(String name) {
        super(name);                          // 步骤1：父类构造器 → 全局计数
        this.guitarNo = ++guitarCount;        // 步骤2：子类独立计数
    }

    public int getGuitarNo() { return guitarNo; }

    /**
     * 实现父类抽象方法：吉他演奏。
     * 此方法的地址在类加载时填入 Guitar_Klass.vtable[play_slot]。
     * invokevirtual 时通过 vtable 间接跳转到此方法。
     */
    @Override
    public void play() {
        System.out.println("吉他「" + getName() + "」拨动琴弦，发出悠扬的旋律~ ♪♫");
    }

    /**
     * 重写 toString()：打印全部信息。
     * no 继承自 Instrument（堆上实例字段），guitarNo 是 Guitar 自有字段。
     */
    @Override
    public String toString() {
        return String.format(
            "全局编号: %d, 乐器姓名: %s, 乐器种类: 吉他, 吉他编号: %d",
            no, getName(), guitarNo
        );
    }
}
