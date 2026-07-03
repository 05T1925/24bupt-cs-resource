package com.assignment.oop.instrument;

/**
 * Q4 · InstrumentTest 测试类
 *
 * 测试内容：
 *   1. 创建混合乐器数组（3把吉他、2架钢琴、2把小提琴）
 *   2. 第一轮遍历：tune() → play()（展示多态行为）
 *   3. 第二轮遍历：toString()（展示完整信息与双重编号）
 *   4. 验证 static 双重计数器正确性
 *
 * @author JavaHomework
 * @version 1.0
 */
public class InstrumentTest {

    public static void main(String[] args) {
        System.out.println("========== Q4 Instrument 乐器体系统一测试 ==========");
        System.out.println();

        // ==================== 步骤1：创建混合乐器数组 ====================
        // 创建顺序决定全局编号：每个 new 依次触发 Instrument 构造器 → instrumentCount 递增
        System.out.println("创建 7 件乐器对象（3把吉他、2架钢琴、2把小提琴）...");
        System.out.println();

        Instrument[] instruments = new Instrument[7];
        instruments[0] = new Guitar("古典吉他");      // no=1, guitarNo=1
        instruments[1] = new Piano("三角钢琴");       // no=2, pianoNo=1
        instruments[2] = new Violin("斯特拉迪瓦里");   // no=3, violinNo=1
        instruments[3] = new Guitar("民谣吉他");      // no=4, guitarNo=2
        instruments[4] = new Piano("立式钢琴");       // no=5, pianoNo=2
        instruments[5] = new Violin("瓜奈里");        // no=6, violinNo=2
        instruments[6] = new Guitar("电吉他");        // no=7, guitarNo=3

        // ==================== 步骤2：多态遍历 —— tune() + play() ====================
        System.out.println("========== 第一轮遍历：调音 + 演奏（多态） ==========");
        System.out.println();

        for (int i = 0; i < instruments.length; i++) {
            Instrument inst = instruments[i];
            System.out.println("[" + (i + 1) + "] " + inst.getClass().getSimpleName()
                             + "「" + inst.getName() + "」");

            // tune() 是父类的具体方法，所有乐器共用同一实现
            // 编译为 invokevirtual Instrument.tune()V → vtable 指向 Instrument.tune 的代码
            System.out.print("    调音: ");
            inst.tune();

            // play() 是抽象方法，由各子类重写 → 多态调用
            // 编译为 invokevirtual Instrument.play()V → vtable[play_slot] 指向 Guitar/Piano/Violin 各自的实现
            System.out.print("    演奏: ");
            inst.play();

            System.out.println();
        }

        // ==================== 步骤3：第二轮遍历 —— toString() 打印完整信息 ====================
        System.out.println("========== 第二轮遍历：toString() 完整信息 ==========");
        System.out.println();

        for (int i = 0; i < instruments.length; i++) {
            System.out.println("[" + (i + 1) + "] " + instruments[i].toString());
        }

        // ==================== 步骤4：static 计数器验证 ====================
        System.out.println();
        System.out.println("========== static 双重计数器验证 ==========");
        System.out.println();

        System.out.println("--- 计数器递增规则验证 ---");
        System.out.println("创建第 4 把吉他、第 3 架钢琴、第 3 把小提琴...");
        System.out.println();

        // 创建新对象验证计数器状态是"类级别"的（static 在 Metaspace 中持久存在，跨所有实例）
        Guitar extraGuitar = new Guitar("额外吉他");
        Piano extraPiano = new Piano("额外钢琴");
        Violin extraViolin = new Violin("额外小提琴");

        System.out.println(extraGuitar.toString());
        System.out.println("  ↑ 预期：全局编号=8, 吉他编号=4（前3把已占 1/2/3）");
        System.out.println();
        System.out.println(extraPiano.toString());
        System.out.println("  ↑ 预期：全局编号=9, 钢琴编号=3（前2架已占 1/2）");
        System.out.println();
        System.out.println(extraViolin.toString());
        System.out.println("  ↑ 预期：全局编号=10, 小提琴编号=3（前2把已占 1/2）");
        System.out.println();

        // ==================== 步骤5：验证表格 ====================
        System.out.println("========== 预期 vs 实际验证表 ==========");
        System.out.println();

        Instrument[] allInstruments = {
            instruments[0], instruments[1], instruments[2],
            instruments[3], instruments[4], instruments[5],
            instruments[6], extraGuitar, extraPiano, extraViolin
        };

        String[] expectedGlobal = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10"};
        String[] expectedSub =  {"吉他-1", "钢琴-1", "小提琴-1", "吉他-2", "钢琴-2", "小提琴-2", "吉他-3", "吉他-4", "钢琴-3", "小提琴-3"};
        String[] actualSub = {
            "吉他-" + ((Guitar) allInstruments[0]).getGuitarNo(),
            "钢琴-" + ((Piano) allInstruments[1]).getPianoNo(),
            "小提琴-" + ((Violin) allInstruments[2]).getViolinNo(),
            "吉他-" + ((Guitar) allInstruments[3]).getGuitarNo(),
            "钢琴-" + ((Piano) allInstruments[4]).getPianoNo(),
            "小提琴-" + ((Violin) allInstruments[5]).getViolinNo(),
            "吉他-" + ((Guitar) allInstruments[6]).getGuitarNo(),
            "吉他-" + extraGuitar.getGuitarNo(),
            "钢琴-" + extraPiano.getPianoNo(),
            "小提琴-" + extraViolin.getViolinNo()
        };

        System.out.println("+--------+------------------+---------------+---------------+------+");
        System.out.println("| 序号   | 实际类型          | 全局编号(no)  | 子类编号       | 验证 |");
        System.out.println("+--------+------------------+---------------+---------------+------+");

        boolean allPassed = true;
        for (int i = 0; i < allInstruments.length; i++) {
            String typeName = allInstruments[i].getClass().getSimpleName();
            String name = allInstruments[i].getName();
            int actualGlobal = allInstruments[i].getNo();
            boolean globalMatch = String.valueOf(actualGlobal).equals(expectedGlobal[i]);
            boolean subMatch = actualSub[i].equals(expectedSub[i]);
            boolean rowOk = globalMatch && subMatch;

            if (!rowOk) { allPassed = false; }

            System.out.printf("| %-6d | %-16s | 全局编号: %-3d  | %-13s | %-4s |%n",
                    (i + 1), typeName + "「" + name + "」",
                    actualGlobal, actualSub[i], rowOk ? "✅" : "❌");
        }

        System.out.println("+--------+------------------+---------------+---------------+------+");
        System.out.println();
        System.out.println("全部验证: " + (allPassed ? "✅ 通过 —— static 双重计数器实现正确" : "❌ 存在错误"));

        System.out.println();
        System.out.println("========== Q4 核心结论 ==========");
        System.out.println("1. Instrument.instrumentCount 是父类的 private static 变量 → 所有子类共享。");
        System.out.println("2. Guitar.guitarCount 是 Guitar 自己的 private static 变量 → 与父类完全独立。");
        System.out.println("3. 构造器中 super(name) 必须先于 this.xxxNo = ++xxxCount 执行。");
        System.out.println("4. static 变量属于类（Class 对象），而非实例，因此能跨越所有实例持续计数。");
        System.out.println("5. 子类可以拥有与父类「同名」的 static 字段，但这叫「隐藏」(hide)而非「重写」(override)。");
        System.out.println("6. 最佳实践：给子类计数器取不同的名（guitarCount/pianoCount/violinCount），避免混淆。");
        System.out.println();
        System.out.println("========== InstrumentTest 完成 ==========");
    }
}
