"""Этап 2 (часть 2). Компенсация пружинения — предварительная реализация.

Правила из спецификации:
- B_ст: если задан (теоретическое значение), берётся он; иначе B/K (3.6, выбор пользователя);
- n_ст = n / K (спираль, навивка на оправке);
- длина заготовки с компенсацией = ΣS_ст + ΣR·B_ст + ΣL_спир.ст + E_А + E_Б + Δ_нач + Δ_кон;
- S_ст = S − ΔT слева − ΔT справа (3.6, решение пользователя 18.10.2026).
При K = 1 и пустом B_ст результат совпадает с blank_length (без компенсации).
"""
from __future__ import annotations

import math
from typing import Callable, Optional, Sequence

from blank import BlankResult, EndRule, blank_length, _end_straight, end_allowance
from tubegeom import Bend, Item, Spiral, Straight, Issue, spiral_length


def b_st(bend: Bend, K: float, theoretical: Optional[Callable[[Bend], Optional[float]]] = None) -> float:
    """Угол на трубогибе: теоретический, если задан, иначе B/K (3.6)."""
    if theoretical is not None:
        t = theoretical(bend)
        if t is not None:
            return t
    return bend.B / K


def delta_T(bend: Bend, K: float) -> float:
    """Удлинение касательной с одной стороны гиба после пружинения (3.6): ΔT = (R′ − R)·tan(B/2), R′ = R/K."""
    return (bend.R / K - bend.R) * math.tan(math.radians(abs(bend.B)) / 2)


def compensated_path(path: Sequence[Item], K: float,
                     theoretical: Optional[Callable[[Bend], Optional[float]]] = None) -> list:
    """Путь на станке (3.6): гибы с B_ст, спирали с n_ст = n/K; прямой участок у гиба укорачивается на ΔT
    с каждой стороны: S_ст = S − ΔT слева − ΔT справа (для прямой между двумя гибами — оба ΔT)."""
    items = list(path)
    out: list = []
    for i, it in enumerate(items):
        if isinstance(it, Bend):
            out.append(Bend(it.R, b_st(it, K, theoretical), it.A))
        elif isinstance(it, Spiral):
            out.append(Spiral(it.R, it.p, it.n / K))
        elif isinstance(it, Straight):
            cut = 0.0
            if i > 0 and isinstance(items[i - 1], Bend):
                cut += delta_T(items[i - 1], K)
            if i + 1 < len(items) and isinstance(items[i + 1], Bend):
                cut += delta_T(items[i + 1], K)
            out.append(Straight(it.S - cut))
        else:
            out.append(it)
    return out


def compensated_blank(path: Sequence[Item], start: EndRule, end: EndRule, K: float,
                      dmin: float = 0.0,
                      theoretical: Optional[Callable[[Bend], Optional[float]]] = None) -> BlankResult:
    """Длина заготовки с компенсацией (3.6, 2.5)."""
    if K <= 0:
        raise ValueError("K должен быть > 0")
    station = compensated_path(path, K, theoretical)
    # длина на станке: ΣS_ст + ΣR·B_ст + ΣL_спир.ст (для K=1 и пустого B_ст совпадает с осевой)
    total = 0.0
    for it in station:
        if isinstance(it, Straight):
            total += it.S
        elif isinstance(it, Bend) and abs(it.B) >= 0.5:
            total += it.R * math.radians(abs(it.B))
        elif isinstance(it, Spiral):
            total += spiral_length(it.R, it.p, it.n)
    S_start = _end_straight(path, True)
    S_end = _end_straight(path, False)
    d_start, i1 = end_allowance(S_start, start, dmin)
    d_end, i2 = end_allowance(S_end, end, dmin)
    issues = [i for i in (i1, i2) if i is not None]
    length = total + start.E + end.E + d_start + d_end
    return BlankResult(d_start, d_end, length, issues)


def check_station_angles(path: Sequence[Item], B_max: float, K: float,
                         theoretical: Optional[Callable[[Bend], Optional[float]]] = None) -> list:
    """3.6: угол на трубогибе B_ст больше макс. угла профиля — технологическая ошибка."""
    issues = []
    for i, it in enumerate(path):
        if isinstance(it, Bend) and abs(it.B) >= 0.5:
            b = abs(b_st(it, K, theoretical))
            if b > B_max:
                issues.append(Issue("technological", i, f"B_ст = {b:.1f}° больше макс. угла профиля {B_max}°"))
    return issues
