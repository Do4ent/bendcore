"""Этап 4 (часть 3). Ограничения эскиза, решатель, привязки (1.3, 5.5, 5.6).

- Точки эскиза (3D) и ограничения: фиксация, расстояние между точками, совпадение с точкой признака,
  расстояние до плоскости, направление отрезка ∥ / ⊥ оси, равенство длин.
- Решатель: Левенберг–Марквардт (scipy) от текущего состояния; невязки — по каждому ограничению.
- Диагностика: степени свободы по рангу якобиана; конфликт — ненулевые невязки после сходимости.
- Привязки по приоритету 1.2; ортогональный режим — одна ось за шаг; притяжение угла гиба по профилю (1.2).
"""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Callable, Dict, List, Optional, Sequence, Tuple

import numpy as np
from scipy.optimize import least_squares

Point = Tuple[float, float, float]


@dataclass
class Constraint:
    """Ограничение или размер эскиза (1.3).
    Ограничения: fix, coincide, along_axis, parallel, perp, coplanar, equal_len, symmetry.
    Размеры: dist (L между вершинами), straight_S (прямой участок между точками касания), to_tangent
    (до точки касания), bend_angle (угол гиба), plane_dist, line_dist (до ребра или оси окружения).
    source — приоритет при перетаскивании: user_dim > explicit > auto (> сохранение формы).
    reference — справочный размер: в решении не участвует, показывает измеренное значение."""
    kind: str
    idx: Tuple[int, ...] = ()
    value: float = 0.0
    target: Optional[np.ndarray] = None
    normal: Optional[np.ndarray] = None   # нормаль плоскости или направление оси / ребра
    origin: Optional[np.ndarray] = None   # точка на плоскости, оси или ребре
    label: str = ""
    source: str = "explicit"
    reference: bool = False
    R: float = 0.0                        # радиус гиба для straight_S и to_tangent


WEIGHTS = {"user_dim": 100.0, "explicit": 10.0, "auto": 1.0}
SHAPE_WEIGHT = 1e-3
DIMENSIONS = ("dist", "straight_S", "to_tangent", "bend_angle", "plane_dist", "line_dist")


def _unit(v: np.ndarray) -> np.ndarray:
    n = np.linalg.norm(v)
    return v / n if n > 1e-12 else v


def _bend_angle(P: np.ndarray, a: int, b: int, c: int) -> float:
    """Угол гиба в вершине b, градусы (0 — прямо)."""
    u, v = _unit(P[b] - P[a]), _unit(P[c] - P[b])
    # atan2 вместо arccos: при малых углах arccos теряет точность (как в end_drift и ports)
    return float(np.degrees(np.arctan2(np.linalg.norm(np.cross(u, v)), np.dot(u, v))))


def _tangent(P: np.ndarray, prev: int, i: int, nxt: int, R: float) -> float:
    if prev < 0 or nxt < 0:
        return 0.0
    return R * float(np.tan(np.radians(_bend_angle(P, prev, i, nxt)) / 2))


def measure(P: np.ndarray, c: Constraint) -> float:
    """Текущее значение размера (для справочных и для невязки)."""
    if c.kind == "dist":
        a, b = c.idx
        return float(np.linalg.norm(P[a] - P[b]))
    if c.kind == "straight_S":                       # idx: (пред., i, j, след.) — −1 у концов пути
        p, i, j, q = c.idx
        return float(np.linalg.norm(P[j] - P[i])) - _tangent(P, p, i, j, c.R) - _tangent(P, i, j, q, c.R)
    if c.kind == "to_tangent":                       # idx: (точка, пред., вершина гиба, след.)
        a, p, i, q = c.idx
        tp = P[i] - _unit(P[i] - P[p]) * _tangent(P, p, i, q, c.R)
        return float(np.linalg.norm(P[a] - tp))
    if c.kind == "bend_angle":
        return _bend_angle(P, *c.idx)
    if c.kind == "plane_dist":
        return float(np.dot(P[c.idx[0]] - c.origin, _unit(c.normal)))
    if c.kind == "line_dist":
        return float(np.linalg.norm(np.cross(P[c.idx[0]] - c.origin, _unit(c.normal))))
    raise ValueError(f"{c.kind} — не размер")


def _rows(P: np.ndarray, c: Constraint) -> List[float]:
    if c.kind in ("fix", "coincide"):
        return list(P[c.idx[0]] - c.target)
    if c.kind in DIMENSIONS:
        return [measure(P, c) - c.value]
    if c.kind in ("parallel", "along_axis"):         # «вдоль оси» — параллельно оси X/Y/Z или заданной
        a, b = c.idx
        return [float(np.linalg.norm(np.cross(P[b] - P[a], _unit(c.normal))))]
    if c.kind == "perp":
        a, b = c.idx
        return [float(np.dot(P[b] - P[a], _unit(c.normal)))]
    if c.kind == "equal_len":
        a, b, cc, d = c.idx
        return [float(np.linalg.norm(P[a] - P[b]) - np.linalg.norm(P[cc] - P[d]))]
    if c.kind == "coplanar":                         # все точки в плоскости первых трёх
        a, b, cc, *rest = c.idx
        n = _unit(np.cross(P[b] - P[a], P[cc] - P[a]))
        return [float(np.dot(P[k] - P[a], n)) for k in rest]
    if c.kind == "symmetry":                         # точки a, b симметричны относительно плоскости
        a, b = c.idx
        n = _unit(c.normal)
        return [float(np.dot((P[a] + P[b]) / 2 - c.origin, n)),
                float(np.linalg.norm(np.cross(P[b] - P[a], n)))]
    raise ValueError(f"неизвестное ограничение: {c.kind}")


def _active(cons: Sequence[Constraint]) -> List[Constraint]:
    return [c for c in cons if not c.reference]


def _residuals(x: np.ndarray, n: int, cons: Sequence[Constraint], weighted: bool = False,
               x_shape: Optional[np.ndarray] = None) -> np.ndarray:
    P = x.reshape(n, 3)
    r: List[float] = []
    for c in cons:
        w = WEIGHTS.get(c.source, 10.0) if weighted else 1.0
        r += [w * v for v in _rows(P, c)]
    if x_shape is not None:
        r += list(SHAPE_WEIGHT * (x - x_shape))
    return np.array(r, dtype=float)


@dataclass
class SolveResult:
    status: str                     # определено | недоопределено | конфликт
    dof: int
    points: np.ndarray
    residual: float
    conflicts: List[str] = field(default_factory=list)
    reference_values: Dict[str, float] = field(default_factory=dict)


def solve(points: Sequence[Point], cons: Sequence[Constraint], preserve_shape: bool = False) -> SolveResult:
    """Решатель (1.3): итерационный (Левенберг–Марквардт / доверительная область) от текущего состояния.
    preserve_shape — режим перетаскивания: невязки с весами приоритета и слабое сохранение формы."""
    P0 = np.array(points, dtype=float)
    n = len(P0)
    x0 = P0.reshape(-1)
    act = _active(cons)
    x_shape = x0.copy() if preserve_shape else None
    m = len(_residuals(x0, n, act, True, x_shape))
    method = "lm" if m >= x0.size else "trf"
    if m == 0:
        res_x, jac = x0, np.zeros((0, x0.size))
    else:
        res = least_squares(_residuals, x0, args=(n, act, preserve_shape, x_shape), method=method,
                            xtol=1e-12, ftol=1e-12)
        res_x = res.x
        jac = _jacobian(res_x, n, act)
    P = res_x.reshape(n, 3)
    r = _residuals(res_x, n, act)
    rank = np.linalg.matrix_rank(jac, tol=1e-8) if jac.size else 0
    dof = 3 * n - rank
    total = float(np.linalg.norm(r)) if r.size else 0.0
    refs = {(c.label or c.kind): measure(P, c) for c in cons if c.reference}
    if total > 1e-6:
        bad = [c.label or c.kind for c in act if np.linalg.norm(_rows(P, c)) > 1e-6]
        return SolveResult("конфликт", dof, P, total, bad, refs)
    return SolveResult("определено" if dof == 0 else "недоопределено", dof, P, total, [], refs)


def _jacobian(x: np.ndarray, n: int, cons: Sequence[Constraint], h: float = 1e-6) -> np.ndarray:
    """Якобиан невязок ограничений без весов и без сохранения формы — для подсчёта степеней свободы.
    Центральные разности (ошибка ~h², а не ~h): ранг в C++ считается так же."""
    r0 = _residuals(x, n, cons)
    J = np.zeros((len(r0), x.size))
    for k in range(x.size):
        xp, xm = x.copy(), x.copy()
        xp[k] += h
        xm[k] -= h
        J[:, k] = (_residuals(xp, n, cons) - _residuals(xm, n, cons)) / (2 * h)
    return J


# ---------- диалог «Конфликт» (1.3): «Удалить» и «Сделать справочным» ----------
def delete_constraint(cons: List[Constraint], label: str) -> List[Constraint]:
    return [c for c in cons if c.label != label]


def make_reference(cons: List[Constraint], label: str) -> List[Constraint]:
    out = []
    for c in cons:
        if c.label == label:
            if c.kind not in DIMENSIONS:
                raise ValueError("справочным можно сделать только размер")
            c = Constraint(**{**c.__dict__, "reference": True})
        out.append(c)
    return out


# ---------- привязки ----------

def snap_point(target: Point, ports: Sequence[Point] = (), features: Sequence[Point] = (),
               radius: float = 5.0, vertices: Sequence[Point] = (),
               guides: Sequence[Tuple[Point, Point]] = (), grid: Optional[Tuple[Point, Point, Point, float]] = None
               ) -> Tuple[Point, str]:
    """Приоритет привязок (1.2): вершина или порт → геометрия окружения (ось отверстия, ребро, центр) →
    направляющие (оси X/Y/Z, ∥/⊥ предыдущему сегменту) → сетка → свободная точка в плоскости.
    guides — прямые (точка, направление); grid — (начало, ось u, ось v, шаг) активной плоскости."""
    t = np.array(target, dtype=float)
    for p in ports:
        if np.linalg.norm(np.array(p) - t) <= radius:
            return tuple(p), "порт"
    for p in vertices:
        if np.linalg.norm(np.array(p) - t) <= radius:
            return tuple(p), "вершина"
    for f in features:
        if np.linalg.norm(np.array(f) - t) <= radius:
            return tuple(f), "признак"
    best = None
    for origin, direction in guides:
        o = np.array(origin, dtype=float)
        d = np.array(direction, dtype=float)
        d /= np.linalg.norm(d)
        q = o + d * float(np.dot(t - o, d))
        dist = float(np.linalg.norm(q - t))
        if dist <= radius and (best is None or dist < best[0]):
            best = (dist, q)
    if best is not None:
        return tuple(best[1]), "направляющая"
    if grid is not None:
        o, u, v, step = (np.array(grid[0], float), np.array(grid[1], float), np.array(grid[2], float), grid[3])
        a = round(float(np.dot(t - o, u)) / step) * step
        b = round(float(np.dot(t - o, v)) / step) * step
        q = o + u * a + v * b
        if np.linalg.norm(q - t) <= radius:
            return tuple(q), "сетка"
    return tuple(t), "свободно"


def ortho_step(prev: Point, target: Point) -> Point:
    """Ортогональный режим: за шаг меняется одна ось (доминирующая)."""
    p = np.array(prev, dtype=float)
    t = np.array(target, dtype=float)
    d = np.abs(t - p)
    axis = int(np.argmax(d))
    out = p.copy()
    out[axis] = t[axis]
    return tuple(out)


def snap_angle(angle_deg: float, allowed: Sequence[float]) -> float:
    """Ближайшее значение из списка (вспомогательная функция)."""
    return min(allowed, key=lambda a: abs(a - angle_deg))


SCALE_WARNING = "Угол вне шкалы: точность ±2°"


def snap_bend_angle(angle_deg: float, manual: bool, preferred: Sequence[float] = (), ticks: Sequence[float] = (),
                    stops: Sequence[float] = (), step: Optional[float] = None, typed: bool = False,
                    hard_radius: float = 7.5, soft_radius: float = 2.0, eps: float = 1e-6
                    ) -> Tuple[float, Optional[str]]:
    """Притяжение угла гиба при построении (1.2).
    Ручной трубогиб: жёсткое притяжение к предпочтительным углам (6.13) и рискам шкалы; угол не на риске —
    предупреждение «Угол вне шкалы: точность ±2°» (3.4).
    Механизированный: мягкое притяжение к предпочтительным углам, упорам и шагу как подсказка; без предупреждений;
    если шага и упоров нет и угол не рядом с предпочтительным — угол свободный.
    Ручной ввод значения (typed) обходит притяжение. Радиусы притяжения спецификацией не заданы — параметры."""
    a = angle_deg
    if not typed:
        if manual:
            cands = list(preferred) + list(ticks)
            if cands:
                c = snap_angle(a, cands)
                if abs(c - a) <= hard_radius:
                    a = c
        else:
            cands = list(preferred) + list(stops)
            if step:
                cands.append(round(a / step) * step)
            if cands:
                c = snap_angle(a, cands)
                if abs(c - a) <= soft_radius:
                    a = c
    warning = None
    if manual and ticks and not any(abs(a - t) < eps for t in ticks):
        warning = SCALE_WARNING
    return a, warning


# ---------- 9.3: дуга на входе → вершина ----------
ARC_TOL_R = 0.1   # мм, допуск совпадения радиуса (6.13 задаёт «допуск совпадения»; значение 0,1 мм — рабочее, как для ∅)


def entry_arc_to_vertex(R_arc: float, R_profile: float, tol_R: float = ARC_TOL_R) -> dict:
    """Дуга на входе трубы становится вершиной с радиусом профиля (1.1, 9.3).
    Внутри допуска — радиус профиля без конфликта, отклонение пишется в отчёт. Вне допуска — статус «конфликт»,
    по умолчанию «оставить тело»; выбор: принять R профиля, создать пользовательский трубогиб с R модели, оставить тело."""
    dev = abs(R_arc - R_profile)
    if dev <= tol_R + 1e-12:
        return {"status": "ok", "R": R_profile, "report": f"R дуги отличается на {dev:.3f} мм, принят R профиля"}
    return {"status": "conflict", "R": None, "default": "keep_body",
            "options": ["accept_profile", "custom_bend_from_model", "keep_body"],
            "report": f"конфликт радиуса: R модели {R_arc} ≠ R профиля {R_profile} (допуск {tol_R} мм)"}
