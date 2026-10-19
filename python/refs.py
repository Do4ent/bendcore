"""Этап 4 (часть 4). Ссылки на окружение, обновление и адаптивность (5.7).

Состояния ссылки: валидна / устарела / не загружена / потеряна.
«Обновить компонент»: все ссылки в норме; либо вершины сдвинулись (пересчёт трубы);
либо потерянные ссылки — список для диалога.
Режим трубы: «Адаптивна» (пересчитывается при смещении окружения) или «Заморожена»
(значения из кэша, ссылки игнорируются).
"""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Dict, List, Optional, Sequence, Tuple

import numpy as np

from sketch import Constraint, solve

VALID, OUTDATED, NOT_LOADED, LOST = "валидна", "устарела", "не загружена", "потеряна"


@dataclass
class Ref:
    ref_id: str                    # ссылка на признак окружения
    vertex: int                    # индекс вершины трубы, привязанной к признаку
    cached: Tuple[float, float, float]
    state: str = VALID


@dataclass
class Tube:
    points: np.ndarray             # вершины трубы (N×3)
    refs: List[Ref] = field(default_factory=list)
    mode: str = "адаптивна"        # адаптивна | заморожена


@dataclass
class UpdateResult:
    status: str                    # все в норме | вершины сдвинулись | потеряны ссылки | заморожена
    moved: List[str] = field(default_factory=list)
    lost: List[str] = field(default_factory=list)
    points: Optional[np.ndarray] = None


ORIGINS_FROZEN = ("recognized", "imported", "dragged", "table")


def initial_mode(origin: str) -> str:
    """Начальный режим трубы (5.7, 9.3): созданная в сборке — «адаптивна»; распознанная, импортированная,
    скопированная перетаскиванием и созданная в режиме «Таблица» — «заморожена»."""
    if origin == "assembly":
        return "адаптивна"
    if origin in ORIGINS_FROZEN:
        return "заморожена"
    raise ValueError(f"неизвестное происхождение трубы: {origin}")


def update_component(tube: Tube, features: Dict[str, Tuple[float, float, float]],
                     tol: float = 1e-6, unloaded: frozenset = frozenset()) -> UpdateResult:
    """Переоценка ссылок по текущим признакам окружения. features: ref_id -> точка признака.
    unloaded — ссылки на окружение, которое не загружено (состояние «не загружена», 5.7): не пересчитываются,
    их вершины остаются на месте, в потерянные не попадают."""
    if tube.mode == "заморожена":
        return UpdateResult("заморожена", points=tube.points)
    moved: List[str] = []
    lost: List[str] = []
    cons: List[Constraint] = []
    skipped = False
    for ref in tube.refs:
        if ref.ref_id in unloaded:
            ref.state = NOT_LOADED
            skipped = True
            continue
        if ref.ref_id not in features:
            ref.state = LOST
            lost.append(ref.ref_id)
            continue
        target = np.array(features[ref.ref_id], dtype=float)
        if np.linalg.norm(target - np.array(ref.cached)) > tol:
            ref.state = OUTDATED
            moved.append(ref.ref_id)
        else:
            ref.state = VALID
        cons.append(Constraint("coincide", (ref.vertex,), target=target, label=ref.ref_id))
    if lost:
        return UpdateResult("потеряны ссылки", moved=moved, lost=lost, points=tube.points)
    if not moved:
        return UpdateResult("не загружена" if skipped else "все в норме", points=tube.points)
    # пересчёт: вершины с ссылками садятся в новые признаки, остальное (в т.ч. не загруженные) — по решателю
    active = {r.vertex for r in tube.refs if r.ref_id not in unloaded}
    fixed_others = [Constraint("fix", (i,), target=tube.points[i], label=f"v{i}")
                    for i in range(len(tube.points)) if i not in active]
    res = solve([tuple(p) for p in tube.points], cons + fixed_others)
    for ref in tube.refs:
        if ref.ref_id in unloaded:
            continue
        ref.cached = tuple(features[ref.ref_id])
        ref.state = VALID
    tube.points = res.points
    return UpdateResult("вершины сдвинулись", moved=moved, points=res.points)
