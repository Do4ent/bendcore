"""Этап 4. 3D-ядро: тело трубы, пространственный индекс, транзакции, пересчёт.

- sweep_tube: тело трубы — протяжка окружности вдоль центральной линии с минимальным вращением рамки;
- SpatialIndex: сетка для выборки по объёму (масштаб 2500–5000 компонентов);
- Document: транзакции (Undo/Redo) и пересчёт зависимостей с обнаружением цикла (список участников).
Порты, ограничения эскиза и деформация по сетке — дальнейшие шаги этапа 4.
"""
from __future__ import annotations

import math
from collections import defaultdict
from dataclasses import dataclass, field
from typing import Callable, Dict, Iterable, List, Optional, Sequence, Set, Tuple

import numpy as np


# ---------- тело трубы ----------

def _rmf(points: np.ndarray) -> List[np.ndarray]:
    """Рамка с минимальным вращением (double reflection, Wang et al.). Возвращает нормали N_i."""
    n = len(points)
    T = np.zeros((n, 3))
    T[0] = points[1] - points[0]
    T[0] /= np.linalg.norm(T[0])
    t = np.array([0.0, 0.0, 1.0]) if abs(T[0][2]) < 0.9 else np.array([1.0, 0.0, 0.0])
    N = np.cross(T[0], t)
    N /= np.linalg.norm(N)
    frames = [N.copy()]
    for i in range(n - 1):
        v1 = points[i + 1] - points[i]
        c1 = np.dot(v1, v1)
        if c1 < 1e-18:
            frames.append(N.copy())
            continue
        rL = N - (2 / c1) * np.dot(v1, N) * v1
        tL = T[i] - (2 / c1) * np.dot(v1, T[i]) * v1
        T[i + 1] = points[i + 2] - points[i + 1] if i + 2 < n else T[i]
        T[i + 1] /= np.linalg.norm(T[i + 1])
        v2 = T[i + 1] - tL
        c2 = np.dot(v2, v2)
        N = rL if c2 < 1e-18 else rL - (2 / c2) * np.dot(v2, rL) * v2
        N /= np.linalg.norm(N)
        frames.append(N.copy())
    return frames


def sweep_tube(centreline: np.ndarray, od: float, segments: int = 24
               ) -> Tuple[np.ndarray, np.ndarray]:
    """Вершины и треугольники тела трубы (без торцевых крышек). centreline — N×3."""
    if centreline.shape[0] < 2:
        raise ValueError("нужно минимум две точки центральной линии")
    r = od / 2.0
    frames = _rmf(centreline)
    verts = []
    for p, N in zip(centreline, frames):
        B = np.cross(_tangent_at(centreline, p, frames, N), N)
        for k in range(segments):
            a = 2 * math.pi * k / segments
            verts.append(p + r * (math.cos(a) * N + math.sin(a) * B))
    V = np.array(verts)
    faces = []
    for i in range(len(centreline) - 1):
        for k in range(segments):
            a = i * segments + k
            b = i * segments + (k + 1) % segments
            c = a + segments
            d = b + segments
            faces.append((a, c, b))
            faces.append((b, c, d))
    return V, np.array(faces, dtype=np.int64)


def _tangent_at(centreline, p, frames, N):
    # касательная для построения B; достаточно разности соседних точек
    idx = int(np.argmin(np.linalg.norm(centreline - p, axis=1)))
    if idx + 1 < len(centreline):
        t = centreline[idx + 1] - centreline[idx]
    else:
        t = centreline[idx] - centreline[idx - 1]
    return t / np.linalg.norm(t)


def bbox(points: np.ndarray) -> Tuple[np.ndarray, np.ndarray]:
    return points.min(axis=0), points.max(axis=0)


# ---------- пространственный индекс ----------

class SpatialIndex:
    """Равномерная сетка по AABB: быстрый выбор объектов в объёме."""

    def __init__(self, cell: float = 100.0):
        self.cell = cell
        self.items: Dict[str, Tuple[np.ndarray, np.ndarray]] = {}
        self.grid: Dict[Tuple[int, int, int], Set[str]] = defaultdict(set)

    def _cells(self, lo, hi):
        c = self.cell
        a = np.floor(lo / c).astype(int)
        b = np.floor(hi / c).astype(int)
        for x in range(a[0], b[0] + 1):
            for y in range(a[1], b[1] + 1):
                for z in range(a[2], b[2] + 1):
                    yield (x, y, z)

    def insert(self, key: str, lo: np.ndarray, hi: np.ndarray):
        self.remove(key)
        self.items[key] = (lo, hi)
        for cell in self._cells(lo, hi):
            self.grid[cell].add(key)

    def remove(self, key: str):
        if key in self.items:
            lo, hi = self.items.pop(key)
            for cell in self._cells(lo, hi):
                self.grid[cell].discard(key)

    def query(self, lo: np.ndarray, hi: np.ndarray) -> List[str]:
        found: Set[str] = set()
        for cell in self._cells(lo, hi):
            found |= self.grid.get(cell, set())
        out = []
        for k in found:
            a, b = self.items[k]
            if np.all(a <= hi) and np.all(b >= lo):
                out.append(k)
        return sorted(out)


# ---------- транзакции и пересчёт ----------

@dataclass
class Command:
    do: Callable[[], None]
    undo: Callable[[], None]
    label: str = ""


# Этапы пересчёта (6.7): профиль и спецификация → вспомогательная геометрия → эскиз → тело → проверки → чертёж
STAGES = ("spec", "aux", "sketch", "body", "checks", "drawing")


@dataclass
class RecomputeError:
    node: str
    message: str


class Document:
    """Документ: транзакции (одна операция = одна транзакция вместе с пересчётом, 6.6) и пересчёт (6.7):
    порядок зависимостей с учётом этапов, «Автопересчёт» или отложенный режим («нужен пересчёт» + «Обновить»),
    пересчёт только изменённого и зависимого, при ошибке — список ошибок и откат к последнему удачному состоянию."""

    def __init__(self):
        self.undo_stack: List[List[Command]] = []
        self.redo_stack: List[List[Command]] = []
        self._tx: Optional[List[Command]] = None
        self._tx_dirty: Set[str] = set()
        self.deps: Dict[str, Set[str]] = defaultdict(set)     # узел -> от чего зависит
        self.compute: Dict[str, Callable[[Dict[str, object]], object]] = {}
        self.stage: Dict[str, int] = {}
        self.values: Dict[str, object] = {}
        self.auto_recompute: bool = True
        self.dirty: Set[str] = set()
        self.errors: List[RecomputeError] = []

    # транзакции
    def begin(self, label: str = ""):
        if self._tx is not None:
            raise RuntimeError("транзакция уже открыта")
        self._tx = []
        self._tx_dirty = set()

    def apply(self, cmd: Command, touches: Iterable[str] = ()):
        """touches — узлы, которые меняет команда (для пересчёта только затронутого)."""
        if self._tx is None:
            raise RuntimeError("нет открытой транзакции")
        cmd.do()
        self._tx.append(cmd)
        self._tx_dirty |= set(touches)

    def commit(self):
        if self._tx is None:
            return
        if self._tx:
            self.undo_stack.append(self._tx)
            self.redo_stack.clear()
        touched, self._tx, self._tx_dirty = self._tx_dirty, None, set()
        self._after_change(touched)

    def rollback(self):
        if self._tx is None:
            return
        for cmd in reversed(self._tx):
            cmd.undo()
        self._tx = None
        self._tx_dirty = set()

    def undo(self):
        if self.undo_stack:
            tx = self.undo_stack.pop()
            for cmd in reversed(tx):
                cmd.undo()
            self.redo_stack.append(tx)
            self._after_change(set(self.compute))

    def redo(self):
        if self.redo_stack:
            tx = self.redo_stack.pop()
            for cmd in tx:
                cmd.do()
            self.undo_stack.append(tx)
            self._after_change(set(self.compute))

    def _after_change(self, touched: Set[str]):
        for n in touched:
            self.mark_dirty(n)
        if self.auto_recompute and self.dirty:
            self.recompute()

    # пересчёт
    def set_node(self, name: str, deps: Iterable[str], fn: Callable[[Dict[str, object]], object],
                 stage: Optional[str] = None):
        self.deps[name] = set(deps)
        self.compute[name] = fn
        self.stage[name] = STAGES.index(stage) if stage else 0
        self.dirty.add(name)

    def dependents(self, name: str) -> Set[str]:
        out: Set[str] = set()
        frontier = [name]
        while frontier:
            n = frontier.pop()
            for m, ds in self.deps.items():
                if n in ds and m not in out:
                    out.add(m)
                    frontier.append(m)
        return out

    def mark_dirty(self, name: str):
        """Изменённый узел и всё, что от него зависит, получают состояние «нужен пересчёт»."""
        self.dirty |= {name} | self.dependents(name)

    def needs_recompute(self) -> Set[str]:
        return set(self.dirty)

    def order(self) -> List[str]:
        """Порядок зависимостей; при равенстве — по этапам 6.7. При цикле — ошибка со списком участников."""
        order: List[str] = []
        state: Dict[str, int] = {}
        stack: List[str] = []

        def visit(n: str):
            st = state.get(n, 0)
            if st == 1:
                cycle = stack[stack.index(n):] + [n]
                raise ValueError("цикл зависимостей: " + " → ".join(cycle))
            if st == 2:
                return
            state[n] = 1
            stack.append(n)
            for d in sorted(self.deps.get(n, ()), key=lambda d: (self.stage.get(d, 0), d)):
                visit(d)
            stack.pop()
            state[n] = 2
            order.append(n)

        for n in sorted(self.compute, key=lambda n: (self.stage.get(n, 0), n)):
            visit(n)
        return order

    def recompute(self, full: bool = False) -> List[str]:
        """«Обновить»: пересчёт изменённых и зависимых узлов (full — всех). При ошибке узла значения
        откатываются к последнему удачному состоянию, ошибки — в self.errors (диалог со списком, 6.7)."""
        order = self.order()
        todo = [n for n in order if n in self.compute and (full or n in self.dirty)]
        snapshot = dict(self.values)
        self.errors = []
        for n in todo:
            try:
                self.values[n] = self.compute[n](self.values)
            except Exception as e:                         # noqa: BLE001 — ошибка пересчёта узла
                self.errors.append(RecomputeError(n, str(e)))
                self.values = snapshot                     # откат к последнему удачному состоянию
                return [m for m in todo if m != n and todo.index(m) < todo.index(n)]
        self.dirty -= set(todo)
        return todo
