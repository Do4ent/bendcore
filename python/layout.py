"""Компоновка окна, строка состояния и контекстные меню. Спецификация 6.14 («Компоновка окна»,
«Быстрые инструменты», «Строка состояния»), 6.4 (мини-панель), 6.6 (ПКМ в активной команде).
"""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Dict, List, Optional, Sequence, Set

DETAIL, ASSEMBLY, DRAWING = "detail", "assembly", "drawing"
MODEL_DOCS = (DETAIL, ASSEMBLY)
ALL_DOCS = (DETAIL, ASSEMBLY, DRAWING)

# Слева — панель менеджеров (6.14)
LEFT_PANEL_TABS = ["Дерево", "Команда", "Семейства", "Отображение"]

# Справа — панель задач, вертикальные вкладки сверху вниз (6.14); набор зависит от типа документа
TASK_PANEL_TABS: List[tuple] = [
    ("Свойства", ALL_DOCS),
    ("Проверка", MODEL_DOCS),            # «деталь и сборка»
    ("Библиотеки", ALL_DOCS),
    ("Палитра видов", (DRAWING,)),       # «чертёж»
    ("Проводник файлов", ALL_DOCS),
    ("Журнал", ALL_DOCS),
    ("Уведомления", ALL_DOCS),
]


def task_panel_tabs(doc_type: str) -> List[str]:
    return [name for name, docs in TASK_PANEL_TABS if doc_type in docs]


@dataclass
class TaskPanel:
    """Панель задач: клик по значку открывает и сворачивает; без «булавки» сворачивается при уходе
    курсора в графическую область; на значке — счётчик (6.14)."""
    doc_type: str
    open_tab: Optional[str] = None
    pinned: bool = False
    counters: Dict[str, int] = field(default_factory=dict)

    def click(self, tab: str) -> None:
        if tab not in task_panel_tabs(self.doc_type):
            raise ValueError(f"вкладки «{tab}» нет у документа {self.doc_type}")
        self.open_tab = None if self.open_tab == tab else tab

    def cursor_to_graphics(self) -> None:
        if not self.pinned:
            self.open_tab = None


# Таблица гибов (3.1) — закрепляемая панель внизу, у модели; у чертежа внизу вкладки листов
def bottom_panels(doc_type: str) -> List[str]:
    return (["Таблица гибов"] if doc_type in MODEL_DOCS else []) + \
           ["Листы" if doc_type == DRAWING else "Документы", "Строка состояния"]


# ---------- Строка состояния (6.14) ----------
STATUS_SECTIONS = [
    "hint",                  # подсказка текущего шага
    "cursor",                # координаты курсора в активной СК
    "plane_snaps",           # активная плоскость и режим привязок
    "units_standard",        # единицы и стандарт (клик — диалог)
    "violations",            # счётчики нарушений по классам (клик — «Проверка»)
    "recompute",             # состояние пересчёта и фоновых проверок
    "sketch_state",          # «Определён» / «Недоопределён: N степеней свободы»
    "edit_context",          # «Правка: Труба 12 в сборке …»
    "selection",             # значения выбранного, число выбранного, значок фильтра
    "toggles",               # привязки (F3), автоограничения, ортогональный режим, проверять на лету, автопересчёт
    "background_tasks",      # импорт, распознавание, проверки — прогресс и «Отменить»
    "assembly_loading",      # «Загружено 1240 из 4800 компонентов»
    "drawing_scale",         # на чертеже: масштаб листа и вида, координаты на листе
]
STATUS_TOGGLES = ["snap_toggle", "autoconstraints", "ortho_mode", "check_on_the_fly", "auto_recompute"]


@dataclass
class StatusBar:
    """Секции включаются и выключаются правым кликом; набор по умолчанию — из профиля предприятия (6.14)."""
    visible: Set[str] = field(default_factory=lambda: set(STATUS_SECTIONS))

    def toggle(self, section: str) -> None:
        if section not in STATUS_SECTIONS:
            raise ValueError(section)
        self.visible ^= {section}


def sketch_state_text(dof: int, conflict: bool = False) -> str:
    """Секция состояния эскиза (6.14, 1.3)."""
    if conflict:
        return "Конфликт"
    return "Определён" if dof == 0 else f"Недоопределён: {dof} степеней свободы"


def selection_text(kind: str, values: Dict[str, float]) -> str:
    """Значения выбранного: сегмент — S и L, гиб — B и R, два объекта — расстояние (6.14)."""
    order = {"segment": ("S", "L"), "bend": ("B", "R"), "pair": ("Расстояние",)}[kind]
    return ", ".join(f"{k} = {values[k]:g}" for k in order if k in values)


# ---------- Контекстные меню и мини-панель ----------
# Мини-панель у выбранного объекта (6.4) — частые действия по типу.
MINI_PANEL: Dict[str, List[str]] = {
    "tube": ["edit_spec", "bend_table"],
    "port": ["new_tube", "flip_port"],
    "import": ["hide_component", "exclude_restore", "set_color"],
    "vertex": ["sketch_dimension"],
    # предложение 18.10.2026 (спецификация состав не перечисляет): сегмент, компонент, вид, пустое место
    "segment": ["sketch_dimension", "bend_table"],
    "component": ["move_component", "hide_component", "set_color"],
    "view": ["view_fit", "section", "shading_mode"],
    "empty": ["paste", "select_all", "view_fit"],
}
OBJECT_TYPES = ("tube", "vertex", "segment", "port", "component", "view", "empty")
CONTEXT_MENU_TAIL = ["properties", "show_dependents", "hide_isolate"]


def context_menu(object_type: str, in_command: bool = False) -> List[str]:
    """ПКМ (6.14): в активной команде — ОК, Отмена, Повторить (6.6); иначе сверху частые действия по типу
    объекта, ниже Свойства, Показать зависимые, Скрыть / Изолировать. Частые действия взяты из мини-панели 6.4
    (для сегмента, компонента, вида и пустого места спецификация их не перечисляет)."""
    if in_command:
        return ["confirm_command", "cancel_command", "repeat_last_command"]
    if object_type not in OBJECT_TYPES:
        raise ValueError(object_type)
    return MINI_PANEL.get(object_type, []) + CONTEXT_MENU_TAIL


# Цвета эскиза (1.3): недоопределено — синий; определено — чёрный; конфликт — красный; ссылка на окружение — фиолетовый.
SKETCH_COLORS = {"under": "синий", "defined": "чёрный", "conflict": "красный", "reference": "фиолетовый"}


def sketch_color(dof: int, conflict: bool = False, reference: bool = False) -> str:
    """Цвет элемента эскиза по состоянию (1.3). Конфликт важнее определённости; ссылка на окружение — отдельный цвет."""
    if conflict:
        return SKETCH_COLORS["conflict"]
    if reference:
        return SKETCH_COLORS["reference"]
    return SKETCH_COLORS["defined" if dof == 0 else "under"]
