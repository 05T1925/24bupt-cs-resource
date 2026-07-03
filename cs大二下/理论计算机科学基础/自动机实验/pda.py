# -*- coding: utf-8 -*-
"""
pda.py —— 下推自动机(PDA)的数据结构、文本解析，以及由 PDA 构造等价 CFG。

构造采用经典的「三元组变量」法(以空栈方式接受)：
    变量为起始符号 S 以及形如 [q, X, p] 的三元组，其含义是
    「PDA 从状态 q、栈顶为 X 出发，不依赖 X 以下的内容，最终把 X 弹出并到达状态 p
      所读入的输入串集合」。
本实验的 PDA 终态集为 Φ(空)，正是以空栈接受，故该构造适用。
"""
from collections import defaultdict
from itertools import product as iproduct

from grammar import Grammar


class PDA:
    """下推自动机 M = (Q, Σ, Γ, δ, q0, Z0, F)。"""

    def __init__(self, states, input_symbols, stack_symbols,
                 transitions, start_state, start_stack, final_states=None):
        self.states = list(states)
        self.input_symbols = set(input_symbols)
        self.stack_symbols = list(stack_symbols)
        # transitions: dict[(q, a, X)] -> list[(r, tuple(压栈串))]，a == "" 表示 ε
        self.transitions = transitions
        self.start_state = start_state
        self.start_stack = start_stack
        self.final_states = set(final_states or [])

    def pretty(self, title="PDA"):
        lines = [title,
                 f"    Q  = {{{', '.join(self.states)}}}",
                 f"    Σ  = {{{', '.join(sorted(self.input_symbols))}}}",
                 f"    Γ  = {{{', '.join(self.stack_symbols)}}}",
                 f"    q0 = {self.start_state} , Z0 = {self.start_stack} , "
                 f"F = {{{', '.join(sorted(self.final_states)) or '空(以空栈接受)'}}}",
                 "    δ:"]
        for (q, a, X), outs in self.transitions.items():
            ai = a if a else "ε"
            for (r, push) in outs:
                ps = "".join(push) if push else "ε"
                lines.append(f"        δ({q}, {ai}, {X}) ∋ ({r}, {ps})")
        return "\n".join(lines)


def tokenize_stack(s, stack_symbols):
    """把压栈串(如 'Bz0')按已知栈符号做「最长匹配」切分为元组('B','z0')。"""
    if s in ("", "ε", "eps", "$"):
        return ()
    syms = sorted(stack_symbols, key=len, reverse=True)
    toks, i = [], 0
    while i < len(s):
        for sym in syms:
            if s.startswith(sym, i):
                toks.append(sym)
                i += len(sym)
                break
        else:
            raise ValueError(f"无法切分压栈串 {s!r}(位置 {i})")
    return tuple(toks)


def parse_pda(text):
    """解析文本格式的 PDA(格式见 examples/task2_pda.txt 与 README)。"""
    states = inp = stk = None
    start_state = start_stack = None
    finals = []
    trans = defaultdict(list)

    for raw in text.splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        low = line.lower()
        if low.startswith("states:"):
            states = line.split(":", 1)[1].split(); continue
        if low.startswith("input:"):
            inp = line.split(":", 1)[1].split(); continue
        if low.startswith("stack:"):
            stk = line.split(":", 1)[1].split(); continue
        if low.startswith("start_state:"):
            start_state = line.split(":", 1)[1].strip(); continue
        if low.startswith("start_stack:"):
            start_stack = line.split(":", 1)[1].strip(); continue
        if low.startswith("final:"):
            finals = line.split(":", 1)[1].split(); continue
        if low.startswith("transitions:"):
            continue
        # 转移行： q,a,X -> r,压栈串
        if "->" in line:
            lhs, rhs = line.split("->", 1)
            q, a, X = [x.strip() for x in lhs.split(",")]
            if "," in rhs:
                r, push = [x.strip() for x in rhs.split(",", 1)]
            else:
                r, push = rhs.strip(), ""
            a = "" if a in ("ε", "eps", "$", "") else a
            trans[(q, a, X)].append((r, tokenize_stack(push, stk)))

    return PDA(states, inp, stk, trans, start_state, start_stack, finals)


# ============================================================
#  由 PDA 构造等价 CFG(空栈接受)
# ============================================================
def pda_to_cfg(pda):
    """由 PDA 构造等价 CFG。

    产生式规则：
      (a) 对每个状态 p：           S -> [q0, Z0, p]
      (b) 对每条转移 δ(q,a,X) ∋ (r, Y1 Y2 ... Yk)：
            若 k = 0：              [q, X, r] -> a            (a 可为 ε)
            若 k >= 1：对所有状态序列 r1,...,rk ∈ Q：
                  [q, X, rk] -> a [r, Y1, r1][r1, Y2, r2]...[r_{k-1}, Yk, rk]
    """
    Q = pda.states

    def trip(q, X, p):
        return f"[{q},{X},{p}]"

    variables = {"S"}
    for q in Q:
        for X in pda.stack_symbols:
            for p in Q:
                variables.add(trip(q, X, p))

    prods = defaultdict(set)

    # (a) 起始产生式
    for p in Q:
        prods["S"].add((trip(pda.start_state, pda.start_stack, p),))

    # (b) 转移产生式
    for (q, a, X), outs in pda.transitions.items():
        for (r, gamma) in outs:
            k = len(gamma)
            if k == 0:
                prods[trip(q, X, r)].add(() if a == "" else (a,))
            else:
                for mids in iproduct(Q, repeat=k):   # (r1, ..., rk)
                    seq = []
                    if a != "":
                        seq.append(a)
                    prev = r
                    for i in range(k):
                        seq.append(trip(prev, gamma[i], mids[i]))
                        prev = mids[i]
                    prods[trip(q, X, mids[-1])].add(tuple(seq))

    return Grammar("S", variables, set(pda.input_symbols), prods)
