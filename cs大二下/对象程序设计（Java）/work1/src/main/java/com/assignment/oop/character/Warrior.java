package com.assignment.oop.character;

/**
 * Q3 · Warrior 战士子类
 *
 * 继承 GameCharacter，实现战士的攻击方式（挥砍）。
 * 额外拥有特有方法 shieldBlock()（盾牌格挡），
 * 此方法在父类引用中无法直接调用，必须向下转型。
 *
 * ──────────── 底层原理（C 语言视角） ────────────
 * 【特有方法的底层含义】
 *   shieldBlock() 不在 GameCharacter 的 vtable 槽位中——
 *   它是 Warrior 类独有的方法，只在 Warrior 的 vtable 扩展区（或直接调用）中存在
 *
 *   这意味着：
 *     GameCharacter c = new Warrior("亚瑟", 15);
 *     c.attack();    // ✅ 可以——attack 在 GameCharacter 的 vtable 中
 *     c.shieldBlock(); // ❌ 编译错误——编译器在 GameCharacter 的 vtable 中找不到此方法
 *
 *   C 语言等价：
 *     GameCharacter* c = (GameCharacter*)warrior;
 *     c->vtable->attack(c);       // ✅ attack 在基类 vtable 中
 *     // shieldBlock 不在基类 vtable 中 → 根本无法通过 GameCharacter* 调用
 *
 *   解决：((Warrior*)c)->shieldBlock(c);  ← 向下转型
 *   但这是危险的——如果 c 实际指向 Mage → C: 未定义行为，Java: ClassCastException
 *
 * @author JavaHomework
 * @version 1.0
 */
public class Warrior extends GameCharacter {

    public Warrior(String name, int level) {
        super(name, level);  // ★ 先初始化父类字段（name, level）
                             // 等价于 C: GameCharacter_ctor((GameCharacter*)this_, name, level);
    }

    /**
     * 实现父类的抽象方法：战士攻击
     * 战士使用物理攻击——挥舞武器砍向敌人
     *
     * 【底层】此方法地址被填入 Warrior_Klass.vtable[attack_slot]
     * 与 Mage.attack 占据同一槽位 → invokevirtual 通过不同 Klass 的 vtable 分发
     */
    @Override
    public void attack() {
        System.out.println("⚔ 战士「" + getName() + "」(Lv." + getLevel() + ") 挥舞长剑，猛烈砍向敌人！");
    }

    /**
     * 战士特有方法：盾牌格挡
     * 此方法在父类 GameCharacter 中不存在，
     * 只能通过 Warrior 引用或向下转型后调用
     *
     * 【底层】此方法在 Warrior 的 vtable 中，但不在 GameCharacter 的 vtable 槽位定义中
     * 编译器按"声明类型"检查方法 → GameCharacter 类型看不到此方法
     * 必须向下转型为 Warrior → 编译器才能"看"到 vtable 中这个槽位
     *
     * C 类比：Warrior 的 vtable 比 GameCharacter 多一个槽位
     *   GameCharacter_vtable: [attack]            ← 3个槽位
     *   Warrior_vtable:       [attack][shield]    ← 4个槽位（多出来的）
     *   通过 GameCharacter* 只能访问前3个槽位
     */
    public void shieldBlock() {
        System.out.println("🛡 战士「" + getName() + "」举起盾牌，格挡了攻击！");
    }
}
