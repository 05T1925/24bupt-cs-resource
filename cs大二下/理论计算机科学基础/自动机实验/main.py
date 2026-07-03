# -*- coding: utf-8 -*-
"""
main.py —— 实验主程序：演示任务一(CFG 化简)与任务二(PDA -> CFG -> 化简)。

运行：
    python3 main.py                 # 跑内置的两个测试用例
    python3 main.py <cfg文件>       # 对指定 CFG 文件做化简(任务一)
"""
import os
import sys

from grammar import (parse_simple_cfg, enumerate_language, language_equal_upto,
                     rename_to_letters)
from transforms import (eliminate_epsilon, eliminate_unit, eliminate_useless,
                        nullable_symbols, unit_pairs, generating_symbols,
                        reachable_symbols, simplify)
from pda import parse_pda, pda_to_cfg

HERE = os.path.dirname(os.path.abspath(__file__))

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")
    sys.stderr.reconfigure(encoding="utf-8")


def hr(title=""):
    print("\n" + "=" * 70)
    if title:
        print(title)
        print("=" * 70)


def show_set(name, s):
    print(f"    {name} = {{{', '.join(sorted(s))}}}")


# ============================================================
#  任务一：CFG 化简(带每一步的中间结果)
# ============================================================
def run_task1(g, max_len=7):
    hr("任务一：消除 ε 产生式、单产生式、无用符号")
    print(g.pretty("【输入文法】"))

    print("\n—— 第 1 步：消除 ε 产生式 ——")
    g1, nullable = eliminate_epsilon(g)
    show_set("可空变量 Nullable", nullable)
    print(g1.pretty("结果："))

    print("\n—— 第 2 步：消除单产生式 ——")
    g2, pairs = eliminate_unit(g1)
    nonempty = {f"({A},{B})" for A, bs in pairs.items() for B in bs if A != B}
    show_set("非平凡单元对", nonempty or {"无"})
    print(g2.pretty("结果："))

    print("\n—— 第 3 步：消除无用符号 ——")
    gen = generating_symbols(g2)
    print("    (3a) 删除非产生符号")
    show_set("产生符号", gen)
    show_set("非产生符号", set(g2.variables) - gen)
    g3, _, reach = eliminate_useless(g2)
    print("    (3b) 删除不可达符号")
    show_set("可达符号", {s for s in reach if s in g2.variables})
    print(g3.pretty("结果(最终文法)："))

    ok, l1, l3 = language_equal_upto(g, g3, max_len)
    print(f"\n[验证] 化简前后语言在长度<= {max_len} 内是否一致：{ok}")
    print(f"        生成的串(长度<= {max_len})：{sorted(l3, key=lambda s:(len(s), s))}")
    return g3


# ============================================================
#  任务二：PDA -> CFG -> 化简
# ============================================================
def run_task2(pda, max_len=8):
    hr("任务二：由 PDA 构造等价 CFG，再化简")
    print(pda.pretty("【输入下推自动机】"))

    print("\n—— 第 1 步：三元组构造法得到等价 CFG ——")
    raw = pda_to_cfg(pda)
    print(f"    变量数 = {len(raw.variables)} ，产生式数 = {raw.production_count()}")
    print(raw.pretty("原始文法(未化简)："))

    print("\n—— 第 2 步：用任务一的算法化简 ——")
    g_simpl = simplify(raw)
    print(g_simpl.pretty("化简后(三元组命名)："))

    renamed, mapping = rename_to_letters(g_simpl)
    print("\n    为便于阅读，把变量重命名：")
    for k, v in sorted(mapping.items(), key=lambda kv: kv[1]):
        print(f"        {v}  =  {k}")
    print(renamed.pretty("化简后(重命名)："))

    # 验证：与 PDA 接受的语言 { b^n a^m | 1<=m<=n } 比较
    expected = {"b" * n + "a" * m
                for n in range(1, max_len + 1)
                for m in range(1, n + 1)
                if n + m <= max_len}
    got = enumerate_language(renamed, max_len)
    print(f"\n[验证] 文法生成的串(长度<= {max_len})：")
    print("        " + ", ".join(sorted(got, key=lambda s: (len(s), s))))
    print(f"        理论语言 {{ b^n a^m | 1<=m<=n }} 在该范围内：{got == expected}")
    return renamed


def main():
    # 允许：python3 main.py 自定义.cfg  —— 只跑任务一的化简
    if len(sys.argv) > 1:
        with open(sys.argv[1], encoding="utf-8") as f:
            run_task1(parse_simple_cfg(f.read()))
        return

    with open(os.path.join(HERE, "examples", "task1_grammar.cfg"), encoding="utf-8") as f:
        g = parse_simple_cfg(f.read())
    run_task1(g)

    with open(os.path.join(HERE, "examples", "task2_pda.txt"), encoding="utf-8") as f:
        pda = parse_pda(f.read())
    run_task2(pda)

    hr("全部测试完成")


if __name__ == "__main__":
    main()
