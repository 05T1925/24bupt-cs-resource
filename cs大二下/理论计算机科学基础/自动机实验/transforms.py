# -*- coding: utf-8 -*-
"""
transforms.py —— 上下文无关文法的三种化简变换：

    1. eliminate_epsilon  消除 ε 产生式
    2. eliminate_unit     消除单产生式
    3. eliminate_useless  消除无用符号(先去非产生符号，再去不可达符号)

以及把三者按规范顺序串起来的 ``simplify``：ε 产生式 -> 单产生式 -> 无用符号。
该顺序保证最终文法同时不含这三类成分(且不会互相「复活」)。
"""
from collections import defaultdict
from itertools import combinations

from grammar import Grammar


# ============================================================
#  1. 消除 ε 产生式
# ============================================================
def nullable_symbols(g):
    """求可空变量集 Nullable = { A | A =>* ε }，不动点迭代。"""
    nullable = set()
    changed = True
    while changed:
        changed = False
        for A, rhss in g.productions.items():
            if A in nullable:
                continue
            for rhs in rhss:
                # 空右部()，或右部全部由可空变量构成 => A 可空
                if all(s in nullable for s in rhs):
                    nullable.add(A)
                    changed = True
                    break
    return nullable


def eliminate_epsilon(g):
    """消除 ε 产生式。

    思路：先求可空变量集；对每条产生式，枚举「保留/删除其中可空符号」的所有组合，
    生成对应的新产生式，但不加入空产生式。返回 (新文法, nullable集合)。

    注：若起始符号可空(即 ε ∈ L(G))，按经典定理化简后语言为 L(G) 去掉 ε；
    本实验两个测试用例的语言均不含 ε。
    """
    nullable = nullable_symbols(g)
    new = defaultdict(set)
    for A, rhss in g.productions.items():
        for rhs in rhss:
            positions = [i for i, s in enumerate(rhs) if s in nullable]
            # 枚举要删除的「可空符号位置」的所有子集
            for r in range(len(positions) + 1):
                for dele in combinations(positions, r):
                    drop = set(dele)
                    newrhs = tuple(s for i, s in enumerate(rhs) if i not in drop)
                    if newrhs:                      # 不产生空产生式
                        new[A].add(newrhs)
    return Grammar(g.start, g.variables, g.terminals, new), nullable


# ============================================================
#  2. 消除单产生式
# ============================================================
def unit_pairs(g):
    """求单元对：返回 dict, A -> { B | A =>* B 且仅用单产生式 }(含 A 自身)。"""
    pairs = {A: {A} for A in g.variables}
    changed = True
    while changed:
        changed = False
        for A in g.variables:
            for B in list(pairs[A]):
                for rhs in g.productions.get(B, ()):
                    if len(rhs) == 1 and rhs[0] in g.variables:   # B -> C 单产生式
                        C = rhs[0]
                        if C not in pairs[A]:
                            pairs[A].add(C)
                            changed = True
    return pairs


def eliminate_unit(g):
    """消除单产生式。

    对每个单元对 (A, B)，把 B 的所有「非单产生式」加到 A 上。返回 (新文法, 单元对)。
    """
    pairs = unit_pairs(g)
    new = defaultdict(set)
    for A in g.variables:
        for B in pairs[A]:
            for rhs in g.productions.get(B, ()):
                if len(rhs) == 1 and rhs[0] in g.variables:
                    continue                        # 跳过单产生式本身
                new[A].add(rhs)
    return Grammar(g.start, g.variables, g.terminals, new), pairs


# ============================================================
#  3. 消除无用符号
# ============================================================
def generating_symbols(g):
    """求可产生(终结符串)的变量集： X 能推导出某个终结符串(可含 ε)。"""
    gen = set(g.terminals)                # 终结符天然可产生
    changed = True
    while changed:
        changed = False
        for A, rhss in g.productions.items():
            if A in gen:
                continue
            for rhs in rhss:
                if all(s in gen for s in rhs):        # 空右部 => 产生 ε
                    gen.add(A)
                    changed = True
                    break
    return {A for A in gen if A in g.variables}


def reachable_symbols(g):
    """求从起始符号可达的所有符号(变量+终结符)。"""
    reach = {g.start}
    changed = True
    while changed:
        changed = False
        for A in list(reach):
            for rhs in g.productions.get(A, ()):
                for s in rhs:
                    if s not in reach:
                        reach.add(s)
                        changed = True
    return reach


def eliminate_useless(g):
    """消除无用符号：必须先删非产生符号，再删不可达符号。

    返回 (新文法, 产生集, 可达集)。
    """
    # 第一步：删除非产生符号及含它们的产生式
    gen = generating_symbols(g)
    new1 = defaultdict(set)
    for A, rhss in g.productions.items():
        if A not in gen:
            continue
        for rhs in rhss:
            if all((s in gen) or (s in g.terminals) for s in rhs):
                new1[A].add(rhs)
    g1 = Grammar(g.start, gen, g.terminals, new1)

    # 第二步：在此基础上删除不可达符号
    reach = reachable_symbols(g1)
    new2 = defaultdict(set)
    for A, rhss in g1.productions.items():
        if A in reach:
            new2[A] = set(rhss)
    vars2 = {A for A in g1.variables if A in reach}
    terms2 = {t for t in g1.terminals if t in reach}
    return Grammar(g.start, vars2, terms2, new2), gen, reach


# ============================================================
#  串起来：规范化简
# ============================================================
def simplify(g):
    """按 ε 产生式 -> 单产生式 -> 无用符号 的顺序化简，返回最终文法。"""
    g1, _ = eliminate_epsilon(g)
    g2, _ = eliminate_unit(g1)
    g3, _, _ = eliminate_useless(g2)
    return g3
