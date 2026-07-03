package com.assignment.oop.character;

import java.util.ArrayList;
import java.util.Random;

/**
 * Q3 · GameTest 测试类
 *
 * 测试内容：
 *   1. 使用 ArrayList<GameCharacter> 存储角色，实现动态管理
 *   2. 模拟战斗回合：随机选择角色进行攻击
 *   3. 使用 instanceof 安全判断类型后，向下转型调用特有方法
 *   4. 展示向上转型（存入 ArrayList）和向下转型（取出后强转）的完整流程
 *
 * 核心知识点：
 *   - ArrayList 泛型 + 多态：List<父类> 可以存储任意子类对象
 *   - instanceof 安全检查：避免 ClassCastException
 *   - 向下转型：从父类引用恢复为子类引用，以调用子类特有方法
 *
 * ──────────── 底层原理（C 语言视角） ────────────
 *
 * 【instanceof 的底层真相——为什么它比 C 的强转安全？】
 *
 *   C 语言的强制转型：
 *     GameCharacter* c = get_character();
 *     Warrior* w = (Warrior*)c;     // ★ 0 条 CPU 指令！编译器单纯信任你
 *     w->shieldBlock();            // 如果 c 实际是 Mage → 行为未定义/segfault
 *
 *   Java 的 instanceof + (Warrior)：
 *     if (c instanceof Warrior) {           // ★ JVM 指令：instanceof
 *         Warrior w = (Warrior) c;          // ★ JVM 指令：checkcast
 *         w.shieldBlock();
 *     }
 *
 *   instanceof 的汇编级实现（概念）：
 *     mov  ecx, [edi + 8]          ; 从对象头读取 Klass Pointer（offset 8）
 *     cmp  ecx, Warrior_Klass      ; ★ 一次指针比较！
 *     je   .L_is_warrior           ; 命中 → 是 Warrior
 *     ; 遍历继承链 _super 指针
 *     mov  ecx, [ecx + super_off]  ; 取父类 Klass
 *     cmp  ecx, Warrior_Klass      ; 再次比较
 *     je   .L_is_warrior
 *     ; 查 secondary super cache（接口/间接父类快速哈希）
 *     ...
 *     xor  eax, eax                ; 都不匹配 → false
 *     ret
 *   .L_is_warrior:
 *     mov  eax, 1                  ; 匹配 → true
 *     ret
 *
 *   关键：instanceof 的本质是比对 Metaspace 中的 Klass 对象指针
 *   每个被加载的 Java 类在 Metaspace 中有且仅有一个 Klass 对象
 *   → 指针比较即可完成类型确认（时间复杂度 O(1) 或 O(继承深度)）
 *
 * 【ArrayList 的底层】
 *   ArrayList ≈ C++ 的 std::vector —— 底层是 Object[] 数组 + 自动扩容
 *   等价于 C: GameCharacter** arr; int size; int capacity; + realloc 逻辑
 *   泛型擦除：编译后 ArrayList<GameCharacter> 变成 ArrayList（存 Object 引用）
 *   JVM 在取出时自动插入 checkcast GameCharacter 指令
 *
 * @author JavaHomework
 * @version 1.0
 */
public class GameTest {

    public static void main(String[] args) {
        System.out.println("========== Q3 GameCharacter 角色战斗测试 ==========");
        System.out.println();

        // ==================== 步骤1：创建 ArrayList 并填充角色 ====================
        // 关键：ArrayList<GameCharacter> 可以存储任意 GameCharacter 的子类对象
        // 子类对象在存入时自动"向上转型"为 GameCharacter 引用
        //
        // 【底层】ArrayList 内部是 Object[] elementData，存储的是引用（指针）
        // 等价于 C: GameCharacter** team = malloc(capacity * sizeof(GameCharacter*));
        ArrayList<GameCharacter> team = new ArrayList<>();

        // ★
        // new Mage("刘文涛", 12) 在堆上分配约 20 字节（对象头+name+level+特有字段）
        // add() 时自动向上转型：Mage* → GameCharacter*（类似 C 的隐式 void* 转换但安全）
        team.add(new Warrior("亚瑟", 15));     // 战士
        team.add(new Mage("刘文涛", 12));      // ★ 指定姓名的法师
        team.add(new Warrior("盖伦", 18));     // 战士
        team.add(new Mage("梅林", 20));         // 法师
        team.add(new Warrior("李青", 14));     // 战士
        team.add(new Mage("吉安娜", 16));      // 法师

        System.out.println("========== 角色列表 ==========");
        for (int i = 0; i < team.size(); i++) {
            GameCharacter c = team.get(i);
            System.out.printf("  [%d] %s 「%s」 Lv.%d%n",
                    i, c.getSpecies(), c.getName(), c.getLevel());
        }
        System.out.println();

        // ==================== 步骤2：模拟随机战斗回合 ====================
        System.out.println("========== 战斗开始！共 6 回合 ==========");
        System.out.println();

        Random rand = new Random();
        int totalRounds = 6;

        for (int round = 1; round <= totalRounds; round++) {
            System.out.println("--- 第 " + round + " 回合 ---");

            // 随机选择一个角色进行攻击
            int index = rand.nextInt(team.size());
            GameCharacter character = team.get(index);

            // 多态调用 attack()：实际执行的是运行时的子类版本
            // 【底层】invokevirtual GameCharacter.attack()V
            // → 从 character 对象头读 Klass Pointer → 查 vtable[attack_slot]
            // → 如果是 Warrior → jmp Warrior.attack；如果是 Mage → jmp Mage.attack
            System.out.print("随机选中 [" + index + "] " + character.getSpecies()
                           + "「" + character.getName() + "」→ ");
            character.attack();

            // ==================== 步骤3：instanceof 安全转型 + 特有方法调用 ====================
            // ★ 关键知识点：
            //   character 的编译时类型是 GameCharacter（父类），只能调用父类中定义的方法
            //   要调用 shieldBlock() 或 manaShield()，必须先向下转型
            //   直接强转有风险：((Warrior) character).shieldBlock() ← 如果 character 实际是 Mage 则崩溃
            //   正确做法：先用 instanceof 判断运行时类型，再安全转型
            //
            // 【C 语言对比】
            //   C: if (character的某个标记 == WARRIOR_TYPE) { ((Warrior*)c)->shieldBlock(c); }
            //   但 C 没有内置的类型标记——你需要手动维护一个 type_tag 枚举字段
            //   Java 把类型标记固化到了每个对象的对象头中（Klass Pointer）

            if (character instanceof Warrior) {
                // 【底层】instanceof 指令：比对 Klass Pointer
                // 确认是 Warrior 类型 → 安全向下转型 → 调用特有方法
                // checkcast 指令：再次比对 Klass Pointer（但已在 instanceof 中确认）
                Warrior warrior = (Warrior) character;
                warrior.shieldBlock();
            } else if (character instanceof Mage) {
                // 确认是 Mage 类型 → 安全向下转型 → 调用特有方法
                Mage mage = (Mage) character;
                mage.manaShield();
            }

            System.out.println();  // 回合间空行

            // 模拟回合间隔（可选，使输出更清晰）
            // Thread.sleep 让当前线程让出 CPU（类似 C 的 sleep/usleep）
            try {
                Thread.sleep(300);
            } catch (InterruptedException e) {
                // 【底层】InterruptedException 是 Checked Exception
                // 编译器强制处理——这保证了多线程环境下中断机制的正确传播
                Thread.currentThread().interrupt();
                break;
            }
        }

        // ==================== 步骤4：集中演示 instanceof + 向下转型 ====================
        System.out.println("========== 集中演示：instanceof + 向下转型 ==========");
        System.out.println();
        System.out.println("遍历所有角色，根据实际类型调用特有技能：");
        System.out.println();

        for (GameCharacter character : team) {
            System.out.print("角色: " + character.getSpecies()
                           + "「" + character.getName() + "」(Lv." + character.getLevel() + ") → ");

            // 使用 instanceof 判断类型，然后安全转型
            // 【安全模式】先检查 → 再转型 —— 类似于 C 中检查 tag 再强转
            // 但 Java 的类型检查由 JVM 内置 (Klass Pointer 比对)，无需程序员手动维护 type_tag
            if (character instanceof Warrior) {
                Warrior w = (Warrior) character;
                w.shieldBlock();
            } else if (character instanceof Mage) {
                Mage m = (Mage) character;
                m.manaShield();
            } else {
                System.out.println("未知角色类型");
            }
        }

        // ==================== 步骤5：错误演示（注释形式展示风险） ====================
        System.out.println();
        System.out.println("--- 直接强转的风险演示（代码中已注释，此处仅为说明） ---");
        System.out.println("// 错误写法：");
        System.out.println("// GameCharacter c = team.get(0);  // 假设是 Warrior");
        System.out.println("// ((Mage) c).manaShield();       // ★ ClassCastException！");
        System.out.println("//");
        System.out.println("// 因为 c 实际指向 Warrior 对象，强行转为 Mage → 运行时异常");
        System.out.println("// 所以：向下转型前必须使用 instanceof 进行安全检查。");
        System.out.println();

        // ==================== 总结 ====================
        System.out.println("========== Q3 核心结论 ==========");
        System.out.println("1. ArrayList<父类> 存储子类对象 → 自动向上转型（安全、隐式）。");
        System.out.println("2. 取出时需向下转型 → 必须先 instanceof 检查（否则可能 ClassCastException）。");
        System.out.println("3. 多态调用 attack()：同一行代码，不同的实际行为（战士挥砍 / 法师施法）。");
        System.out.println("4. instanceof 模式：if (obj instanceof SubType) { SubType s = (SubType) obj; ... }");
        System.out.println("5. 特有方法（shieldBlock / manaShield）需要通过子类引用调用，父类引用「看」不到它们。");
    }
}
