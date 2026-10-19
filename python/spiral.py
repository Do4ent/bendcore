"""Этап 4 (часть 5). Спираль (навивка) в 3D — спецификация 1.2.1.

Модель: винтовая линия радиуса R с осью a и шагом q (q = direction·p, направление задаёт знак шага).
Касательная начала: t0 = cosα·u + sinα·a, tanα = q/(2πR), u ⊥ a. Стыковка с входящей прямой
по касательной означает h·a = sinα, поэтому ось задаётся как a = sinα·h + cosα·w, где w ⊥ h
(по умолчанию w — проекция «верха» трубы). Явно заданная ось проверяется на касание (ValueError).
Начальный угол не нужен: положение входа на витке определяется точкой входа.
Длина по оси: n·√((2πR)² + p²).
"""
from __future__ import annotations

import math
from typing import Optional, Sequence, Tuple

import numpy as np


def _unit(v) -> np.ndarray:
    v = np.asarray(v, dtype=float)
    n = np.linalg.norm(v)
    if n < 1e-12:
        raise ValueError("нулевой вектор")
    return v / n


def spiral_axis(heading, R: float, p: float, direction: int = +1,
                up: Sequence[float] = (0.0, 0.0, 1.0)) -> np.ndarray:
    """Ось навивки, касательная к входящей прямой (h·a = sinα)."""
    h = _unit(heading)
    q = direction * p
    s = q / math.hypot(2 * math.pi * R, q)            # sinα
    c = 2 * math.pi * R / math.hypot(2 * math.pi * R, q)  # cosα
    w = _unit(np.asarray(up, dtype=float) - np.dot(up, h) * h)
    return s * h + c * w


def spiral_centreline(start: Sequence[float], heading: Sequence[float], R: float, p: float,
                      n: float, direction: int = +1, axis: Optional[Sequence[float]] = None,
                      steps_per_turn: int = 64) -> Tuple[np.ndarray, np.ndarray, np.ndarray]:
    """Точки центральной линии спирали, касательная в конце и ось навивки."""
    if R <= 0 or n <= 0 or p < 0:
        raise ValueError("R и n должны быть > 0, p ≥ 0")
    h = _unit(heading)
    q = direction * p
    s = q / math.hypot(2 * math.pi * R, q)
    c = 2 * math.pi * R / math.hypot(2 * math.pi * R, q)
    a = spiral_axis(h, R, p, direction) if axis is None else _unit(axis)
    if abs(np.dot(h, a) - s) > 1e-6:
        raise ValueError("прямая не касается спирали: h·a должно быть sinα")
    # u = (h − sinα·a)/cosα — единичный, ⊥ a
    u = _unit(h - s * a)
    e1 = np.cross(u, a)                                  # радиальный единичный в точке входа
    centre = np.asarray(start, dtype=float) - R * e1
    steps = max(8, int(steps_per_turn * n))
    th = np.linspace(0.0, 2 * math.pi * n, steps + 1)
    pts = (centre[None, :] + R * np.cos(th)[:, None] * e1[None, :]
           + R * np.sin(th)[:, None] * u[None, :] + (q * th / (2 * math.pi))[:, None] * a[None, :])
    tend = -R * math.sin(th[-1]) * e1 + R * math.cos(th[-1]) * u + (q / (2 * math.pi)) * a
    return pts, _unit(tend), a


def spiral_axis_length(R: float, p: float, n: float) -> float:
    return n * math.sqrt((2 * math.pi * R) ** 2 + p ** 2)
