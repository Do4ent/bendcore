"""Калибровка оснастки (2.2.2, 3.6): журнал поправок по строке оснастки (позиция × профиль × ∅ + марка + состояние
+ стенка, допуск стенки 0,1 мм). Новая запись заменяет действующую, история хранится. Нет записи — расчёт.

Поправка угла B_ст = a·B + b. Пробные гибы (1–5): (B_model, B_ст_задан, B_факт). Нужный угол станка для модели
B_model при пробе: B_model · B_ст_задан / B_факт (пропорционально пружинению этой пробы). По одной пробе — только a
(b = 0); по двум и более на разных углах — a и b методом наименьших квадратов.
Поправка длины ΔL на гиб заменяет расчётное ΔT с каждой стороны гиба (допущение, см. findings).
"""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Dict, List, Optional, Sequence, Tuple

WALL_TOL = 0.1   # мм, допуск стенки для ключа калибровки (2.2.2)


@dataclass
class Trial:
    B_model: float    # угол модели, градусы
    B_set: float      # заданный на станке угол, градусы
    B_fact: float     # измеренный угол, градусы


@dataclass
class Calibration:
    date: str
    operator: str
    od: float
    wall: float
    a: float
    b: float
    dL: Dict[int, float] = field(default_factory=dict)   # номер гиба → измеренное ΔL, мм
    cancelled: bool = False


def fit_ab(trials: Sequence[Trial]) -> Tuple[float, float]:
    """(a, b) по пробам (2.2.2). Пустой набор — ValueError; одна проба — только a; две и более на разных углах — a, b."""
    if not trials:
        raise ValueError("нет пробных гибов")
    xs = [t.B_model for t in trials]
    ys = [t.B_model * t.B_set / t.B_fact for t in trials]      # нужный угол станка для модели
    if len(trials) == 1 or max(xs) - min(xs) < 1e-9:
        return sum(ys) / sum(xs), 0.0
    n = len(xs)
    mx, my = sum(xs) / n, sum(ys) / n
    sxx = sum((x - mx) ** 2 for x in xs)
    a = sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / sxx
    b = my - a * mx
    return a, b


class CalibrationJournal:
    """Журнал по ключу оснастки: (позиция, профиль) — ∅ и марка/состояние проверяются записью."""

    def __init__(self) -> None:
        self._history: Dict[Tuple[str, str], List[Calibration]] = {}

    def record(self, tool: Tuple[str, str], cal: Calibration) -> None:
        self._history.setdefault(tool, []).append(cal)

    def current(self, tool: Tuple[str, str], od: float, wall: float) -> Optional[Calibration]:
        """Действующая запись: последняя не отменённая с тем же ∅ и стенкой в допуске."""
        for cal in reversed(self._history.get(tool, [])):
            if cal.cancelled:
                continue
            if abs(cal.od - od) < 1e-9 and abs(cal.wall - wall) <= WALL_TOL + 1e-9:
                return cal
            return None   # последняя действующая не подходит по ∅ / стенке — расчёт
        return None

    def history(self, tool: Tuple[str, str]) -> List[Calibration]:
        return list(self._history.get(tool, []))


def corrected_angle(B: float, cal: Optional[Calibration]) -> Optional[float]:
    """B_ст по калибровке (a·B + b) или None — тогда расчёт (3.6)."""
    if cal is None:
        return None
    return cal.a * B + cal.b
