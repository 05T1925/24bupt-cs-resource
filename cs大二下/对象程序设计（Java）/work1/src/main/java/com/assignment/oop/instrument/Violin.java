package com.assignment.oop.instrument;

/**
 * Q4 · Violin 小提琴子类
 *
 * 继承 Instrument，拥有独立的 static 计数器 violinCount。
 * 与 Guitar/Piano 的计数器物理隔离，各自在各自的 Klass 对象中独立递增。
 *
 * @author JavaHomework
 * @version 1.0
 */
public class Violin extends Instrument {

    /** 小提琴独立编号 —— 实例字段 */
    private int violinNo;

    /**
     * 小提琴独立计数器（private static）
     * 位于 Metaspace 中 Violin_Klass 对象内，与 Instrument.instrumentCount 物理地址不同。
     * 类比 C：Violin.c 中的 static int violinCount = 0;（独立的 .data 段地址）
     */
    private static int violinCount = 0;

    /**
     * 构造器：创建一把小提琴。
     * super(name) 先执行（全局+1），再执行 violinNo = ++violinCount（小提琴+1）。
     */
    public Violin(String name) {
        super(name);
        this.violinNo = ++violinCount;
    }

    public int getViolinNo() { return violinNo; }

    @Override
    public void play() {
        System.out.println("小提琴「" + getName() + "」拉动琴弓，流淌出优雅的音符~ 🎻");
    }

    @Override
    public String toString() {
        return String.format(
            "全局编号: %d, 乐器姓名: %s, 乐器种类: 小提琴, 小提琴编号: %d",
            no, getName(), violinNo
        );
    }
}
