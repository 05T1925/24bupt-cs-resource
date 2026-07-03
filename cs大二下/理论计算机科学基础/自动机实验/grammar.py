# -*- coding: utf-8 -*-
"""
grammar.py —— 上下文无关文法(CFG)的数据结构、解析、显示与语言枚举。

约定
----
* 一个符号(symbol)就是一个字符串。是否为变量(非终结符)由 ``variables`` 集合唯一决定，
  从而既能表示单字符文法(S, A, a, b...)，也能表示形如 "[q0,z0,q1]" 的三元组变量。
* 一条产生式的右部用「符号元组」表示；空串 ε 用空元组 ``()`` 表示。
* 文法用 ``productions`` 字典表示： 变量 -> 右部元组的集合。
"""
from __future__ import annotations

from collections import defaultdict, deque
from itertools import product as iproduct

EPSILON = "ε"           # 显示空串时使用的符号


class Grammar:
    """上下文无关文法 G = (V, T, P, S)。"""

    def __init__(self, start, variables, terminals, productions):
        self.start = start
        self.variables = set(variables)
        self.terminals = set(terminals)
        # productions: dict[str, set[tuple[str, ...]]]
        self.productions = defaultdict(set)
        for A, rhss in productions.items():
            for rhs in rhss:
                self.productions[A].add(tuple(rhs))
        self.variables.add(self.start)   # 起始符号一定是变量

    # ---------- 基本工具 ----------
    def copy(self):
        return Grammar(self.start, set(self.variables), set(self.terminals),
                       {A: set(rhss) for A, rhss in self.productions.items()})

    def is_variable(self, sym):
        return sym in self.variables

    def production_count(self):
        return sum(len(rhss) for rhss in self.productions.values())

    def format_rhs(self, rhs):
        """把右部元组渲染成字符串；空串显示为 ε。"""
        return EPSILON if len(rhs) == 0 else "".join(rhs)

    def ordered_variables(self):
        """变量的展示顺序：起始符号在最前，其余按字典序。"""
        rest = sorted(A for A in self.variables if A != self.start)
        return [self.start] + rest

    def pretty(self, title=None):
        """返回多行字符串形式的文法。"""
        lines = []
        if title:
            lines.append(title)
        for A in self.ordered_variables():
            rhss = self.productions.get(A)
            if not rhss:
                continue
            alts = sorted({self.format_rhs(r) for r in rhss}, key=lambda s: (len(s), s))
            lines.append(f"    {A} -> {' | '.join(alts)}")
        return "\n".join(lines)

    def __str__(self):
        return self.pretty()


# ============================================================
#  解析：单字符约定的 CFG 文本
# ============================================================
def parse_simple_cfg(text):
    """解析「单字符」约定的 CFG。

    规则：大写字母 = 变量，其它(小写字母/数字)= 终结符；ε / eps / $ / 空 表示空串；
    箭头可用 ``->`` 或 ``→``；以 ``#`` 开头的行为注释；第一条产生式的左部即起始符号。
    """
    productions = defaultdict(set)
    variables, terminals = set(), set()
    start = None

    for raw in text.splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if "->" in line:
            lhs, rhs = line.split("->", 1)
        elif "→" in line:
            lhs, rhs = line.split("→", 1)
        else:
            continue

        A = lhs.strip()
        variables.add(A)
        if start is None:
            start = A

        for alt in rhs.split("|"):
            alt = alt.strip()
            if alt in ("", "ε", "eps", "$", "epsilon"):
                productions[A].add(())          # 空串
                continue
            syms = []
            for ch in alt:
                if ch.isspace():
                    continue
                syms.append(ch)
                (variables if ch.isupper() else terminals).add(ch)
            productions[A].add(tuple(syms))

    return Grammar(start, variables, terminals, productions)


# ============================================================
#  语言枚举(用于正确性验证)
# ============================================================
def enumerate_language(g, max_len):
    """枚举文法 g 能生成的、长度 <= max_len 的全部终结符串。

    采用对句型做最左替换的搜索，并按「已生成终结符个数」剪枝；对本实验中
    出现的文法均能快速终止。返回字符串集合(空串记作 "")。
    """
    results = set()
    seen = set()
    stack = [(g.start,)]
    while stack:
        form = stack.pop()
        if form in seen:
            continue
        seen.add(form)

        # 找最左变量
        idx = next((i for i, s in enumerate(form) if s in g.variables), None)
        if idx is None:                      # 已是终结符串
            if len(form) <= max_len:
                results.add("".join(form))
            continue

        # 剪枝：已出现的终结符已超过上限，则不可能再缩短
        n_term = sum(1 for s in form if s not in g.variables)
        if n_term > max_len or len(form) > max_len + 8:
            continue

        A = form[idx]
        for rhs in g.productions.get(A, ()):
            stack.append(form[:idx] + tuple(rhs) + form[idx + 1:])
    return results


def language_equal_upto(g1, g2, max_len):
    """比较两个文法在长度 <= max_len 范围内生成的语言是否相同。"""
    l1, l2 = enumerate_language(g1, max_len), enumerate_language(g2, max_len)
    return l1 == l2, l1, l2


# ============================================================
#  变量重命名(把三元组变量改写为 S, A, B, C... 便于阅读)
# ============================================================
def rename_to_letters(g):
    """把变量按从起始符号可达的 BFS 顺序重命名为 S, A, B, C ...。

    返回 (新文法, 映射表)。
    """
    order, visited = [g.start], {g.start}
    queue = deque([g.start])
    while queue:
        A = queue.popleft()
        for rhs in sorted(g.productions.get(A, ())):
            for s in rhs:
                if s in g.variables and s not in visited:
                    visited.add(s)
                    order.append(s)
                    queue.append(s)
    for A in sorted(g.variables):            # 兜底：理论上无用符号已删除
        if A not in visited:
            order.append(A)

    pool = list("ABCDEFGHIJKLMNPQRTUVWXYZ")   # 其余变量用的字母
    mapping = {g.start: "S"}
    i = 0
    for A in order:
        if A == g.start:
            continue
        mapping[A] = pool[i]
        i += 1

    new = defaultdict(set)
    for A, rhss in g.productions.items():
        for rhs in rhss:
            new[mapping[A]].add(tuple(mapping.get(s, s) for s in rhs))
    return Grammar("S", set(mapping.values()), set(g.terminals), new), mapping
