"""Этап 8. Карта гибки (3.6), XYZ / LRA (7.6), контрольный пересчёт округления, гейт выпуска (7.7).

Внутри — мм и градусы; округление и единицы профиля применяются только при выводе (3.6, 6.15).
Первый срез: строки карты, метки и подачи, XYZ / LRA без спирали, контрольный пересчёт, блокировка выдачи.
Калибровка оснастки (2.2.2) и мастер выдачи — вне этого среза.
"""
from __future__ import annotations

import math
from dataclasses import dataclass
from dataclasses import field
from typing import Dict, List, Optional, Sequence, Tuple

import numpy as np

from calib import Calibration as _Cal  # noqa: F401  (тип для аннотаций)

from compensation import b_st, compensated_path
from tubegeom import Bend, Issue, Item, Spiral, Straight, table_row, trace

BLOCKING = ("geometric", "technological")


def rounded(value: float, step: float) -> float:
    """Округление к шагу профиля; step ≤ 0 — без округления (3.6)."""
    if step <= 0:
        return value
    return round(value / step) * step


@dataclass
class MapRow:
    no: int                 # номер гиба
    S: float                # прямой участок перед гибом, модель
    S_st: float             # то же на станке (3.6)
    A: float                # поворот, из таблицы гибов
    B: float                # угол модели
    B_st: float             # угол на трубогибе
    source: str             # «расчёт» | «вручную»
    feed: float             # подача: положение начала гиба от торца заготовки (до подрезки)
    mark: float             # положение метки (то же, что подача, без поправки на базу)


def station_straights(path: Sequence[Item], K: float, dL: Optional[Dict[int, float]] = None) -> Dict[int, float]:
    """S_ст каждого прямого (индекс в path): S − ΔT слева − ΔT справа (3.6). dL — измеренные ΔL по номеру гиба
    (калибровка): заменяют ΔT с каждой стороны гиба."""
    from compensation import delta_T
    dL = dL or {}
    bend_no = {}
    k = 0
    for i, it in enumerate(path):
        if isinstance(it, Bend):
            k += 1
            bend_no[i] = k
    out: Dict[int, float] = {}
    for i, it in enumerate(path):
        if not isinstance(it, Straight):
            continue
        cut = 0.0
        for j in (i - 1, i + 1):
            if 0 <= j < len(path) and isinstance(path[j], Bend):
                n = bend_no[j]
                cut += dL[n] if n in dL else delta_T(path[j], K)
        out[i] = it.S - cut
    return out


def bend_rows(path: Sequence[Item], K: float, manual: Optional[Dict[int, float]] = None,
              step_B: float = 0.0, offset_start: float = 0.0,
              cal: Optional["calib.Calibration"] = None) -> List[MapRow]:
    """Строки карты гибки. Приоритет угла на трубогибе (2.2.2, 3.6): ручной ввод → калибровка оснастки → расчёт.
    manual — ручные B_ст по номеру гиба (1-based). cal — действующая калибровка (calib.Calibration) или None.
    offset_start — Δ_нач + E_А: участок до первого прямого (метки и подачи отсчитываются от торца заготовки)."""
    import calib as _calib
    manual = manual or {}
    straights = station_straights(path, K, cal.dL if cal else None)
    rows: List[MapRow] = []
    pos = offset_start
    no = 0
    prev_straight = 0.0
    for i, it in enumerate(path):
        if isinstance(it, Straight):
            prev_straight = it.S
            continue
        if not isinstance(it, Bend):
            continue
        no += 1
        s_st = straights.get(i - 1, 0.0) if i > 0 and isinstance(path[i - 1], Straight) else 0.0
        S_model = prev_straight
        pos += s_st
        corr = _calib.corrected_angle(abs(it.B), cal)
        if no in manual:
            bs, src = manual[no], "вручную"
        elif corr is not None:
            bs, src = rounded(corr, step_B), "калибровка"
        else:
            bs, src = rounded(b_st(it, K), step_B), "расчёт"
        A, B_abs, _ = table_row(it, "both", minimize=True)
        rows.append(MapRow(no, S_model, s_st, A, B_abs, bs, src, pos, pos))
        pos += math.radians(abs(bs)) * it.R        # дуга на оснастке R·B_ст
        prev_straight = 0.0
    return rows


ORDER_PATH, ORDER_TECH = "по пути", "технологический"


def bend_sequence(rows: Sequence[MapRow], order: str = ORDER_PATH) -> List[MapRow]:
    """Порядок выполнения гибов (3.6, схема с номерами). Номера фиксированы по пути; в технологическом порядке
    строки идут в обратной последовательности (например, 3, 2, 1). Номера в строках не меняются."""
    if order == ORDER_PATH:
        return list(rows)
    if order == ORDER_TECH:
        return list(reversed(rows))
    raise ValueError(f"неизвестный порядок: {order}")


def marks_mid_arc(rows: List[MapRow], R_list: Sequence[float]) -> List[float]:
    """Метки от середины дуги (вариант базы профиля, 3.6): положение начала + половина дуги."""
    return [r.feed + math.radians(r.B_st) * R / 2 for r, R in zip(rows, R_list)]


def has_spiral(path: Sequence[Item]) -> bool:
    return any(isinstance(it, Spiral) for it in path)


def xyz_points(path: Sequence[Item], E_A: float = 0.0, E_B: float = 0.0) -> np.ndarray:
    """Координаты вершин детали (7.6): физические торцы — с учётом глубины вставки E (концы А и Б первой и последней
    строкой), вершины гибов — по порядку. Спираль не поддерживается (7.6)."""
    if has_spiral(path):
        raise ValueError("XYZ недоступны: спираль в трубе (7.6)")
    pts = trace(path, steps_per_bend=1)
    keep = [pts[0]]
    for p in pts[1:-1]:
        if np.linalg.norm(p - keep[-1]) > 1e-9:
            keep.append(p)
    keep.append(pts[-1])
    a = keep[0] - E_A * _unit(keep[1] - keep[0])
    b = keep[-1] + E_B * _unit(keep[-1] - keep[-2])
    return np.vstack([a, keep[1:-1], b])


def _unit(v: np.ndarray) -> np.ndarray:
    n = np.linalg.norm(v)
    return v / n if n > 1e-12 else v


def lra_rows(path: Sequence[Item]) -> List[Tuple[float, float, float, float]]:
    """LRA (7.6): (L — прямой участок, R, A — поворот, B — угол) по порядку; спираль недоступна («LRA не описывает спираль»)."""
    if has_spiral(path):
        raise ValueError("LRA не описывает спираль (3.6)")
    out = []
    for it in path:
        if isinstance(it, Straight):
            out.append((it.S, 0.0, 0.0, 0.0))
        elif isinstance(it, Bend):
            A, B, _ = table_row(it, "both", minimize=True)
            out.append((0.0, it.R, A, B))
    return out


MAX_DEV_MM = 3.0   # мм, макс. отклонение геометрии для контрольного пересчёта (решение пользователя 18.10.2026)


def control_recalc(path: Sequence[Item], K: float, rows: List[MapRow], step_B: float,
                   max_dev: float = MAX_DEV_MM) -> List[Issue]:
    """Контрольный пересчёт (3.6): из округлённых B_ст восстанавливаем угол модели (B = B_ст·K), сравниваем с моделью:
    по каждому гибу допуск — половина шага округления; накопленное смещение торца — с макс. отклонением геометрии.
    Превышение — предупреждение, вывод не блокируется."""
    issues: List[Issue] = []
    tol = step_B / 2 if step_B > 0 else 1e-6
    bends = [it for it in path if isinstance(it, Bend)]
    rebuilt: List[Item] = []
    k = 0
    for it in path:
        if isinstance(it, Bend):
            r = rows[k]
            k += 1
            B_back = r.B_st * K
            if abs(abs(B_back) - abs(it.B)) > tol + 1e-9:
                issues.append(Issue("warning", k, f"гиб {k}: округление даёт B = {B_back:.2f}° при модели {abs(it.B)}°"))
            rebuilt.append(Bend(it.R, math.copysign(abs(B_back), it.B), it.A))
        else:
            rebuilt.append(it)
    end_model = trace(path)[-1]
    end_rebuilt = trace(rebuilt)[-1]
    dev = float(np.linalg.norm(end_model - end_rebuilt))
    if dev > max_dev:
        issues.append(Issue("warning", -1, f"смещение торца после округления {dev:.2f} мм больше макс. отклонения {max_dev} мм"))
    return issues


def output_blocked(issues: Sequence[Issue]) -> bool:
    """Вывод карты блокируется технологической или геометрической ошибкой (3.6, 4.5)."""
    return any(i.kind in BLOCKING for i in issues)


def release_gate(issues: Sequence[Issue], unresolved_refs: int = 0, sheet_changed: bool = False,
                 outdated: bool = False, mode: str = "strict") -> Tuple[bool, List[str]]:
    """Выпуск чертежа (7.7): (можно выпускать, предупреждения). Блокируют технологические и геометрические ошибки,
    потерянные ссылки в размерах, лист развёрток «состав изменён» — всегда. «Строгий»: устаревший вид или таблица
    блокирует; «Мягкий»: только предупреждение."""
    warnings: List[str] = []
    blocked = output_blocked(issues) or unresolved_refs > 0 or sheet_changed
    if outdated:
        if mode == "strict":
            blocked = True
        else:
            warnings.append("устаревшие виды и таблицы (мягкий режим)")
    return (not blocked), warnings


# ---------- выдача в производство (3.6): счётчики гибов, журнал документа ----------
@dataclass
class IssueEvent:
    date: str
    user: str
    variant: str                    # вариант порядка («подача с конца А» / «Б»)
    qty: int
    tools: Dict[str, str]           # роль → экземпляр оснастки (инв. номер)
    bends: int                      # гибов на штуку
    cancelled: bool = False
    note: str = ""


@dataclass
class ProductionLog:
    events: List[IssueEvent] = field(default_factory=list)
    counters: Dict[str, int] = field(default_factory=dict)     # инв. номер → число гибов

    def issue(self, issues: Sequence[Issue], released_ok: bool, rows: Sequence[MapRow],
              tools: Dict[str, str], qty: int, user: str, variant: str, date: str,
              note: str = "", soft_release: bool = False) -> IssueEvent:
        """Выдача (3.6). Блокируется технологической / геометрической ошибкой. Чертёж не выпущен или устарел —
        блок в «Строгом» режиме; в «Мягком» — только предупреждение (7.7)."""
        if output_blocked(issues):
            raise ValueError("выдача заблокирована: технологическая или геометрическая ошибка (4.5)")
        if not released_ok and not soft_release:
            raise ValueError("чертёж не выпущен или устарел (7.7)")
        if qty < 1:
            raise ValueError("количество должно быть не меньше 1")
        ev = IssueEvent(date, user, variant, qty, dict(tools), len(rows), note=note)
        self.events.append(ev)
        for inst in tools.values():
            self.counters[inst] = self.counters.get(inst, 0) + len(rows) * qty
        return ev

    def cancel(self, index: int, by_admin: bool) -> None:
        """Ошибочная выдача отменяется только администратором: счётчик уменьшается, запись остаётся (3.6)."""
        if not by_admin:
            raise PermissionError("отменить выдачу может только администратор")
        ev = self.events[index]
        if ev.cancelled:
            raise ValueError("выдача уже отменена")
        for inst in ev.tools.values():
            self.counters[inst] = self.counters.get(inst, 0) - ev.bends * ev.qty
        ev.cancelled = True


# ---------- вывод карты гибки: CSV и PDF (решение пользователя: оба формата по выбору) ----------
MAP_HEADER = ["№", "S", "S_ст", "A, °", "B, °", "B_ст, °", "источник", "подача", "метка"]


_BAD_FILE_CHARS = '<>:"/\\|?*'


def tube_file_name(designation: str) -> str:
    """Имя файла карты трубы из обозначения: запрещённые в именах файлов символы заменяются на «_»."""
    name = "".join("_" if c in _BAD_FILE_CHARS or ord(c) < 32 else c for c in designation).strip().rstrip(".")
    if not name:
        raise ValueError("пустое обозначение трубы")
    return name


def assembly_map_csv(tubes: Dict[str, Sequence[MapRow]], decimals: int = 2) -> Dict[str, str]:
    """Вывод карты для сборки: по одному CSV на трубу (решение 18.10.2026). Ключ — обозначение трубы.
    Возвращает {имя файла: текст CSV}. Одинаковые имена файлов после очистки — ошибка, файлы не перезаписываются."""
    out: Dict[str, str] = {}
    for designation, rows in tubes.items():
        fname = tube_file_name(designation) + ".csv"
        if fname in out:
            raise ValueError(f"имя файла повторяется: {fname}")
        out[fname] = map_csv(rows, decimals)
    return out


def assembly_map_pdf(tubes: Dict[str, Sequence[MapRow]], directory: str, decimals: int = 2) -> List[str]:
    """PDF на каждую трубу (решение 18.10.2026): файлы в каталоге directory, имя — обозначение трубы.
    Возвращает пути. Одинаковые имена после очистки — ошибка до записи какого-либо файла."""
    import os
    names = {}
    for designation in tubes:
        fname = tube_file_name(designation) + ".pdf"
        if fname in names:
            raise ValueError(f"имя файла повторяется: {fname}")
        names[fname] = designation
    paths = []
    for fname, designation in names.items():
        paths.append(map_pdf(tubes[designation], os.path.join(directory, fname), designation, decimals))
    return paths


def _map_table(rows: Sequence[MapRow], decimals: int = 2) -> List[List[str]]:
    f = lambda v: f"{v:.{decimals}f}"
    return [[str(r.no), f(r.S), f(r.S_st), f(r.A), f(r.B), f(r.B_st), r.source, f(r.feed), f(r.mark)] for r in rows]


def map_csv(rows: Sequence[MapRow], decimals: int = 2) -> str:
    """Карта гибки в CSV (3.6): разделитель «;» — как принято в русской Excel-локали."""
    import csv
    import io
    buf = io.StringIO()
    w = csv.writer(buf, delimiter=";")
    w.writerow(MAP_HEADER)
    for line in _map_table(rows, decimals):
        w.writerow(line)
    return buf.getvalue()


def map_pdf(rows: Sequence[MapRow], path: str, title: str, decimals: int = 2) -> str:
    """Карта гибки в PDF для оператора (3.6): шапка и таблица. Возвращает путь к файлу."""
    from reportlab.lib.pagesizes import A4
    from reportlab.lib import colors
    from reportlab.platypus import SimpleDocTemplate, Table, TableStyle, Paragraph
    from reportlab.lib.styles import getSampleStyleSheet
    styles = getSampleStyleSheet()
    doc = SimpleDocTemplate(path, pagesize=A4, title=title)
    data = [MAP_HEADER] + _map_table(rows, decimals)
    table = Table(data, repeatRows=1)
    table.setStyle(TableStyle([
        ("GRID", (0, 0), (-1, -1), 0.4, colors.grey),
        ("BACKGROUND", (0, 0), (-1, 0), colors.lightgrey),
        ("FONTSIZE", (0, 0), (-1, -1), 8),
    ]))
    doc.build([Paragraph(title, styles["Heading2"]), table])
    return path
