"""Этап 5, горячие клавиши, жесты и стили навигации. Спецификация 6.14 («Клавиатура», «Клавиши привязок»,
«Жесты мыши»), 6.4 (стиль навигации), 6.5 (выбор), 6.6 (Enter / Esc / ПКМ), 6.10 (личные настройки), 6.13.

Три независимых слоя (6.10: «стиль навигации», «набор горячих клавиш и клавиши привязок» — разные настройки):
  1. Стиль навигации — вращение, панорама, масштаб мышью (и клавишами-удержаниями Inventor).
  2. Набор горячих клавиш — клавиши, жесты и мышиные действия системы-образца.
  3. Программные привязки — команды программы, которых нет в системе-образце (привязки, выбор перекрытых,
     меню ПКМ, жесты по умолчанию). Они занимают только свободные клавиши: «Трубным командам, которых нет
     в системе-образце, даются свои клавиши без конфликта с набором» (6.14).

Привязка = (контекст, сочетание) → команда. Контексты 6.14: глобальный, эскиз пути, 2D-эскиз чертежа, сборка,
чертёж; «клавиша контекста перекрывает глобальную». Дополнительно — состояние «ввод точки» (разовые привязки
Alt + буква, Tab), которое перекрывает текущий контекст.
Клавиши привязаны к физическому положению и работают в любой раскладке; подпись — «L (Д)».
В полях ввода текста клавиши-команды не перехватываются.
"""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Callable, Dict, List, Optional, Sequence, Tuple

# ---------- Контексты ----------
GLOBAL = "*"
PATH_SKETCH = "path_sketch"      # эскиз пути
SKETCH_2D = "sketch_2d"          # 2D-эскиз чертежа (7.9)
ASSEMBLY = "assembly"
DRAWING = "drawing"
POINT_INPUT = "point_input"      # состояние «ввод точки» поверх любого контекста (6.14, «Клавиши привязок»)
CONTEXTS = (GLOBAL, PATH_SKETCH, SKETCH_2D, ASSEMBLY, DRAWING)

_MODS = ("Ctrl", "Shift", "Alt")
_BUTTON_ALIASES = {"lmb": "LMB", "mmb": "MMB", "rmb": "RMB", "mb1": "LMB", "mb2": "MMB", "mb3": "RMB"}
_ACTIONS = {"drag": "Drag", "click": "Click", "doubleclick": "DoubleClick", "hold": "Hold",
            "upright": "UpRight", "upleft": "UpLeft", "downright": "DownRight", "downleft": "DownLeft",
            "esc": "Esc", "escape": "Esc", "pageup": "PageUp", "pagedown": "PageDown"}

# Русская раскладка по позиции клавиши (ЙЦУКЕН → QWERTY): привязка работает в обеих раскладках.
_CYR = "йцукенгшщзхъфывапролджэячсмитьбю"
_LAT = "qwertyuiop[]asdfghjkl;'zxcvbnm,."
_CYR_TO_LAT = dict(zip(_CYR, _LAT))
_LAT_TO_CYR = dict(zip(_LAT, _CYR))

# Клавиши, которые в поле ввода текста печатают или правят текст — не перехватываются (6.14).
_TEXT_KEYS = {"Space", "Tab", "Delete", "Backspace", "Home", "End", "Left", "Right", "Up", "Down"}


def normalize(combo: str) -> str:
    """«ctrl+shift+z» → «Ctrl+Shift+Z»; «alt+drag» → «Alt+Drag»; «Ctrl+MB2» → «Ctrl+MMB»;
    последовательность (Creo mapkey) «s>l» → «S>L». Порядок: Ctrl, Shift, Alt, кнопка мыши, клавиша/действие."""
    if ">" in combo:
        steps = [normalize(s) for s in combo.split(">") if s.strip()]
        if len(steps) < 2:
            raise ValueError("последовательность из двух и более клавиш")
        return ">".join(steps)
    parts = [p.strip() for p in combo.replace(" ", "").split("+") if p.strip()]
    if not parts:
        raise ValueError("пустое сочетание")
    key: Optional[str] = parts[-1]
    if len(key) == 1 and key.lower() in _CYR_TO_LAT:
        key = _CYR_TO_LAT[key.lower()]           # «Я» → Z: физическая клавиша
    mods: set = set()
    button = None
    for p in parts[:-1]:
        pl = p.lower()
        if pl in ("ctrl", "shift", "alt"):
            mods.add(pl)
        elif pl in _BUTTON_ALIASES:
            if button is not None:
                raise ValueError("аккорды двух кнопок мыши не поддерживаются")
            button = _BUTTON_ALIASES[pl]
        else:
            raise ValueError(f"неизвестный модификатор: {p}")
    if key.lower() in _BUTTON_ALIASES:           # «Ctrl+MMB» — сочетание, оканчивающееся кнопкой
        if button is not None:
            raise ValueError("аккорды двух кнопок мыши не поддерживаются")
        button = _BUTTON_ALIASES[key.lower()]
        key = None
    if key is not None:
        if key.lower() in _ACTIONS:
            key = _ACTIONS[key.lower()]
        elif len(key) == 1:
            key = key.upper()
        else:
            key = key[:1].upper() + key[1:].lower()
    ordered = [m for m in _MODS if m.lower() in mods]
    return "+".join(ordered + [t for t in (button, key) if t])


def label(combo: str) -> str:
    """Подпись для таблицы клавиш: буква с русской парой — «L (Д)», «Ctrl+Z (Я)»."""
    def one(c: str) -> str:
        n = normalize(c)
        last = n.split("+")[-1]
        if len(last) == 1 and last.lower() in _LAT_TO_CYR:
            return f"{n} ({_LAT_TO_CYR[last.lower()].upper()})"
        return n
    if ">" in combo:
        return " > ".join(one(s) for s in combo.split(">"))
    return one(combo)


def _is_text_key(norm: str) -> bool:
    """Сочетание без Ctrl/Alt, которое в поле ввода печатает или правит текст."""
    if ">" in norm:
        return all(_is_text_key(s) for s in norm.split(">"))
    parts = norm.split("+")
    if "Ctrl" in parts or "Alt" in parts or any(b in parts for b in ("LMB", "MMB", "RMB")):
        return False
    key = parts[-1]
    return len(key) == 1 or key in _TEXT_KEYS


# ---------- Набор привязок ----------
@dataclass
class KeySet:
    name: str
    bindings: Dict[Tuple[str, str], str] = field(default_factory=dict)   # (контекст, сочетание) → команда

    def bind(self, combo: str, command: str, context: str = GLOBAL) -> None:
        self.bindings[(context, normalize(combo))] = command

    def command_for(self, combo: str, context: str = GLOBAL, point_input: bool = False,
                    in_text_field: bool = False) -> Optional[str]:
        key = normalize(combo)
        if in_text_field and _is_text_key(key):
            return None                                   # 6.14: в поле ввода текста не перехватываются
        if point_input and (POINT_INPUT, key) in self.bindings:
            return self.bindings[(POINT_INPUT, key)]
        if context != GLOBAL and (context, key) in self.bindings:
            return self.bindings[(context, key)]
        return self.bindings.get((GLOBAL, key))

    def keys_for(self, command: str) -> List[Tuple[str, str]]:
        """Все (контекст, сочетание) команды — для подсказки кнопки и таблицы клавиш."""
        return sorted(k for k, c in self.bindings.items() if c == command)

    def table(self) -> List[Tuple[str, str, str]]:
        """Таблица «команда — клавиша — контекст» (6.14) для показа, печати и экспорта."""
        return sorted((c, label(k[1]), k[0]) for k, c in self.bindings.items())


Pairs = List[Tuple[str, str, str]]


def load_pairs(name: str, pairs: Pairs) -> Tuple[KeySet, List[str]]:
    """Загрузка из троек (контекст, сочетание, команда). Одинаковые тройки допустимы; разные команды
    на одно сочетание в одном контексте — конфликт, сочетание не загружается."""
    report: List[str] = []
    seen: Dict[Tuple[str, str], List[str]] = {}
    for ctx, combo, cmd in pairs:
        try:
            key = (ctx, normalize(combo))
        except ValueError as e:
            report.append(f"{combo!r}: {e}")
            continue
        seen.setdefault(key, [])
        if cmd not in seen[key]:
            seen[key].append(cmd)
    ks = KeySet(name)
    for key, cmds in seen.items():
        if len(cmds) > 1:
            report.append(f"конфликт {key[1]} в контексте {key[0]}: " + ", ".join(cmds))
        else:
            ks.bindings[key] = cmds[0]
    return ks, report


def customize(base: KeySet, overrides: Dict[Tuple[str, str], Optional[str]]) -> Tuple[KeySet, List[str]]:
    """Ручное переназначение поверх набора; None — привязка снята. Базовый набор не меняется.
    Возвращает предупреждения о конфликтах внутри контекста (6.14)."""
    ks = KeySet(name=base.name + " (личный)", bindings=dict(base.bindings))
    warnings: List[str] = []
    for (ctx, combo), cmd in overrides.items():
        key = (ctx, normalize(combo))
        if cmd is None:
            ks.bindings.pop(key, None)
            continue
        old = ks.bindings.get(key)
        if old is not None and old != cmd:
            warnings.append(f"{label(key[1])} в контексте {ctx} было назначено «{old}», теперь «{cmd}»")
        ks.bindings[key] = cmd
    return ks, warnings


def _merge(name: str, layers: Sequence[KeySet]) -> Tuple[KeySet, List[str]]:
    """Слои по возрастанию приоритета: следующий перекрывает предыдущий; перекрытие разных команд — в отчёт."""
    ks = KeySet(name)
    report: List[str] = []
    for layer in layers:
        for key, cmd in layer.bindings.items():
            old = ks.bindings.get(key)
            if old is not None and old != cmd:
                report.append(f"{key[1]} в контексте {key[0]}: «{old}» ({ks.name}) перекрыто «{cmd}» ({layer.name})")
            ks.bindings[key] = cmd
    return ks, report


# ---------- 1. Стили навигации (6.4) ----------
# Удержание клавиш Inventor (F2/F3/F4 + ЛКМ) записано как клавиша; режим удержания — забота обработчика ввода.
NAVIGATION_PAIRS: Dict[str, Pairs] = {
    "SolidWorks": [
        (GLOBAL, "MMB+Drag", "rotate"),
        (GLOBAL, "Shift+MMB+Drag", "zoom"),
        (GLOBAL, "Ctrl+MMB+Drag", "pan"),
        (GLOBAL, "Ctrl+RMB+Drag", "rotate_about_screen_normal"),
    ],
    "Inventor": [
        (GLOBAL, "MMB+Drag", "pan"),
        (GLOBAL, "Shift+MMB+Drag", "orbit"),
        (GLOBAL, "F2", "pan"),
        (GLOBAL, "F3", "zoom"),
        (GLOBAL, "F4", "orbit"),
    ],
    "Siemens NX": [
        (GLOBAL, "MMB+Drag", "rotate"),
        (GLOBAL, "Shift+MMB+Drag", "pan"),
        (GLOBAL, "Ctrl+MMB+Drag", "zoom"),
    ],
    # CATIA (справочник 18.10.2026): панорама и масштаб — Shift+MMB / Ctrl+MMB (решение пользователя; аккорды MMB+RMB не поддержаны)
    "CATIA": [
        (GLOBAL, "MMB+Drag", "rotate"),
        (GLOBAL, "Shift+MMB+Drag", "pan"),        # решение пользователя 18.10.2026 (вместо аккордов MMB+RMB)
        (GLOBAL, "Ctrl+MMB+Drag", "zoom"),
    ],
    "PTC Creo": [
        (GLOBAL, "MMB+Drag", "rotate"),
        (GLOBAL, "Shift+MMB+Drag", "pan"),
        (GLOBAL, "Ctrl+MMB+Drag", "zoom"),
    ],
}


def navigation(style: str) -> KeySet:
    ks, report = load_pairs(f"навигация {style}", NAVIGATION_PAIRS[style])
    assert not report, report
    return ks


# ---------- 2. Наборы горячих клавиш (таблицы пользователя, 18.10.2026) ----------
_SKETCHES = (PATH_SKETCH, SKETCH_2D)


def _both_sketches(rows: List[Tuple[str, str]]) -> Pairs:
    return [(ctx, combo, cmd) for ctx in _SKETCHES for combo, cmd in rows]


SOLIDWORKS_PAIRS: Pairs = [
    (GLOBAL, "S", "shortcut_bar"),
    (GLOBAL, "D", "confirm_breadcrumbs"),
    (GLOBAL, "W", "toggle_command_panel"),
    (GLOBAL, "Space", "view_selector"),
    (GLOBAL, "F", "fit_to_screen"),
    (GLOBAL, "Z", "zoom_out"),
    (GLOBAL, "Shift+Z", "zoom_in"),
    *_both_sketches([("L", "sketch_line"), ("A", "sketch_arc"), ("C", "sketch_circle"),
                     ("R", "sketch_rectangle"), ("S", "sketch_toolbar")]),
    (ASSEMBLY, "Alt+Drag", "smart_mate"),             # решение 18.10: Alt+перетаскивание — умные сопряжения
    (ASSEMBLY, "Ctrl+Drag", "copy_component"),
    (ASSEMBLY, "Ctrl+LMB+Click", "copy_component"),
    (ASSEMBLY, "Tab", "hide_component"),
    (ASSEMBLY, "Shift+Tab", "show_component"),
    (ASSEMBLY, "Alt+Click", "toggle_transparency"),
    (GLOBAL, "Ctrl+1", "view_front"), (GLOBAL, "Ctrl+2", "view_back"), (GLOBAL, "Ctrl+3", "view_left"),
    (GLOBAL, "Ctrl+4", "view_right"), (GLOBAL, "Ctrl+5", "view_top"), (GLOBAL, "Ctrl+6", "view_bottom"),
    (GLOBAL, "Ctrl+7", "view_isometric"), (GLOBAL, "Ctrl+8", "view_normal_to"),
    (GLOBAL, "F5", "selection_filter_panel"),
    (GLOBAL, "Ctrl+N", "new_document"), (GLOBAL, "Ctrl+O", "open_file"), (GLOBAL, "Ctrl+S", "save"),
    (GLOBAL, "Ctrl+B", "rebuild"), (GLOBAL, "Ctrl+Q", "force_rebuild"),
    (GLOBAL, "Ctrl+Z", "undo"), (GLOBAL, "Ctrl+Y", "redo"), (GLOBAL, "Ctrl+C", "copy"), (GLOBAL, "Ctrl+V", "paste"),
    # колесо жестов (ПКМ + направление): виды в детали и сборке, свои команды в эскизе и чертеже
    (GLOBAL, "RMB+Up", "view_top"), (GLOBAL, "RMB+Right", "view_right"),
    (GLOBAL, "RMB+Down", "view_front"), (GLOBAL, "RMB+Left", "view_left"),
    *_both_sketches([("RMB+Up", "smart_dimension"), ("RMB+Right", "sketch_circle"),
                     ("RMB+Down", "sketch_line"), ("RMB+Left", "sketch_rectangle")]),
    (DRAWING, "RMB+Up", "note"), (DRAWING, "RMB+Right", "projected_view"),
    (DRAWING, "RMB+Down", "smart_dimension"), (DRAWING, "RMB+Left", "section_view"),
]

INVENTOR_PAIRS: Pairs = [
    (GLOBAL, "Space", "marking_menu"),
    (GLOBAL, "F5", "previous_view"),
    (GLOBAL, "F6", "home_view"),
    (GLOBAL, "Home", "zoom_all"),
    (GLOBAL, "PageUp", "look_at"),
    (GLOBAL, "S", "new_2d_sketch"),
    (GLOBAL, "Shift+S", "shortcut_bar"),              # решение 18.10.2026: панель команд на Shift+S (S занята)
    (GLOBAL, "Ctrl+F3", "snap_toggle"),               # решение 18.10.2026: привязки на Ctrl+F3 (F3 — масштаб)
    (GLOBAL, "F7", "slice_graphics"),
    *_both_sketches([("L", "sketch_line"), ("D", "sketch_dimension"), ("C", "sketch_circle"),
                     ("R", "sketch_rectangle"), ("A", "sketch_arc"), ("X", "sketch_trim"), ("E", "finish_sketch")]),
    (GLOBAL, "E", "extrude"), (GLOBAL, "R", "revolve"), (GLOBAL, "F", "fillet"), (GLOBAL, "H", "hole"),
    (GLOBAL, "C", "chamfer"), (GLOBAL, "K", "rectangular_pattern"), (GLOBAL, "Shift+C", "circular_pattern"),
    (GLOBAL, "M", "measure"),
    (ASSEMBLY, "P", "place_component"), (ASSEMBLY, "C", "assembly_constraint"), (ASSEMBLY, "V", "joint"),
    (ASSEMBLY, "G", "move_component"), (ASSEMBLY, "Alt+Drag", "quick_mate"), (ASSEMBLY, "Tab", "hide_show_component"),
    (GLOBAL, "Ctrl+N", "new_document"), (GLOBAL, "Ctrl+O", "open_file"), (GLOBAL, "Ctrl+S", "save"),
    (GLOBAL, "Ctrl+Z", "undo"), (GLOBAL, "Ctrl+Y", "redo"), (GLOBAL, "Ctrl+C", "copy"), (GLOBAL, "Ctrl+V", "paste"),
    # маркерное меню 2D-эскиза (ПКМ + направление, слепой жест)
    (SKETCH_2D, "RMB+Up", "fillet"), (SKETCH_2D, "RMB+UpRight", "sketch_rectangle"),
    (SKETCH_2D, "RMB+Right", "sketch_circle"), (SKETCH_2D, "RMB+Down", "sketch_dimension"),
    (SKETCH_2D, "RMB+Left", "sketch_line"), (SKETCH_2D, "RMB+UpLeft", "sketch_trim"),
]

NX_PAIRS: Pairs = [
    (GLOBAL, "Home", "fit_to_window"),
    (GLOBAL, "Ctrl+F", "command_finder"),
    (GLOBAL, "F1", "help"),
    (GLOBAL, "F4", "orient_view"),
    (GLOBAL, "Z", "normal_to"),
    # решение 18.10: «C» в эскизе — окружность, ограничения без клавиши
    *_both_sketches([("L", "sketch_line"), ("C", "sketch_circle"), ("R", "sketch_rectangle"),
                     ("D", "sketch_rapid_dimension"), ("Q", "finish_sketch")]),
    (GLOBAL, "X", "extrude"), (GLOBAL, "H", "hole"), (GLOBAL, "F", "edge_blend"), (GLOBAL, "K", "chamfer"),
    (GLOBAL, "U", "unite"), (GLOBAL, "S", "subtract"), (GLOBAL, "Shift+S", "shortcut_bar"), (GLOBAL, "Ctrl+T", "move_object"), (GLOBAL, "Ctrl+B", "update"),
    (ASSEMBLY, "Ctrl+Shift+A", "add_component"), (ASSEMBLY, "Alt+Click", "mate_or_select_geometry"),
    (ASSEMBLY, "Ctrl+Shift+G", "hide_selected"), (ASSEMBLY, "Ctrl+Shift+B", "show_all"),
    (ASSEMBLY, "Alt+F2", "assembly_isometric"),
    (GLOBAL, "Ctrl+N", "new_file"), (GLOBAL, "Ctrl+O", "open_project"), (GLOBAL, "Ctrl+S", "save"),
    (GLOBAL, "Ctrl+Z", "undo"), (GLOBAL, "Ctrl+Y", "redo"), (GLOBAL, "Ctrl+Shift+S", "save_as"),
    # мышь
    (GLOBAL, "MMB+Click", "confirm_command"),
    (GLOBAL, "Ctrl+Shift+LMB", "radial_menu_1"), (GLOBAL, "Ctrl+Shift+MMB", "radial_menu_2"),
    (GLOBAL, "Ctrl+Shift+RMB", "radial_menu_3"),
]

CREO_PAIRS: Pairs = [
    (GLOBAL, "Ctrl+D", "default_view"),
    (GLOBAL, "Ctrl+F", "find"),                       # решение 18.10: поиск; скругление без клавиши
    (GLOBAL, "Ctrl+R", "repaint"),                    # решение 18.10: обновить экран; вращение без клавиши
    (GLOBAL, "F5", "selection_filter"),
    *_both_sketches([("L", "sketch_line"), ("C", "sketch_circle"), ("R", "sketch_rectangle"),
                     ("S>L", "sketch_line"), ("S>C", "sketch_circle"), ("S>R", "sketch_rectangle"),
                     ("S>D", "sketch_dimension"), ("S>V", "sketch_constraints"), ("S>T", "sketch_trim"),
                     ("MMB+Click", "confirm_command")]),
    (GLOBAL, "Ctrl+E", "extrude"), (GLOBAL, "Ctrl+C", "copy"), (GLOBAL, "Ctrl+V", "paste"), (GLOBAL, "Delete", "delete"),
    (ASSEMBLY, "Ctrl+Shift+A", "activate_component"), (ASSEMBLY, "Tab", "hide_component"),
    (ASSEMBLY, "Ctrl+Alt+LMB+Drag", "move_component_free"), (ASSEMBLY, "Ctrl+Alt+RMB+Drag", "rotate_component"),
    (GLOBAL, "Ctrl+A", "regenerate"), (GLOBAL, "Ctrl+N", "new_document"), (GLOBAL, "Ctrl+O", "open_file"),
    (GLOBAL, "Ctrl+S", "save"), (GLOBAL, "Ctrl+Z", "undo"), (GLOBAL, "Ctrl+Y", "redo"),
]


CATIA_PAIRS: Pairs = [
    # общие (справочник CATIA, раздел «Общие и системные»)
    (GLOBAL, "Ctrl+N", "new_document"), (GLOBAL, "Ctrl+O", "open_file"), (GLOBAL, "Ctrl+S", "save"),
    (GLOBAL, "Ctrl+Z", "undo"), (GLOBAL, "Ctrl+Y", "redo"), (GLOBAL, "Ctrl+F", "find"), (GLOBAL, "Ctrl+P", "print"),
    # 3D и сборки
    (GLOBAL, "F3", "toggle_tree"), (GLOBAL, "Shift+F3", "switch_focus"), (GLOBAL, "Space", "view_selector"),
    (GLOBAL, "Ctrl+F3", "snap_toggle"),           # решение пользователя 18.10.2026: привязки на Ctrl+F3
    (ASSEMBLY, "Alt+Click", "hide_temporarily"),
    (GLOBAL, "DoubleClick", "edit_path_sketch"),  # решение пользователя 18.10.2026: двойной клик по трубе — эскиз траектории
]

KEYSET_PAIRS: Dict[str, Pairs] = {
    "CATIA": CATIA_PAIRS,
    "SolidWorks": SOLIDWORKS_PAIRS,
    "Inventor": INVENTOR_PAIRS,
    "Siemens NX": NX_PAIRS,
    "PTC Creo": CREO_PAIRS,
    # «Стандарт программы» (6.14): состав не описан в спецификации; решение пользователя 18.10.2026 — копия SolidWorks
    "Стандарт программы": SOLIDWORKS_PAIRS,
}
# стиль навигации, одноимённый набору; для копии — стиль SolidWorks
NAV_OF_KEYSET = {"Стандарт программы": "SolidWorks"}
DEFAULT_KEYSET = "SolidWorks"          # 6.13


def keyset(name: str) -> KeySet:
    ks, report = load_pairs(name, KEYSET_PAIRS[name])
    if report:
        raise AssertionError(f"набор {name} содержит конфликты: " + "; ".join(report))
    return ks


def solidworks() -> KeySet:
    return keyset("SolidWorks")


def inventor() -> KeySet:
    return keyset("Inventor")


def nx() -> KeySet:
    return keyset("Siemens NX")


def creo() -> KeySet:
    return keyset("PTC Creo")


# ---------- 3. Программные привязки (команды программы, 6.4, 6.5, 6.6, 6.14) ----------
# (контекст, варианты сочетаний по порядку, команда). Берётся первый свободный вариант.
PROGRAM_BINDINGS: List[Tuple[str, List[str], str]] = [
    (GLOBAL, ["Enter"], "confirm_command"),                 # 6.6: Enter = ОК
    (GLOBAL, ["Esc"], "cancel_command"),                    # 6.6: Esc = отмена
    (GLOBAL, ["RMB+Click"], "context_menu"),                # 6.6, 6.14: ПКМ — контекстное / мини-меню
    (GLOBAL, ["MMB+DoubleClick"], "view_fit"),              # 6.4: двойной клик колесом — «Показать всё»
    (GLOBAL, ["Alt+Click"], "select_other"),                # 6.5: выбор перекрытых объектов списком
    (GLOBAL, ["S"], "shortcut_bar"),                        # 6.14: панель по клавише S у курсора
    (GLOBAL, ["F3"], "snap_toggle"),                        # 6.14, 6.13: привязки вкл/выкл (F3)
    (GLOBAL, ["Shift+RMB+Click"], "snap_menu"),             # 6.14: меню привязок
    # жесты по умолчанию (6.14): стандартные виды в модели, Линия и Размер в эскизе
    (GLOBAL, ["RMB+Up"], "view_top"), (GLOBAL, ["RMB+Right"], "view_right"),
    (GLOBAL, ["RMB+Down"], "view_front"), (GLOBAL, ["RMB+Left"], "view_left"),
    (PATH_SKETCH, ["RMB+Up"], "sketch_dimension"), (PATH_SKETCH, ["RMB+Down"], "sketch_line"),
    (SKETCH_2D, ["RMB+Up"], "sketch_dimension"), (SKETCH_2D, ["RMB+Down"], "sketch_line"),
    # разовые привязки во время ввода точки (6.14)
    (POINT_INPUT, ["Alt+E"], "snap_end"), (POINT_INPUT, ["Alt+M"], "snap_mid"),
    (POINT_INPUT, ["Alt+C"], "snap_center"), (POINT_INPUT, ["Alt+V"], "snap_vertex"),
    (POINT_INPUT, ["Alt+T"], "snap_tangent"), (POINT_INPUT, ["Alt+A"], "snap_axis"),
    (POINT_INPUT, ["Alt+O"], "snap_port"), (POINT_INPUT, ["Alt+I"], "snap_intersection"),
    (POINT_INPUT, ["Alt+P"], "snap_perpendicular"), (POINT_INPUT, ["Alt+N"], "snap_nearest"),
    (POINT_INPUT, ["F10"], "snap_menu"),
    (POINT_INPUT, ["Tab"], "snap_cycle"),      # 6.14; в 1.2 Tab переключает поля у курсора — вопрос (README)
    # 1.2: блокировка оси клавишами X/Y/Z, подъём по нормали стрелками
    (POINT_INPUT, ["X"], "lock_axis_x"), (POINT_INPUT, ["Y"], "lock_axis_y"), (POINT_INPUT, ["Z"], "lock_axis_z"),
    (POINT_INPUT, ["Up"], "raise_along_normal"), (POINT_INPUT, ["Down"], "lower_along_normal"),
]
# Удержание Ctrl во время рисования — временно без привязок и автоограничений (6.14); не привязка, а режим ввода.
HOLD_CTRL_SUSPENDS_SNAPS = True
SNAPS_ON_BY_DEFAULT = True             # 6.13


def _is_mouse(norm: str) -> bool:
    return any(b in norm.split("+") for b in ("LMB", "MMB", "RMB"))


def _same_command(a: str, b: str) -> bool:
    """Одна команда с учётом синонимов реестра (smart_dimension = sketch_dimension)."""
    try:
        from commands import canonical
        return canonical(a) == canonical(b)
    except (ImportError, ValueError):
        return a == b


def _program_layer(taken: KeySet) -> Tuple[KeySet, List[str]]:
    ks = KeySet("программа")
    report: List[str] = []
    for ctx, candidates, cmd in PROGRAM_BINDINGS:
        # Команда уже доступна в наборе в этом контексте тем же способом (клавиша / мышь) — своя привязка не нужна:
        # Enter в NX остаётся нужен (там ОК — клик колесом), а S в SolidWorks уже есть.
        mouse = _is_mouse(normalize(candidates[0]))
        if any(k[0] == ctx and _is_mouse(k[1]) == mouse and _same_command(c, cmd)
               for k, c in taken.bindings.items()):
            continue
        for combo in candidates:
            key = (ctx, normalize(combo))
            if key not in taken.bindings and key not in ks.bindings:
                ks.bindings[key] = cmd
                break
        else:
            report.append(f"«{cmd}»: нет свободной клавиши в контексте {ctx} "
                          f"(занято: {', '.join(normalize(c) for c in candidates)})")
    return ks, report


def effective(keyset_name: str = DEFAULT_KEYSET, navigation_style: Optional[str] = None) -> Tuple[KeySet, List[str]]:
    """Действующие привязки: набор клавиш + стиль навигации + программные команды на свободных клавишах.
    Стиль навигации по умолчанию — одноимённый набору (6.13 стиль не задаёт; см. README)."""
    nav_style = navigation_style or NAV_OF_KEYSET.get(keyset_name, keyset_name)
    base, report = _merge(f"{keyset_name} / навигация {nav_style}",
                          [keyset(keyset_name), navigation(nav_style)])
    prog, prog_report = _program_layer(base)
    merged, _ = _merge(base.name, [base, prog])
    return merged, report + prog_report


def reset(keyset_name: str) -> KeySet:
    """«Сброс к выбранному набору» (6.14)."""
    return keyset(keyset_name)
