"""Этап 4 (часть 2). Порты (спецификация 5.3).

Порт: позиция, направление наружу, требуемый ∅, допуск положения и направления конца трубы,
минимальный вылет прямого участка (геометрическое требование), справочный тип соединения,
состояние ссылки и занятости.

Проверки присоединения трубы к порту:
- порт занят другой трубой — отказ присоединения («refused»), не класс нарушения 4.5;
- ссылка порта потеряна — состояние порта («ref_lost», значок разорванной цепи, 5.2, 6.2), не класс 4.5;
- ∅ порта не совпадает с ∅ трубы — предупреждение (жёлтый треугольник, 5.3, 4.5);
- вылет прямого участка у порта меньше минимального — технологическая ошибка (5.3, 4.5); у конца А — первый
  прямой, у конца Б — последний (5.4);
- конец трубы в сборке вне допуска положения или направления порта — предупреждение.
"""
from __future__ import annotations

import math
from dataclasses import dataclass, field
from typing import List, Optional, Sequence

import numpy as np

from tubegeom import Bend, Issue, Item, Straight, trace

DEFAULT_TOL_POS = 3.0      # мм, допуск положения конца (6.13)
DEFAULT_TOL_ANG = 1.0      # градусы, допуск направления (6.13)


@dataclass
class Port:
    name: str
    pos: tuple                       # (x, y, z), мм
    direction: tuple                 # вектор наружу
    od: float                        # требуемый ∅ трубы, мм
    min_straight: float = 0.0        # минимальный вылет прямого участка, мм
    tol_pos: float = DEFAULT_TOL_POS
    tol_ang: float = DEFAULT_TOL_ANG
    conn_type: str = ""              # справочный атрибут
    ref_ok: bool = True              # ссылка на признак окружения валидна
    occupied_by: Optional[str] = None  # id трубы, если порт занят
    associative: bool = True         # порт по точке + направлению + ∅ — неассоциативный (5.3)
    socket_depth: Optional[float] = None   # измеренная глубина раструба фитинга (9.11)

    def flip(self) -> None:
        """«Развернуть» (5.3)."""
        self.direction = tuple(-np.array(self.direction, dtype=float))

    def unit_dir(self) -> np.ndarray:
        d = np.array(self.direction, dtype=float)
        return d / np.linalg.norm(d)


def _angle_deg(a: np.ndarray, b: np.ndarray) -> float:
    # atan2 вместо acos: при малых углах acos теряет точность (как в end_drift)
    return math.degrees(math.atan2(float(np.linalg.norm(np.cross(a, b))), float(np.dot(a, b))))


def first_straight(path: Sequence[Item]) -> float:
    total = 0.0
    for it in path:
        if isinstance(it, Straight):
            total += it.S
        else:
            break
    return total


def last_straight(path: Sequence[Item]) -> float:
    total = 0.0
    for it in reversed(list(path)):
        if isinstance(it, Straight):
            total += it.S
        else:
            break
    return total


def check_connection(port: Port, path: Sequence[Item], od: float, tube_id: str, end: str = "A",
                     end_pos: Optional[Sequence[float]] = None, end_dir: Optional[Sequence[float]] = None
                     ) -> List[Issue]:
    """Проверки присоединения конца трубы (А — начало пути, Б — конец) к порту.
    end_pos, end_dir — положение конца трубы в сборке и направление от порта в трубу; без них положение
    и направление не проверяются (труба строится от порта и совпадает с ним по построению)."""
    issues: List[Issue] = []
    if port.occupied_by is not None and port.occupied_by != tube_id:
        issues.append(Issue("refused", -1, f"порт {port.name} занят трубой {port.occupied_by}"))
    if port.associative and not port.ref_ok:
        issues.append(Issue("ref_lost", -1, f"порт {port.name}: ссылка потеряна"))
    if abs(port.od - od) > 1e-9:
        issues.append(Issue("warning", -1, f"∅ порта {port.od} ≠ ∅ трубы {od}"))
    straight = first_straight(path) if end == "A" else last_straight(path)
    if straight < port.min_straight:
        issues.append(Issue("technological", -1,
                            f"вылет {straight:.1f} < минимального {port.min_straight:.1f} у порта {port.name}"))
    if end_pos is not None:
        dev_pos = float(np.linalg.norm(np.array(end_pos, dtype=float) - np.array(port.pos, dtype=float)))
        if dev_pos > port.tol_pos:
            issues.append(Issue("warning", -1, f"конец трубы смещён на {dev_pos:.2f} мм (допуск {port.tol_pos})"))
    if end_dir is not None:
        ang = _angle_deg(np.array(end_dir, dtype=float), port.unit_dir())
        if ang > port.tol_ang:
            issues.append(Issue("warning", -1, f"направление отклонено на {ang:.2f}° (допуск {port.tol_ang}°)"))
    return issues


def auto_name(component: str, existing: Sequence[str]) -> str:
    """Имя порта по умолчанию «П1…» с префиксом компонента (5.3)."""
    k = 1
    while f"{component}.П{k}" in existing:
        k += 1
    return f"{component}.П{k}"


def socket_hint(port: Port, E_current: float, decided: bool = False) -> Optional[Issue]:
    """Подсказка «По раструбу: N мм — Применить» (9.11, 2.5): до применения или отклонения — «информация»."""
    if port.socket_depth is None or decided or abs(E_current - port.socket_depth) < 1e-9:
        return None
    return Issue("info", -1, f"По раструбу: {port.socket_depth:g} мм — Применить")


def start_from_port(port: Port):
    """Начало трассировки «отсюда»: позиция и направление порта."""
    return np.array(port.pos, dtype=float), port.unit_dir()


def candidates(ports: Sequence[Port], od: float) -> List[Port]:
    """Свободные порты подходящего ∅ — подсвечиваются как цели трассировки (5.3)."""
    return [p for p in ports if p.occupied_by is None and p.ref_ok and abs(p.od - od) < 1e-9]


def connect(port: Port, tube_id: str, path: Sequence[Item], od: float) -> List[Issue]:
    """Присоединение: проверки, при отсутствии ошибок порт занимается трубой."""
    issues = check_connection(port, path, od, tube_id)
    if not any(i.kind == "refused" for i in issues):
        port.occupied_by = tube_id
    return issues
