package com.assignment.oop.instrument;

/**
 * Q4 · Instrument 抽象乐器类
 *
 * 设计要点：
 *   1. 拥有全局编号 no —— 由 private static instrumentCount 驱动，
 *      每创建一个 Instrument 对象（任意子类），no 递增 1。
 *      这保证了所有乐器按创建顺序统一编号。
 *
 *   2. 子类 Guitar / Piano / Violin 各自拥有独立的 static 计数器，
 *      各自管理本类内部的编号（guitarNo / pianoNo / violinNo）。
 *
 *   3. static 变量属于类而非实例 —— Instrument.instrumentCount 和
 *      Guitar.guitarCount 是完全不同的两个变量，互不干扰。
 *
 * ──────────────────────────────────────────────
 * 底层原理（C 语言视角）
 * ──────────────────────────────────────────────
 * 【static 变量的物理存储位置】
 *
 *   private static int instrumentCount = 0;
 *   此变量存储于 Metaspace（元空间）中 Instrument 的 Klass 对象内。
 *   类比 C 语言：它等价于文件作用域的全局变量，位于 .data 段（已初始化）：
 *     static int instrumentCount = 0;  // .data 段，地址在链接时确定
 *
 *   关键：instrumentCount 绝对不在 new Instrument() 分配的堆对象中。
 *   堆对象上只有实例字段（no, name），static 字段在 Metaspace 的固定地址。
 *
 * 【为什么子类的 static 不会覆盖父类的 static？】
 *
 *   类比 C 语言的符号表：
 *     Instrument.c::instrumentCount → 地址 0x601040（Instrument_Klass 内）
 *     Guitar.c::guitarCount        → 地址 0x601044（Guitar_Klass 内）
 *   这是两个不同的物理内存地址，各自独立，不存在"覆盖"的可能。
 *
 *   如果在 Guitar 中定义同名 static instrumentCount，这称为"隐藏"（hide），
 *   而非"重写"（override）。底层是两个不同 Klass 对象中的两个不同偏移量。
 *
 * 【构造器链与栈帧】
 *   new Guitar("古典吉他") 的执行流：
 *     1. JVM 在堆上分配对象内存
 *     2. invokespecial Guitar.<init> → 创建 Guitar 栈帧
 *     3. Guitar 栈帧中 aload_0; aload_1; invokespecial Instrument.<init>
 *        → 创建 Instrument 栈帧（压栈，在 Guitar 栈帧之上）
 *     4. Instrument.<init> 中执行 this.no = ++instrumentCount（全局+1）
 *     5. Instrument.<init> 返回（出栈，回到 Guitar 栈帧）
 *     6. Guitar.<init> 继续执行 this.guitarNo = ++guitarCount（局部+1）
 *   栈帧的嵌套保证了全局编号先于子类编号赋值。
 *
 * @author JavaHomework
 * @version 1.0
 */
public abstract class Instrument {

    /** 全局编号 —— 所有乐器按创建顺序统一编号（从 1 开始）
     *  存储位置：堆上对象的实例数据区（offset +12，紧邻对象头） */
    protected int no;

    /** 乐器姓名 —— 堆上对象中的引用字段（压缩指针，4字节），指向 String 对象 */
    protected String name;

    /**
     * 全局计数器（private static）
     * 属于 Instrument 类本身，所有子类共享。
     *
     * 物理位置：Metaspace 中 Instrument_Klass 对象的固定偏移量。
     * 类比 C：static int instrumentCount = 0; — 存储于 .data 段，进程全局唯一。
     * 不会出现在任何 new 出来的堆对象上，GC 不会回收它。
     */
    private static int instrumentCount = 0;

    /**
     * 构造器：创建乐器时必须指定姓名。
     * 每次调用自动分配全局编号。
     *
     * 编译为 invokespecial Instrument.<init>，精确调用（非虚调用）。
     * 等价于 C: this_->no = ++global_instrument_counter;
     *
     * @param name 乐器姓名
     */
    public Instrument(String name) {
        this.name = name;
        // ++instrumentCount 的汇编等价：
        //   mov eax, [Instrument_Klass + instrumentCount_offset]  ; 从 Metaspace 读取
        //   inc eax
        //   mov [Instrument_Klass + instrumentCount_offset], eax  ; 写回 Metaspace
        //   mov [this + no_offset], eax                           ; 写入堆对象
        this.no = ++instrumentCount;   // 全局编号自动递增
    }

    public int getNo() { return no; }
    public String getName() { return name; }
    public void setName(String name) { this.name = name; }

    /**
     * 抽象方法：演奏。
     * 在 vtable 中留一个槽位，由子类填充。
     * 编译为 invokevirtual Instrument.play()V → 运行时通过 vtable[play_slot] 间接跳转。
     */
    public abstract void play();

    /**
     * 具体方法：调音。
     * 非 abstract，子类可直接继承使用（无需重写）。
     * 编译为 invokevirtual Instrument.tune()V → 运行时查 vtable。
     */
    public void tune() {
        System.out.println("正在调音...");
    }
}
