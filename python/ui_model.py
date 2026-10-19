"""Этап 5, модель интерфейса (без Qt). Спецификация 6.2 (дерево), 6.3 (панель свойств), 4.5 (классы
нарушений), 9.4, 9.10 (значки импорта), 6.15 (единицы ввода — units.py).

Виджеты Qt строятся поверх этой модели, когда PySide6 будет доступен.
"""
from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum
from typing import Any, Callable, Dict, Iterable, List, Optional, Sequence, Set, Tuple

from units import parse_length


# ---------- 6.2 Единый язык состояний ----------
class State(Enum):
    """Одно состояние узла (значок справа в строке узла)."""
    NORMAL = "norm"                   # без значка
    WARNING = "warn"                  # жёлтый треугольник
    ERROR_GEOM = "err_geom"           # красный круг, класс в подсказке
    ERROR_TECH = "err_tech"           # красный круг, класс в подсказке
    MARK_CROSS = "mark_cross"         # пометка: пересечение — красная метка
    MARK_GAP = "mark_gap"             # пометка: зазор — оранжевая метка
    MARK_FLAG = "mark_flag"           # прочие пометки («материал не подтверждён») — оранжевый флажок
    INFO = "info"                     # синий значок
    NEEDS_RECALC = "recalc"           # молния


class Flag(Enum):
    """Признаки узла, у каждого свой значок; могут сочетаться с состоянием."""
    SUPPRESSED = "suppressed"         # подавлено — серый
    REF_LOST = "ref_lost"             # ссылка потеряна — разорванная цепь
    FROZEN = "frozen"                 # заморожена — замок
    NONSTANDARD = "nonstd"            # нестандартная труба — отдельная отметка
    EXTERNAL = "external"             # внешняя часть проекта — значок связи и путь (6.16)
    FITTING = "fitting"               # фитинг — значок типа (9.11)
    DO_NOT_RECOGNIZE = "no_recognize" # флаг «не распознавать» (9.4)
    REMAINDER = "remainder"           # «Остаток» после «Преобразовать в трубу» (9.4)
    FIXED = "fixed"                   # импорт: фиксация (9.10)
    DEVIATED = "deviated"             # импорт: отклонение от источника — цвет, положение (9.10)
    EXCLUDED = "excluded"             # импорт: исключено (9.10)
    MESH = "mesh"                     # импорт: «по сетке» (9.10)


ICON_FOR_STATE: Dict[State, Optional[str]] = {
    State.NORMAL: None, State.WARNING: "triangle_yellow", State.ERROR_GEOM: "circle_red",
    State.ERROR_TECH: "circle_red", State.MARK_CROSS: "mark_red", State.MARK_GAP: "mark_orange",
    State.MARK_FLAG: "flag_orange", State.INFO: "info_blue", State.NEEDS_RECALC: "bolt",
}
ICON_FOR_FLAG: Dict[Flag, str] = {
    Flag.SUPPRESSED: "grey", Flag.REF_LOST: "chain_broken", Flag.FROZEN: "lock", Flag.NONSTANDARD: "nonstd_mark",
    Flag.EXTERNAL: "link", Flag.FITTING: "fitting_type", Flag.DO_NOT_RECOGNIZE: "no_recognize",
    Flag.REMAINDER: "remainder", Flag.FIXED: "pin", Flag.DEVIATED: "deviated", Flag.EXCLUDED: "excluded",
    Flag.MESH: "mesh",
}
TOOLTIP_CLASS = {State.ERROR_GEOM: "геометрическая ошибка", State.ERROR_TECH: "технологическая ошибка"}

# Классы нарушений 4.5 — для счётчиков у папок и фильтра «Только с нарушениями»
VIOLATION_CLASS = {
    State.ERROR_GEOM: "geometric", State.ERROR_TECH: "technological", State.WARNING: "warning",
    State.MARK_CROSS: "mark", State.MARK_GAP: "mark", State.MARK_FLAG: "mark", State.INFO: "info",
}


def icon_key(state: State) -> Optional[str]:
    return ICON_FOR_STATE[state]


# ---------- 6.2 Узел дерева ----------
@dataclass
class TreeNode:
    key: str
    name: str
    state: State = State.NORMAL
    flags: Set[Flag] = field(default_factory=set)
    children: List["TreeNode"] = field(default_factory=list)
    kind: str = "folder"                        # тип узла: tube, port, component, folder, …
    props: Dict[str, Any] = field(default_factory=dict)   # обозначение, описание, сортамент, система, …
    instance_of: Optional[str] = None           # для свёртки одинаковых экземпляров

    def icons(self) -> List[str]:
        """Значки строки: состояние (если есть) + признаки."""
        out = [ICON_FOR_STATE[self.state]] if ICON_FOR_STATE[self.state] else []
        return out + [ICON_FOR_FLAG[f] for f in sorted(self.flags, key=lambda f: f.value)]

    def walk(self) -> Iterable["TreeNode"]:
        yield self
        for c in self.children:
            yield from c.walk()

    def find(self, key: str) -> Optional["TreeNode"]:
        return next((n for n in self.walk() if n.key == key), None)

    def counters(self) -> Tuple[int, Dict[str, int]]:
        """Счётчики папки (6.2): число объектов внутри и нарушения по классам 4.5."""
        objs = 0
        viol: Dict[str, int] = {}
        for n in self.walk():
            if n is self:
                continue
            if n.kind != "folder":
                objs += 1
            cls = VIOLATION_CLASS.get(n.state)
            if cls:
                viol[cls] = viol.get(cls, 0) + 1
        return objs, viol


LABEL_MODES = ("name", "designation", "description", "sortament")


def node_label(node: TreeNode, mode: str = "name+sortament") -> str:
    """Строка узла (6.2): по умолчанию «имя [сортамент]» (6.13); переключатель «Подписи»: имя, обозначение,
    описание или сортамент."""
    if mode == "name+sortament":
        s = node.props.get("sortament")
        return f"{node.name} [{s}]" if s else node.name
    if mode == "name":
        return node.name
    if mode not in LABEL_MODES:
        raise ValueError(mode)
    return str(node.props.get(mode) or node.name)


def node_tooltip(node: TreeNode) -> str:
    """Подсказка трубы: длина заготовки, число гибов, трубогиб, система, источник, чертёж; класс ошибки."""
    parts = []
    for k, label in (("blank_length", "Заготовка"), ("bends", "Гибов"), ("bender", "Трубогиб"),
                     ("system", "Система"), ("source", "Источник"), ("drawing", "Чертёж")):
        if k in node.props:
            parts.append(f"{label}: {node.props[k]}")
    if node.state in TOOLTIP_CLASS:
        parts.append(TOOLTIP_CLASS[node.state].capitalize())
    return "; ".join(parts)


# Порядок корня сборки (6.2); пустые узлы скрыты; порядок можно переставить (личный профиль)
ROOT_ORDER = ["origin_aux", "import", "ports", "tubes", "forbidden_zones", "measurements", "parameters",
              "drawings", "rollback_bar"]
DETAIL_ROOT = ["origin", "aux_geometry", "measurements", "tube"]      # Труба → Эскиз пути → Тело
TUBE_ORDER = ("by_systems", "by_creation", "by_name")                 # по умолчанию — по системам


def root_children(nodes: Dict[str, TreeNode], order: Sequence[str] = ROOT_ORDER) -> List[TreeNode]:
    """Корень в заданном порядке; пустые узлы скрыты (полоса отката — всегда)."""
    return [nodes[k] for k in order if k in nodes and (nodes[k].children or k == "rollback_bar")]


def sort_tubes(tubes: List[TreeNode], order: str = "by_systems") -> List[TreeNode]:
    if order == "by_systems":
        return sorted(tubes, key=lambda t: (str(t.props.get("system", "")), t.props.get("created", 0)))
    if order == "by_creation":
        return sorted(tubes, key=lambda t: t.props.get("created", 0))
    if order == "by_name":
        return sorted(tubes, key=lambda t: t.name)
    raise ValueError(order)


def collapse_instances(children: List[TreeNode]) -> List[Tuple[TreeNode, int]]:
    """Одинаковые экземпляры свёрнуты в одну строку «Клапан ×24» (6.2, по умолчанию включено, 6.13)."""
    out: List[Tuple[TreeNode, int]] = []
    seen: Dict[str, int] = {}
    for c in children:
        if c.instance_of is None:
            out.append((c, 1))
        elif c.instance_of in seen:
            node, n = out[seen[c.instance_of]]
            out[seen[c.instance_of]] = (node, n + 1)
        else:
            seen[c.instance_of] = len(out)
            out.append((c, 1))
    return out


def collapsed_label(node: TreeNode, count: int) -> str:
    return f"{node.name} ×{count}" if count > 1 else node.name


def reorder(node: TreeNode, key: str, new_index: int) -> None:
    """Перетаскивание порядка внутри одного родителя."""
    for i, c in enumerate(node.children):
        if c.key == key:
            child = node.children.pop(i)
            node.children.insert(max(0, min(new_index, len(node.children))), child)
            return
    raise KeyError(key)


def filter_tree(node: TreeNode, pred: Callable[[TreeNode], bool]) -> Optional[TreeNode]:
    """Копия поддерева с подходящими узлами и их предками."""
    kept = [c for c in (filter_tree(ch, pred) for ch in node.children) if c is not None]
    if pred(node) or kept:
        return TreeNode(node.key, node.name, node.state, set(node.flags), kept, node.kind, dict(node.props),
                        node.instance_of)
    return None


def only_violations(node: TreeNode) -> Optional[TreeNode]:
    """Фильтр «Только с нарушениями» (6.2)."""
    return filter_tree(node, lambda n: n.state in VIOLATION_CLASS)


def search(node: TreeNode, text: str) -> List[TreeNode]:
    """Поиск по всему проекту, включая нераскрытые узлы и свойства (обозначение, материал, ∅) (6.2)."""
    t = text.lower()
    return [n for n in node.walk()
            if t in n.name.lower() or any(t in str(v).lower() for v in n.props.values())]


def rename(root: TreeNode, key: str, new_name: str) -> None:
    """Переименовать (F2 или медленный двойной клик): имя уникально в документе; исходное имя импорта
    остаётся в свойствах (6.2)."""
    if any(n.name == new_name and n.key != key for n in root.walk()):
        raise ValueError(f"имя «{new_name}» уже есть в документе")
    n = root.find(key)
    if n is None:
        raise KeyError(key)
    if n.props.get("from_import") and "original_name" not in n.props:
        n.props["original_name"] = n.name
    n.name = new_name


def delete_folder(parent: TreeNode, key: str) -> None:
    """Удаление пользовательской папки выносит содержимое на уровень выше (6.2)."""
    for i, c in enumerate(parent.children):
        if c.key == key:
            if c.kind != "folder":
                raise ValueError("не папка")
            parent.children[i:i + 1] = c.children
            return
    raise KeyError(key)


# «Табличный вид» дерева (6.2): колонки, сортировка и фильтр по любой
TABLE_COLUMNS = ["name", "state", "od_wall", "blank_length", "bends", "system", "drawing"]
DRAWING_STATES = ("нет", "актуален", "устарел", "выпущен")


def table_view(rows: List[TreeNode], columns: Sequence[str] = TABLE_COLUMNS, sort_by: Optional[str] = None,
               where: Optional[Callable[[Dict[str, Any]], bool]] = None) -> List[Dict[str, Any]]:
    full = []
    for n in rows:
        r = {"name": n.name, "state": n.state.value,
             **{c: n.props.get(c) for c in TABLE_COLUMNS if c not in ("name", "state")}}
        if where is None or where(r):                     # фильтр и сортировка — по любой колонке
            full.append(r)
    if sort_by:
        full.sort(key=lambda r: (r[sort_by] is None, r[sort_by]))
    return [{c: r.get(c) for c in columns} for r in full]


# ---------- 6.3 Панель свойств ----------
DIFFERENT = object()   # значение «разные» при множественном выборе


def common_values(objects: Sequence[Dict[str, Any]], names: Sequence[str]) -> Dict[str, Any]:
    result: Dict[str, Any] = {}
    for name in names:
        values = [o.get(name) for o in objects]
        first = values[0] if values else None
        result[name] = first if all(v == first for v in values) else DIFFERENT
    return result


def parse_length_mm(text: str, doc_unit: str = "mm") -> float:
    """Ввод длины в поле (6.3, 6.15): «1in» → 25.4; без суффикса — единицы документа."""
    return parse_length(text, doc_unit)


@dataclass
class PropertyField:
    name: str
    label: str
    value: Any
    editable: bool = True
    reason_disabled: str = ""       # подсказка, почему поле недоступно (серое)
    overridden: bool = False        # значок «переопределено»
    inherited: Any = None           # для «Сбросить к унаследованному»
    unit: str = ""
    group: str = ""                 # группа свойств (сворачивается)


# Свойства по типам объектов (6.3). (имя, подпись, правится): правятся управляющие размеры и явные поля;
# остальное только чтение.
PROPERTY_SETS: Dict[str, List[Tuple[str, str, bool]]] = {
    "bend": [("S", "S", True), ("A", "A", True), ("B", "B", True), ("R", "R", False), ("status", "Статус", False)],
    "segment": [("S", "S", True), ("L", "Расстояние между вершинами", True)],
    "vertex": [("xyz", "Координаты", True), ("B", "B", True), ("A", "A", True),
               ("B_st", "B_ст", False), ("B_st_source", "Источник B_ст", False)],
    "spiral": [("R", "R оправки", False), ("p", "Шаг p", True), ("n", "Число витков n", True),
               ("direction", "Направление", True), ("L_spiral", "Длина L_спир", False)],
    "sketch": [("dof", "Степени свободы", False), ("state", "Состояние", False)],
    "assembly": [("gap_norm", "Норма зазора", True), ("tubes", "Число труб", False),
                 ("import_status", "Статус источников импорта", False)],
    "document": [("standard", "Стандарт", True), ("units", "Единицы", True), ("mass", "Масса", False),
                 ("status", "Статус", False), ("user_props", "Пользовательские свойства", True)],
}


class PropertyPanel:
    """Панель свойств (6.3): выбранный объект (или документ без выбора), «булавка», множественный выбор,
    группы и поиск, правка одной транзакцией, «только чтение» во время активной команды."""

    def __init__(self, apply: Callable[[Dict[str, Any]], None]):
        self._apply = apply                  # одна транзакция (Undo) над всеми выбранными объектами
        self.pinned: bool = False
        self.fields: List[PropertyField] = []
        self.target: Any = None
        self.command_active: bool = False    # параметры команды — в панели «Команда» слева (6.6)
        self.collapsed: Set[str] = set()

    def select(self, target: Any, fields: List[PropertyField], document_fields: Optional[List[PropertyField]] = None) -> None:
        """Новый выбор в 3D. С «булавкой» панель не меняется; без выбора — свойства документа."""
        if self.pinned:
            return
        if target is None:
            self.target, self.fields = None, list(document_fields or [])
        else:
            self.target, self.fields = target, list(fields)

    def show(self, fields: List[PropertyField]) -> None:
        self.fields = fields

    def visible(self, query: str = "") -> List[PropertyField]:
        """Поля с учётом свёрнутых групп и поиска по свойствам."""
        q = query.lower()
        return [f for f in self.fields
                if (q and (q in f.label.lower() or q in f.name.lower())) or (not q and f.group not in self.collapsed)]

    def commit(self, edits: Dict[str, Any]) -> None:
        if self.command_active:
            raise PermissionError("идёт команда: свойства объекта только для чтения")
        allowed = {f.name: f for f in self.fields}
        for name, val in edits.items():
            f = allowed.get(name)
            if f is None or not f.editable:
                raise PermissionError(f"поле {name!r} недоступно: {f.reason_disabled if f else 'нет такого поля'}")
            if val is DIFFERENT:
                raise ValueError("значение «разные» не применяется")
        if edits:
            self._apply(dict(edits))         # Enter — одна транзакция

    def reset_override(self, name: str) -> None:
        f = next(x for x in self.fields if x.name == name)
        if f.overridden:
            self._apply({name: f.inherited})


# ---------- 6.2: закладки дерева (версия 1 — простые: именованный набор узлов) ----------
class Bookmarks:
    """Закладки хранятся в проекте. Закладка — имя и набор ключей узлов дерева; переключение — одним действием.
    Закладки по фильтрам и переменным не входят в версию 1 (решение пользователя 18.10.2026)."""

    def __init__(self) -> None:
        self._sets: Dict[str, List[str]] = {}

    def add(self, name: str, keys: Iterable[str]) -> None:
        name = name.strip()
        if not name:
            raise ValueError("пустое имя закладки")
        if name in self._sets:
            raise ValueError(f"закладка «{name}» уже есть")
        self._sets[name] = list(dict.fromkeys(keys))

    def remove(self, name: str) -> None:
        self._sets.pop(name)

    def rename(self, old: str, new: str) -> None:
        new = new.strip()
        if not new or new in self._sets:
            raise ValueError("имя пустое или уже занято")
        self._sets[new] = self._sets.pop(old)

    def keys(self, name: str) -> List[str]:
        return list(self._sets[name])

    def names(self) -> List[str]:
        return list(self._sets)

    def to_dict(self) -> Dict[str, List[str]]:
        return {k: list(v) for k, v in self._sets.items()}

    @classmethod
    def from_dict(cls, data: Dict[str, List[str]]) -> "Bookmarks":
        b = cls()
        for name, keys in data.items():
            b.add(name, keys)
        return b
