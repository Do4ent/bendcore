"""Лента, меню и панели команд. Спецификация 6.14 («Компоновка окна», «Строка меню», «CommandManager»,
«Панель вида»), с учётом 5.7, 5.9, 6.1, 1.4, 1.8, 7.9.

Элемент группы — канонический идентификатор команды (commands.py) или разделённая кнопка Split
(основное действие + варианты, 6.14). Видимость пунктов меню зависит от типа документа: «показываются
команды, применимые к текущему документу» (6.14).
"""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Dict, List, Optional, Sequence, Tuple, Union

DETAIL, ASSEMBLY, DRAWING = "detail", "assembly", "drawing"
ALL_DOCS = (DETAIL, ASSEMBLY, DRAWING)
MODEL_DOCS = (DETAIL, ASSEMBLY)


@dataclass(frozen=True)
class Split:
    """Разделённая кнопка: основное действие + варианты (6.14)."""
    main: str
    variants: Tuple[str, ...]


Item = Union[str, Split]


@dataclass
class Group:
    name: str
    items: List[Item]


@dataclass
class Tab:
    name: str
    groups: List[Group]
    contextual: bool = False          # «Эскиз» — контекстная вкладка, выделена цветом (6.14)


# ---------- Панель быстрого доступа (6.14) ----------
QUICK_ACCESS: List[str] = ["new_document", "open_file", "save", "undo", "redo", "rebuild", "print"]

# ---------- Строка меню (6.14): (команда, типы документов; None — все) ----------
MenuItem = Tuple[Item, Optional[Sequence[str]]]
MENU_BAR: Dict[str, List[MenuItem]] = {
    "Файл": [
        (Split("new_document", ("new_detail", "new_assembly", "new_drawing")), None),
        ("open_file", None), ("recent", None), ("close", None), ("save", None), ("save_as", None),
        ("save_copy", None), ("pack_project", None), ("import_file", None), ("tube_from_file", MODEL_DOCS),
        (Split("export", ("export_pdf", "export_dwg_dxf", "export_csv")), None),
        ("export_3d", MODEL_DOCS),                          # заглушка: видна, недоступна (6.14)
        ("print", None), ("document_properties", None), ("exit", None),
    ],
    "Правка": [
        ("undo", None), ("redo", None), ("cut", None), ("copy", None), ("paste", None), ("delete", None),
        ("suppress_restore", MODEL_DOCS), ("rename", None), ("select_all", None), ("edit_spec", MODEL_DOCS),
    ],
    "Вид": [
        (Split("view_front", ("view_back", "view_top", "view_bottom", "view_left", "view_right", "view_isometric")), MODEL_DOCS),
        ("shading_mode", MODEL_DOCS), ("hide_component", MODEL_DOCS), ("isolate", MODEL_DOCS),
        ("section", MODEL_DOCS), ("saved_views", MODEL_DOCS), ("toolbars", None), ("full_screen", None),
    ],
    "Вставка": [
        ("new_tube", MODEL_DOCS), (Split("tube_from_table", ("tube_from_curve", "tube_from_file")), MODEL_DOCS),
        ("sketch", None), (Split("plane", ("axis", "point", "coordinate_system")), MODEL_DOCS),
        ("port", MODEL_DOCS), ("support", MODEL_DOCS),
        ("component", (ASSEMBLY,)), ("forbidden_zone", (ASSEMBLY,)),
        ("drawing_view", (DRAWING,)), ("table", (DRAWING,)), ("annotation", (DRAWING,)),
    ],
    "Инструменты": [
        ("sketch", None), ("check", MODEL_DOCS), ("libraries", None), ("standards_manager", None),
        ("customize_ui", None), ("options", None),
    ],
    "Окно": [("new_window", None), ("tile", None), ("cascade", None), ("window_layouts", None), ("window_list", None)],
    "Справка": [("help", None), ("hotkeys_list", None), ("about", None)],
}

# ---------- CommandManager (6.14) ----------
AUX_GEOMETRY = Group("Вспомогательная геометрия", ["plane", "axis", "point", "coordinate_system"])   # 1.7

LIBRARIES = Split("libraries", ("lib_nomenclature", "lib_materials", "lib_benders", "lib_equipment",
                                "lib_insulation", "lib_systems", "lib_presets", "lib_property_templates"))   # 6.1

SKETCH_EDIT_1_4 = ["delete", "sketch_trim", "sketch_extend", "sketch_split", "sketch_join", "sketch_fillet",
                   "sketch_chamfer", "sketch_offset", "sketch_move", "sketch_copy", "sketch_rotate", "sketch_mirror",
                   Split("sketch_pattern_linear", ("sketch_pattern_circular",)), "sketch_scale", "sketch_stretch"]

TUBE_TAB = Tab("Труба", [
    Group("Создание", ["new_tube", "tube_by_points", "typical_shape", "bundle", "mirror_tube",
                       "tube_from_table", "tube_from_curve", "tube_from_file"]),
    Group("Правка", ["edit_spec", "bend_table", "path_source"]),
    Group("Заготовка", ["trim_allowance", "feed_from_end", "minimize_rotation", "springback_compensation"]),
    AUX_GEOMETRY,
    Group("", ["round_geometry", Split("fit_to_machine", ("zero_small_rotation",)), LIBRARIES]),
])

PATH_SKETCH_TAB = Tab("Эскиз", [
    Group("Построение", ["sketch_line", "spiral"]),
    # вспомогательная геометрия эскиза (1.3): отрезок, прямая и луч, точка, окружность и дуга, прямоугольник, сплайн
    Group("Вспомогательная", ["construction_geometry", "sketch_circle", "sketch_arc", "sketch_rectangle",
                              "sketch_point", "sketch_spline"]),
    Group("Ограничения и размеры", ["sketch_constraints", "sketch_dimension"]),
    Group("Правка", SKETCH_EDIT_1_4),
    Group("Плоскость и привязки", ["active_plane", "snap_toggle"]),
    Group("", ["project_geometry", "finish_sketch"]),
], contextual=True)

ANALYSIS_TAB = Tab("Анализ", [Group("Анализ", ["check", "collisions", "measure", "mass", "report"])])

OUTPUT_TAB = Tab("Вывод", [Group("Вывод", ["bend_map", "release_to_production", "export", "export_3d"])])

ASSEMBLY_TAB = Tab("Сборка", [
    Group("Компоненты", ["add_component", "import_file"]),
    Group("Трубы", ["new_tube", "port", "support", "forbidden_zone", "replace_spec"]),
    Group("Импорт", [Split("recognize", ("recognize_tubes", "recognize_environment", "recognize_all")),
                     "convert_to_tube", "return_to_body", "refresh_import",
                     Split("load_more", ("load_neighborhood",))]),
    Group("Правка импорта", ["exclude_restore", "delete", "set_color", "toggle_transparency", "move_component",
                             "reset_position", "reset_my_edits"]),
    # «Обновить компонент» (5.7, 5.9) — во вкладке «Сборка» по решению пользователя 18.10.2026
    Group("Ссылки", ["reference_manager", "adaptive_freeze", "update_component"]),
    AUX_GEOMETRY,
])

DRAWING_TAB = Tab("Чертёж", [Group("Чертёж", ["sheets", "drawing_views", "tables", "flat_sheet", "drawing_set",
                                              "release", "measure"])])
ANNOTATIONS_TAB = Tab("Аннотации", [Group("Аннотации", ["tube_dimensions", "drawing_dimension", "bend_label",
                                                        "leader", "balloon", "text"])])
SKETCH_2D_TAB = Tab("Эскиз", [
    Group("Создание", ["sketch_on_view", "sketch_on_sheet", "bind_to_view", "unbind_from_view"]),
    Group("Построение", ["sketch_line", "sketch_arc", "sketch_circle", "sketch_rectangle", "sketch_polyline",
                         "sketch_ellipse", "sketch_spline", "sketch_point", "sketch_centerline", "construction_geometry"]),
    Group("Ограничения и размеры", ["sketch_constraints", "sketch_dimension"]),
    Group("Правка", SKETCH_EDIT_1_4),
    Group("Оформление", ["hatch", "text"]),
    Group("", ["project_view_geometry", "finish_sketch"]),
], contextual=True)

COMMAND_MANAGER: Dict[str, List[Tab]] = {
    DETAIL: [TUBE_TAB, PATH_SKETCH_TAB, ANALYSIS_TAB, OUTPUT_TAB],
    ASSEMBLY: [ASSEMBLY_TAB, TUBE_TAB, PATH_SKETCH_TAB, ANALYSIS_TAB, OUTPUT_TAB],
    DRAWING: [DRAWING_TAB, ANNOTATIONS_TAB, SKETCH_2D_TAB],
}

# ---------- Панель вида (6.14) ----------
VIEW_PANEL: List[Item] = [
    "view_fit", "show_selected", "section",
    Split("view_isometric", ("view_front", "view_back", "view_top", "view_bottom", "view_left", "view_right",
                             "view_normal_to")),
    "shading_mode", "projection", "saved_views",
    Split("visibility_types", ("show_ports", "show_port_candidates", "show_aux_geometry", "show_reference_geometry",
                               "show_environment", "show_insulation", "show_forbidden_zones", "show_violations")),
]


# ---------- Вспомогательные функции ----------
def commands_of(item: Item) -> List[str]:
    return [item] if isinstance(item, str) else [item.main, *item.variants]


def visible_tabs(doc_type: str, in_sketch: bool) -> List[str]:
    """Видимые вкладки: контекстная «Эскиз» — только внутри эскиза (6.1, 6.14)."""
    return [t.name for t in COMMAND_MANAGER[doc_type] if not t.contextual or in_sketch]


def menu(doc_type: str) -> Dict[str, List[Item]]:
    """Строка меню для документа: только применимые пункты (6.14)."""
    return {name: [it for it, docs in items if docs is None or doc_type in docs] for name, items in MENU_BAR.items()}


def all_commands() -> List[str]:
    out: List[str] = []
    for tabs in COMMAND_MANAGER.values():
        for t in tabs:
            for g in t.groups:
                for it in g.items:
                    out += commands_of(it)
    for items in MENU_BAR.values():
        for it, _ in items:
            out += commands_of(it)
    for it in VIEW_PANEL:
        out += commands_of(it)
    return sorted(set(out + QUICK_ACCESS))
