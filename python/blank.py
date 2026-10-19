"""Этап 2 (часть 1). Припуски концов и длина заготовки — без компенсации пружинения.

Правила из спецификации (2.5, 3.6):
- Авто: Δ = max(Δ_мин, N − (S + E)); участок заготовки на конце = S + E + Δ;
- без подрезки (не допускается): Δ = 0, Δ_мин не применяется, N — жёсткий лимит: S + E ≥ N;
- длина заготовки по модели = ΣS + ΣR·B + ΣL_спир + E_А + E_Б + Δ_нач + Δ_кон.
Компенсация пружинения (B_ст, S_ст, n_ст = n/K) — отдельный модуль, пока не реализован.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import List, Optional, Sequence

from tubegeom import Bend, Item, Spiral, Straight, Issue, axis_length


@dataclass(frozen=True)
class EndRule:
    E: float = 0.0            # глубина вставки в соединение, мм
    N: float = 0.0            # минимальный участок заготовки на конце, мм (из профиля)
    cut_allowed: bool = True  # подрезка допускается


@dataclass
class BlankResult:
    delta_start: Optional[float]     # None — ошибка ручного участка, значение не показывается
    delta_end: Optional[float]
    blank_length: Optional[float]
    issues: List[Issue]


def _end_straight(path: Sequence[Item], from_start: bool) -> float:
    """Прямой участок у торца: первый (от начала) или последний (от конца) до первого/последнего гиба."""
    seq = list(path) if from_start else list(reversed(path))
    total = 0.0
    for it in seq:
        if isinstance(it, Straight):
            total += it.S
        else:
            break
    return total


def end_allowance(S_end: float, rule: EndRule, dmin: float) -> tuple[float, Optional[Issue]]:
    if rule.cut_allowed:
        return max(dmin, rule.N - (S_end + rule.E)), None
    if S_end + rule.E < rule.N:
        return 0.0, Issue("technological", -1,
                          f"без подрезки: S + E = {S_end + rule.E:.1f} < N = {rule.N:.1f}")
    return 0.0, None


def blank_length(path: Sequence[Item], start: EndRule, end: EndRule, dmin: float = 0.0) -> BlankResult:
    S_start = _end_straight(path, True)
    S_end = _end_straight(path, False)
    d_start, i1 = end_allowance(S_start, start, dmin)
    d_end, i2 = end_allowance(S_end, end, dmin)
    issues = [i for i in (i1, i2) if i is not None]
    length = axis_length(path) + start.E + end.E + d_start + d_end
    return BlankResult(d_start, d_end, length, issues)


def check_min_straights(path: Sequence[Item], Lmin: float) -> List[Issue]:
    """Прямой участок между гибами не меньше минимума профиля (2.5). Крайние участки проверяет N."""
    issues: List[Issue] = []
    items = list(path)
    bends_idx = [i for i, it in enumerate(items) if isinstance(it, (Bend, Spiral))]
    for k in range(len(bends_idx) - 1):
        a, b = bends_idx[k], bends_idx[k + 1]
        between = sum(it.S for it in items[a + 1:b] if isinstance(it, Straight))
        if between < Lmin:
            issues.append(Issue("technological", b, f"прямой между гибами {between:.1f} < Lmin {Lmin:.1f}"))
    return issues


# ---------- Сверка с 2.5, 3.5 (18.10.2026) ----------
@dataclass(frozen=True)
class EndSpec:
    """Физический конец трубы (А или Б): глубина вставки, ручной участок заготовки (None — «Авто»)."""
    E: float = 0.0
    manual: Optional[float] = None


def end_minimum(S_end: float, E: float, N: float, dmin: float, cut_allowed: bool) -> float:
    """Минимум участка заготовки на конце (2.5): max(S + E + Δ_мин, N); без подрезки — S + E."""
    if not cut_allowed:
        return S_end + E
    return max(S_end + E + dmin, N)


def end_segment(S_end: float, end: EndSpec, N: float, dmin: float, cut_allowed: bool
                ) -> tuple[Optional[float], List[Issue]]:
    """Припуск Δ конца (2.5). Авто: Δ = max(Δ_мин, N − (S + E)). Вручную: значение не меньше минимума,
    Δ = значение − (S + E); меньшее значение — ошибка поля (технологическая). Без подрезки ручной ввод
    недоступен, Δ = 0, N — жёсткий лимит."""
    issues: List[Issue] = []
    if not cut_allowed:
        if end.manual is not None:
            issues.append(Issue("technological", -1, "подрезка не допускается: ручной участок недоступен"))
        if S_end + end.E < N:
            issues.append(Issue("technological", -1, f"без подрезки: S + E = {S_end + end.E:.1f} < N = {N:.1f}"))
        return 0.0, issues
    if end.manual is None:
        return max(dmin, N - (S_end + end.E)), issues
    minimum = end_minimum(S_end, end.E, N, dmin, True)
    if end.manual < minimum - 1e-9:
        # решение 19.10.2026: ошибка вместо значения, Δ не показывается (ни отрицательного, ни нуля)
        issues.append(Issue("technological", -1, f"участок заготовки {end.manual:.1f} меньше минимума {minimum:.1f}"))
        return None, issues
    return end.manual - (S_end + end.E), issues


def blank_by_order(path: Sequence[Item], end_A: EndSpec, end_B: EndSpec, N_first: float, N_last: float,
                   feed_from: str = "A", both_orders: bool = False, dmin: float = 0.0,
                   cut_allowed: bool = True) -> BlankResult:
    """Заготовка с учётом технологического порядка (2.5, 3.5): N_first — минимум начального конца (с которого
    подают), N_last — конечного. Значения привязаны к физическим концам; при смене порядка N меняются местами.
    «Оба порядка»: припуск на каждом конце — больший из двух порядков; минимум ручного — тоже больший."""
    S_A = _end_straight(path, True)
    S_B = _end_straight(path, False)
    orders = ["A", "B"] if both_orders else [feed_from]
    dA_all, dB_all, issues = [], [], []
    for o in orders:
        NA, NB = (N_first, N_last) if o == "A" else (N_last, N_first)
        dA, iA = end_segment(S_A, end_A, NA, dmin, cut_allowed)
        dB, iB = end_segment(S_B, end_B, NB, dmin, cut_allowed)
        dA_all.append(dA)
        dB_all.append(dB)
        issues += iA + iB
    # Δ с ошибкой (None) не показывается и длина не считается (решение 19.10.2026)
    if any(d is None for d in dA_all + dB_all):
        dA = dB = length = None
    else:
        dA, dB = max(dA_all), max(dB_all)
        length = axis_length(path) + end_A.E + end_B.E + dA + dB
    uniq = []
    for i in issues:
        if i.text not in [u.text for u in uniq]:
            uniq.append(i)
    return BlankResult(dA, dB, length, uniq)


def check_blank_limits(length: float, stock_length: Optional[float], default_stock: Optional[float],
                       machine_max: Optional[float], coil: bool = False) -> List[Issue]:
    """Длина заготовки против длины хлыста (своей или по умолчанию 6.13) и макс. длины трубы на станке (2.5).
    Превышение — технологическая ошибка; хлыст не задан нигде — информация; бухта — по хлысту не проверяется."""
    issues: List[Issue] = []
    if not coil:
        stock = stock_length if stock_length is not None else default_stock
        if stock is None:
            issues.append(Issue("info", -1, "длина хлыста не задана ни в записи, ни по умолчанию"))
        elif length > stock:
            issues.append(Issue("technological", -1, f"заготовка {length:.0f} мм длиннее хлыста {stock:.0f} мм"))
    if machine_max is not None and length > machine_max:
        issues.append(Issue("technological", -1, f"заготовка {length:.0f} мм длиннее макс. длины на станке {machine_max:.0f} мм"))
    return issues
