package com.assignment.oop.instrument;

/**
 * Q4 · Piano 钢琴子类
 *
 * 继承 Instrument，拥有独立的 static 计数器 pianoCount。
 * 与 Guitar/Violin 的计数器物理隔离，各自在各自的 Klass 对象中独立递增。
 *
 * @author JavaHomework
 * @version 1.0
 */
public class Piano extends Instrument {

    /** 钢琴独立编号 —— 实例字段 */
    private int pianoNo;

    /**
     * 钢琴独立计数器（private static）
     * 位于 Metaspace 中 Piano_Klass 对象内，与 Instrument.instrumentCount 物理地址不同。
     * 类比 C：Piano.c 中的 static int pianoCount = 0;（独立的 .data 段地址）
     */
    private static int pianoCount = 0;

    /**
     * 构造器：创建一架钢琴。
     * super(name) 先执行（全局+1），再执行 pianoNo = ++pianoCount（钢琴+1）。
     * 栈帧顺序：Piano.<init> 帧 → Instrument.<init> 帧（压栈）→ 执行完 → 出栈 → Piano 帧继续。
     */
    public Piano(String name) {
        super(name);
        this.pianoNo = ++pianoCount;
    }

    public int getPianoNo() { return pianoNo; }

    @Override
    public void play() {
        System.out.println("钢琴「" + getName() + "」敲击琴键，奏出华丽的乐章~ 🎹");
    }

    @Override
    public String toString() {
        return String.format(
            "全局编号: %d, 乐器姓名: %s, 乐器种类: 钢琴, 钢琴编号: %d",
            no, getName(), pianoNo
        );
    }
}
