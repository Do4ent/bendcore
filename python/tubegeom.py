"""Этап 1. Ядро геометрии пути гнутой трубы (эталонная реализация на Python).

Соглашения (из спецификации):
- путь — цепочка прямых участков S и гибов (R по оси, B — угол гиба, A — поворот вокруг касательной перед гибом);
- длина по оси: L = ΣS + ΣR·B (B в радианах) + ΣL_спир;
- касательная: T = R·tan(B/2);
- B меньше порога вырожденного гиба (по умолчанию 0,5°) — гиба нет, участок прямой;
- A > 90° при включённом «Минимизировать поворот» заменяется на A ∓ 180° с гибом в другую сторону.
Ошибки: S < 0 — геометрическая; B > B_max профиля — технологическая.
"""
from __future__ import annotations

import math
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Sequence, Tuple

import numpy as np

DEGENERATE_B_DEG = 0.5   # порог вырожденного гиба (2.4), по умолчанию


@dataclass(frozen=True)
class Straight:
    S: float                      # мм


@dataclass(frozen=True)
class Bend:
    R: float                      # мм, радиус по оси
    B: float                      # градусы, угол гиба (знак — направление)
    A: float = 0.0                # градусы, поворот вокруг касательной перед гибом


@dataclass(frozen=True)
class Spiral:
    R: float                      # мм, радиус витка (= R оправки)
    p: float                      # мм, шаг
    n: float                      # число витков
    direction: int = +1           # +1 правая, -1 левая


Item = Straight | Bend | Spiral


@dataclass
class Issue:
    kind: str                     # "geometric" | "technological" | "warning"
    index: int
    text: str


def tangent_length(R: float, B_deg: float) -> float:
    """T = R·tan(B/2)."""
    return R * math.tan(math.radians(B_deg) / 2.0)


def spiral_length(R: float, p: float, n: float) -> float:
    """L_спир = n·√((2πR)² + p²)."""
    return n * math.sqrt((2 * math.pi * R) ** 2 + p ** 2)


def axis_length(path: Sequence[Item]) -> float:
    """Длина трубы по оси (без вставки концов)."""
    total = 0.0
    for it in path:
        if isinstance(it, Straight):
            total += it.S
        elif isinstance(it, Bend):
            if abs(it.B) >= DEGENERATE_B_DEG:
                total += it.R * math.radians(abs(it.B))
        elif isinstance(it, Spiral):
            total += spiral_length(it.R, it.p, it.n)
    return total


def normalize_rotation(bend: Bend, direction: str = "both") -> Bend:
    """«Минимизировать поворот» (3.1, 3.4): только у профиля «оба» — |A| > 90° → A ∓ 180° с гибом в другую
    сторону. У профиля с одним направлением гиб в другую сторону невозможен, поворот не меняется."""
    if direction != "both":
        return bend
    A = bend.A
    if A > 90.0:
        return Bend(bend.R, -bend.B, A - 180.0)
    if A < -90.0:
        return Bend(bend.R, -bend.B, A + 180.0)
    return bend


def _wrap180(a: float) -> float:
    """A в интервал (−180°, 180°] (3.4)."""
    a = math.fmod(a, 360.0)
    if a <= -180.0:
        a += 360.0
    elif a > 180.0:
        a -= 360.0
    return a


def table_row(bend: Bend, direction: str = "both", minimize: bool = True) -> Tuple[float, float, Optional[str]]:
    """Строка таблицы гибов (3.1, 3.4): (A, B ≥ 0, направление «П»/«Л» или None).
    Знак B в модели: B > 0 — гиб в основную сторону профиля, B < 0 — в другую.
    Профиль с одним направлением («right»/«left»): гиб в другую сторону заменяется поворотом A += 180°.
    Профиль «оба»: колонка «Направление»; при «Минимизировать поворот» |A| > 90° → A ∓ 180°, гиб в другую сторону."""
    if direction == "both":
        b = normalize_rotation(bend, "both") if minimize else bend
        side = "П" if b.B >= 0 else "Л"
        return _wrap180(b.A), abs(b.B), side
    A = bend.A + (180.0 if bend.B < 0 else 0.0)
    return _wrap180(A), abs(bend.B), None


def _rot(v: np.ndarray, axis: np.ndarray, angle_rad: float) -> np.ndarray:
    """Поворот вектора v вокруг единичной оси axis (формула Родрига)."""
    c, s = math.cos(angle_rad), math.sin(angle_rad)
    return v * c + np.cross(axis, v) * s + axis * np.dot(axis, v) * (1 - c)


def trace(path: Sequence[Item], steps_per_bend: int = 64,
          start: Tuple[float, float, float] = (0.0, 0.0, 0.0)) -> np.ndarray:
    """Центральная линия трубы как ломаная (N×3). Спираль — винтовая линия, касательная к соседним прямым."""
    pos = np.array(start, dtype=float)
    heading = np.array([1.0, 0.0, 0.0])
    up = np.array([0.0, 0.0, 1.0])
    pts: List[np.ndarray] = [pos.copy()]
    for it in path:
        if isinstance(it, Straight):
            if it.S < 0:
                raise ValueError("S < 0: геометрическая ошибка")
            if it.S > 0:
                pos = pos + heading * it.S
                pts.append(pos.copy())
        elif isinstance(it, Bend):
            if abs(it.B) < DEGENERATE_B_DEG:
                continue                                  # вырожденный гиб — прямой участок
            # поворот A вокруг касательной перед гибом
            up = _rot(up, heading, math.radians(it.A))
            side = np.cross(up, heading)                  # «левая» сторона плоскости гиба
            sgn = 1.0 if it.B >= 0 else -1.0
            side = side * sgn
            centre = pos + side * it.R
            total = math.radians(abs(it.B))
            n = max(2, int(steps_per_bend * abs(it.B) / 90.0) + 1)
            for k in range(1, n + 1):
                t = total * k / n
                q = _rot(pos - centre, up, sgn * t)
                pts.append(centre + q)
            pos = pts[-1].copy()
            heading = _rot(heading, up, sgn * total)
        elif isinstance(it, Spiral):
            from spiral import spiral_centreline, spiral_axis
            up_perp = up - np.dot(up, heading) * heading
            if np.linalg.norm(up_perp) < 1e-9:
                raise ValueError("не задан верх трубы для оси спирали")
            axis = spiral_axis(heading, it.R, it.p, it.direction, up=up_perp / np.linalg.norm(up_perp))
            spts, tend, _ = spiral_centreline(pos, heading, it.R, it.p, it.n, it.direction,
                                              axis=axis, steps_per_turn=steps_per_bend)
            pts.extend(list(spts[1:]))
            pos = spts[-1].copy()
            heading = tend / np.linalg.norm(tend)
            # 1.2.1: A следующего гиба отсчитывается от плоскости «касательная выхода + ось спирали»;
            # «верх» — нормаль к этой плоскости (гиб при A = 0 лежит в ней), знак — по прежнему «верху».
            n_plane = np.cross(heading, axis)
            if np.linalg.norm(n_plane) < 1e-9:                 # касательная вдоль оси (p ≫ R) — прежний верх
                n_plane = up_perp - np.dot(up_perp, heading) * heading
            up = n_plane / np.linalg.norm(n_plane)
            if np.dot(up, up_perp) < 0:
                up = -up
    return np.array(pts)


def polyline_length(points: np.ndarray) -> float:
    return float(np.sum(np.linalg.norm(np.diff(points, axis=0), axis=1)))


def resample(points: np.ndarray, step: float) -> Tuple[np.ndarray, np.ndarray]:
    """Точки осевой с шагом не больше step и длина по осевой до каждой точки."""
    out = [points[0]]
    for a, b in zip(points[:-1], points[1:]):
        d = float(np.linalg.norm(b - a))
        k = max(1, int(math.ceil(d / step)))
        for j in range(1, k + 1):
            out.append(a + (b - a) * j / k)
    P = np.array(out)
    s = np.concatenate([[0.0], np.cumsum(np.linalg.norm(np.diff(P, axis=0), axis=1))])
    return P, s


def self_intersections(path: Sequence[Item], od: float, max_points: int = 2500) -> List[Tuple[float, float, float]]:
    """Самопересечение осевой (1.5, 4.5): расстояние между осями несмежных участков меньше наружного ∅.
    Несмежность — по длине вдоль осевой больше π·∅/2: на любой дуге с R ≥ ∅/2 точки ближе этого
    отстоят меньше чем на ∅ сами по себе, поэтому соседство через гиб не даёт ложной ошибки.
    Возвращает пары (s1, s2, расстояние)."""
    pts = trace(path)
    total = polyline_length(pts)
    step = max(od / 4.0, total / max_points)
    P, s = resample(pts, step)
    gap = math.pi * od / 2.0
    hits: List[Tuple[float, float, float]] = []
    for i in range(len(P)):
        far = s - s[i] > gap
        if not far.any():
            continue
        d = np.linalg.norm(P[far] - P[i], axis=1)
        j = int(np.argmin(d))
        if d[j] < od - 1e-9:
            hits.append((float(s[i]), float(s[far][j]), float(d[j])))
    return hits


def validate(path: Sequence[Item], B_max: float = 180.0, B_min: float = 0.0, od: Optional[float] = None,
             spiral_min_pitch: Optional[float] = None, spiral_allowed: bool = True) -> List[Issue]:
    """Проверки пути (1.2.1, 1.5, 3.4, 4.5). Классы 4.5: geometric, technological, warning.
    «suggestion» — предложение пользователю (слить коллинеарные участки, 1.5), не нарушение."""
    issues: List[Issue] = []
    prev_kind = None
    for i, it in enumerate(path):
        if isinstance(it, Straight):
            if it.S < 0:
                issues.append(Issue("geometric", i, f"S = {it.S} < 0"))
            if prev_kind == "straight":
                issues.append(Issue("suggestion", i, "коллинеарные участки — слить"))
            prev_kind = "straight"
        elif isinstance(it, Bend):
            if it.R <= 0:
                issues.append(Issue("geometric", i, "R ≤ 0"))
            # 3.4 и решение 18.10.2026: A определён только при ненулевом прямом участке перед гибом;
            # первый гиб A не учитывает
            if any(isinstance(x, Bend) for x in path[:i]) and abs(it.A) > 1e-9:
                prev = path[i - 1] if i > 0 else None
                gap = prev.S if isinstance(prev, Straight) else 0.0
                if gap <= 0:
                    issues.append(Issue("geometric", i,
                                        f"A = {it.A}° задан при нулевом участке между гибами: ввод A запрещён"))
            b = abs(it.B)
            if b < DEGENERATE_B_DEG:
                issues.append(Issue("warning", i, f"вырожденный гиб B = {it.B}°: гиба нет"))
                issues.append(Issue("suggestion", i, "вырожденный гиб между прямыми — слить участки"))
                continue                                     # для слияния гиб считается прямой
            if b > B_max:
                issues.append(Issue("technological", i, f"B = {b}° > B_max = {B_max}°"))
            elif b < B_min:
                issues.append(Issue("technological", i, f"B = {b}° вне диапазона профиля (мин. {B_min}°)"))
            prev_kind = "bend"
        elif isinstance(it, Spiral):
            if not spiral_allowed:
                issues.append(Issue("technological", i, "навивка на строке профиля без разрешения"))
            if spiral_min_pitch is not None and it.p < spiral_min_pitch:
                issues.append(Issue("technological", i, f"шаг спирали {it.p} меньше мин. шага профиля {spiral_min_pitch}"))
            if od is not None and it.p < od:
                issues.append(Issue("technological", i, f"шаг {it.p} меньше наружного ∅ {od}: самопересечение витков"))
            prev_kind = "spiral"
    if od is not None and not any(i.kind == "geometric" for i in issues):
        hits = self_intersections(path, od)
        if hits:
            s1, s2, d = hits[0]
            issues.append(Issue("technological", -1,
                                f"самопересечение осевой: {d:.1f} мм < ∅ {od} (между {s1:.0f} и {s2:.0f} мм по осевой)"))
    return issues


# ---------- Уход конца (3.4) ----------
def _end_pose(path: Sequence[Item]) -> Tuple[np.ndarray, np.ndarray]:
    pts = trace(path, steps_per_bend=8)
    return pts[-1], _unit_dir(pts)


def _unit_dir(pts: np.ndarray) -> np.ndarray:
    d = pts[-1] - pts[-2]
    return d / np.linalg.norm(d)


def manual_angle_tolerance(B: float, ticks: Sequence[float], stops: Sequence[float] = (),
                           on_tick: float = 0.5, on_stop: float = 0.2, between: float = 2.0,
                           eps: float = 1e-6) -> float:
    """Точность угла ручного трубогиба по шкале (2.2.1, 3.4): на упоре, на риске или между рисками."""
    b = abs(B)
    if any(abs(b - s) < eps for s in stops):
        return on_stop
    if any(abs(b - t) < eps for t in ticks):
        return on_tick
    return between


def end_drift(path: Sequence[Item], tol_B: Sequence[float], tol_A: float, tol_S: float,
              tol_pos: float = 3.0, tol_dir_deg: float = 1.0) -> Dict:
    """Уход конца трубы по точности профиля (3.4). Допуски предельные (±3σ):
    худший случай — сумма отклонений, вероятный — среднеквадратичный. Вклад — доля в вероятном уходе по
    гибу и источнику (B, A, S). tol_B — по одному на гиб (по порядку гибов); tol_A, tol_S — общие.
    Начало трубы — база; уход считается для второго конца."""
    path = list(path)
    p0, d0 = _end_pose(path)
    contrib = []            # (гиб №, источник, вклад по положению мм, по направлению °)
    bend_no = 0
    for idx, it in enumerate(path):
        if isinstance(it, Bend) and abs(it.B) >= DEGENERATE_B_DEG:
            bend_no += 1
            # A определён только между двумя гибами (3.1, 3.4): перед первым гибом поворота нет — нет и его ошибки
            sources = [("B", "B", tol_B[bend_no - 1])] + ([("A", "A", tol_A)] if bend_no > 1 else [])
            for src, attr, tol in sources:
                h = 1e-4
                q = list(path)
                q[idx] = Bend(it.R, it.B + h if attr == "B" else it.B, it.A + h if attr == "A" else it.A)
                p1, d1 = _end_pose(q)
                dp = np.linalg.norm(p1 - p0) / h * tol
                # угол через atan2: acos при малых углах теряет точность (≈1e-4 относительно), atan2 — нет
                dd = math.degrees(math.atan2(float(np.linalg.norm(np.cross(d0, d1))), float(np.dot(d0, d1)))) / h * tol
                contrib.append((bend_no, src, dp, dd))
        elif isinstance(it, Straight):
            # подача / метка перед гибом с номером bend_no+1 (у последнего прямого — конец трубы)
            h = 1e-4
            q = list(path)
            q[idx] = Straight(it.S + h)
            p1, _ = _end_pose(q)
            contrib.append((bend_no + 1, "S", float(np.linalg.norm(p1 - p0)) / h * tol_S, 0.0))
    worst_p = sum(c[2] for c in contrib)
    worst_d = sum(c[3] for c in contrib)
    prob_p = math.sqrt(sum(c[2] ** 2 for c in contrib))
    prob_d = math.sqrt(sum(c[3] ** 2 for c in contrib))
    var = sum((c[2] / tol_pos) ** 2 + (c[3] / tol_dir_deg) ** 2 for c in contrib) or 1.0
    shares = sorted(((c[0], c[1], ((c[2] / tol_pos) ** 2 + (c[3] / tol_dir_deg) ** 2) / var) for c in contrib),
                    key=lambda x: -x[2])
    warning = prob_p > tol_pos or prob_d > tol_dir_deg
    return {"worst_pos": worst_p, "worst_dir": worst_d, "probable_pos": prob_p, "probable_dir": prob_d,
            "shares": shares, "warning": warning}


def path_from_table(rows: Sequence[Tuple[float, float, float, float]]) -> List[Item]:
    """Таблица S–A–B: строки (S, R, B, A). Последняя строка может иметь B = 0 (только прямой участок)."""
    path: List[Item] = []
    for S, R, B, A in rows:
        path.append(Straight(S))
        if B != 0:
            path.append(Bend(R, B, A))
    return path
