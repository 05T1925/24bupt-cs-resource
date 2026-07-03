package com.assignment.oop.character;

/**
 * Q3 · Mage 法师子类
 *
 * 继承 GameCharacter，实现法师的攻击方式（施法）。
 * 额外拥有特有方法 manaShield()（魔法护盾），
 * 此方法在父类引用中无法直接调用，必须向下转型。
 *
 * ──────────── 底层原理（C 语言视角） ────────────
 * 【C 语言对标】
 *   Mage 与 Warrior 是 GameCharacter 的两个"子类型"
 *   各自在 vtable 的扩展槽位定义了不同的特有方法
 *   通过父类指针无法访问扩展槽位 → 必须向下转型
 *
 * @author JavaHomework
 * @version 1.0
 */
public class Mage extends GameCharacter {

    public Mage(String name, int level) {
        super(name, level);  // 调用父类 GameCharacter 的构造器
    }

    /**
     * 实现父类的抽象方法：法师攻击
     * 法师使用魔法攻击——吟唱咒语释放法术
     */
    @Override
    public void attack() {
        System.out.println("🔮 法师「" + getName() + "」(Lv." + getLevel() + ") 吟唱咒语，释放奥术飞弹！");
    }

    /**
     * 法师特有方法：魔法护盾
     * 此方法在父类 GameCharacter 中不存在，
     * 只能通过 Mage 引用或向下转型后调用
     *
     * 【底层】
     *   与 Warrior.shieldBlock() 相同——这是 Mage vtable 中的扩展槽位
     *   必须向下转型为 Mage 类型才能调用
     */
    public void manaShield() {
        System.out.println("✨ 法师「" + getName() + "」展开魔法护盾，吸收了伤害！");
    }
}
