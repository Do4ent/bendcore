"""Этап 5, реестр команд. Спецификация 6.6 (единый слой команд) и 6.14 (лента, горячие клавиши).

Канонический идентификатор команды → подпись. Идентификаторы наборов клавиш и жестов
(keymap.py) сводятся к каноническим через ALIASES: одно и то же действие в разных наборах — одна команда,
значок и подпись. Значки темы (icons.py) ищутся по каноническому идентификатору.
"""
from __future__ import annotations

from typing import Dict, Optional

TITLES: Dict[str, str] = {
    # документ и файл
    "new_document": "Создать", "open_file": "Открыть", "save": "Сохранить", "save_as": "Сохранить как",
    "undo": "Отменить", "redo": "Повторить", "copy": "Копировать", "paste": "Вставить", "delete": "Удалить",
    "rebuild": "Перестроить", "force_rebuild": "Полное перестроение", "repaint": "Обновить экран",
    # вид
    "view_fit": "Вписать в окно", "view_front": "Спереди", "view_back": "Сзади", "view_left": "Слева",
    "view_right": "Справа", "view_top": "Сверху", "view_bottom": "Снизу", "view_isometric": "Изометрия",
    "view_normal_to": "Перпендикулярно", "view_selector": "Селектор видов", "previous_view": "Предыдущий вид",
    "look_at": "Взгляд на", "orient_view": "Ориентация вида", "zoom": "Масштаб", "zoom_in": "Увеличить",
    "zoom_out": "Уменьшить", "pan": "Панорама", "rotate": "Вращение", "rotate_about_screen_normal": "Вращение вокруг нормали экрана",
    "selection_filter": "Фильтры выбора", "slice_graphics": "Рассечь графику",
    # интерфейс
    "shortcut_bar": "Меню ярлыков", "marking_menu": "Маркерное меню", "radial_menu_1": "Радиальное меню 1",
    "radial_menu_2": "Радиальное меню 2", "radial_menu_3": "Радиальное меню 3", "command_finder": "Найти команду",
    "find": "Найти объект", "help": "Справка", "toggle_command_panel": "Показать / скрыть панель команд",
    "confirm_command": "ОК", "cancel_command": "Отмена", "confirm_breadcrumbs": "Подтвердить / выход",
    # эскиз
    "new_2d_sketch": "Новый эскиз", "sketch_line": "Линия", "sketch_arc": "Дуга", "sketch_circle": "Окружность",
    "sketch_rectangle": "Прямоугольник", "sketch_dimension": "Размер", "sketch_constraints": "Ограничения",
    "sketch_trim": "Обрезать", "sketch_toolbar": "Панель инструментов эскиза", "finish_sketch": "Завершить эскиз",
    "note": "Выносная надпись", "projected_view": "Проекционный вид", "section_view": "Разрез",
    # моделирование
    "extrude": "Выдавливание", "revolve": "Вращение (тело)", "fillet": "Скругление", "chamfer": "Фаска",
    "hole": "Отверстие", "rectangular_pattern": "Прямоугольный массив", "circular_pattern": "Круговой массив",
    "subtract": "Вычитание", "unite": "Объединение", "move_object": "Переместить объект", "measure": "Измерить",
    # сборка
    "add_component": "Добавить компонент", "activate_component": "Активировать компонент",
    "assembly_constraint": "Зависимости", "joint": "Соединение", "move_component": "Переместить компонент",
    "rotate_component": "Вращение компонента", "copy_component": "Копировать компонент", "smart_mate": "Умные сопряжения",
    "mate_or_select_geometry": "Сопряжение / выбор геометрии", "hide_component": "Скрыть / показать компонент",
    "show_component": "Показать компонент", "hide_selected": "Скрыть выбранное", "show_all": "Показать все",
    "toggle_transparency": "Прозрачность",
}

# Синонимы: идентификатор из набора → каноническая команда (одно действие — одна команда).
ALIASES: Dict[str, str] = {
    "fit_to_screen": "view_fit", "fit_to_window": "view_fit", "zoom_all": "view_fit",
    "rebuild": "rebuild", "update": "rebuild", "regenerate": "rebuild",
    "new_file": "new_document", "open_project": "open_file",
    "selection_filter_panel": "selection_filter",
    "smart_dimension": "sketch_dimension", "sketch_rapid_dimension": "sketch_dimension",
    "normal_to": "view_normal_to",
    "edge_blend": "fillet",
    "home_view": "view_isometric", "default_view": "view_isometric", "assembly_isometric": "view_isometric",
    "orbit": "rotate",
    "quick_mate": "smart_mate",
    "place_component": "add_component",
    "hide_show_component": "hide_component",
    "move_component_free": "move_component",
}


def canonical(command_id: str) -> str:
    """Каноническое имя команды; неизвестные — ValueError."""
    c = ALIASES.get(command_id, command_id)
    if c not in TITLES:
        raise ValueError(f"нет команды в реестре: {command_id}")
    return c


def title(command_id: str) -> str:
    return TITLES[canonical(command_id)]


# Команды лентного состава вкладок (спецификация 6.1, 5.9 — описания вкладок «Труба», «Вывод», «Сборка», меню «Файл»)
# CATIA (справочник 18.10.2026): команды, которых не было в реестре
TITLES.update({
    "toggle_tree": "Дерево: показать / скрыть",
    "switch_focus": "Переключить фокус: дерево / графика",
    "hide_temporarily": "Временно скрыть",
    "sketch_analysis": "Анализ эскиза",
    "edit_path_sketch": "Открыть эскиз траектории трубы",
})

TITLES.update({
    "new_tube": "Новая труба", "tube_by_points": "Труба по точкам", "typical_shape": "Типовая форма",
    "bundle": "Пучок", "mirror_tube": "Зеркальная труба", "edit_spec": "Изменить спецификацию",
    "bend_table": "Таблица гибов", "tube_from_table": "Труба из таблицы", "tube_from_curve": "Труба из кривой",
    "tube_from_file": "Труба из файла", "libraries": "Библиотеки", "bend_map": "Карта гибки",
    "release_to_production": "Выдать в производство", "plane": "Плоскость", "axis": "Ось", "point": "Точка",
    "coordinate_system": "СК", "import_file": "Импорт", "port": "Порт", "support": "Опора",
    "refresh": "Обновить", "adaptive_freeze": "Адаптивно / Заморозить", "reference_manager": "Менеджер ссылок",
    "forbidden_zone": "Запретная зона", "replace_spec": "Заменить спецификацию",
    "exclude_restore": "Исключить / Восстановить", "set_color": "Цвет", "reset_position": "Вернуть исходное положение",
    "reset_my_edits": "Сбросить мои правки", "recognize": "Распознать", "convert_to_tube": "Преобразовать в трубу",
    "return_to_body": "Вернуть в тело", "refresh_import": "Обновить импорт", "load_more": "Дозагрузить",
    "load_neighborhood": "Дозагрузить окрестность", "round_geometry": "Округлить геометрию трубы",
    "fit_to_machine": "Подогнать к станку",
})


# Сверка с полным текстом 6.14 (18.10.2026): меню, CommandManager, привязки
TITLES.update({
    # Труба: правка и заготовка
    "path_source": "Источник пути", "trim_allowance": "Подрезка", "feed_from_end": "Подача с конца",
    "minimize_rotation": "Минимизировать поворот", "springback_compensation": "Компенсация пружинения",
    # Эскиз
    "spiral": "Спираль", "construction_geometry": "Вспомогательная", "active_plane": "Активная плоскость",
    "project_geometry": "Спроецировать",
    # Анализ
    "check": "Проверка", "collisions": "Коллизии", "mass": "Масса", "report": "Отчёт",
    # Вывод и файл
    "export": "Экспорт", "export_3d": "Экспорт 3D", "print": "Печать", "recent": "Недавние", "close": "Закрыть",
    "save_copy": "Сохранить копию", "pack_project": "Упаковать проект", "document_properties": "Свойства документа",
    "exit": "Выход",
    # Правка
    "cut": "Вырезать", "suppress_restore": "Подавить / Восстановить", "rename": "Переименовать",
    "select_all": "Выбрать всё",
    # Вид
    "show_selected": "Показать выбранное", "section": "Сечение", "shading_mode": "Режим тонировки",
    "projection": "Проекция", "saved_views": "Сохранённые виды", "visibility_types": "Видимость типов объектов",
    "isolate": "Изолировать", "full_screen": "Полноэкранный режим", "toolbars": "Панели инструментов",
    # Вставка
    "sketch": "Эскиз", "component": "Компонент", "drawing_view": "Вид", "table": "Таблица", "annotation": "Аннотация",
    # Инструменты, окно, справка
    "standards_manager": "Менеджер стандартов", "customize_ui": "Настройка интерфейса", "options": "Параметры",
    "new_window": "Новое окно", "tile": "Плитка", "cascade": "Каскад", "window_layouts": "Раскладки",
    "window_list": "Список окон", "hotkeys_list": "Горячие клавиши", "about": "О программе",
    # Чертёж и аннотации
    "sheets": "Листы", "drawing_views": "Виды", "tables": "Таблицы", "flat_sheet": "Лист развёрток",
    "drawing_set": "Комплект", "release": "Выпуск", "tube_dimensions": "Размеры трубы",
    "drawing_dimension": "Размер", "bend_label": "Метка гиба", "leader": "Выноска", "balloon": "Позиция", "text": "Текст",
    # Привязки (6.14, «Клавиши привязок»)
    "snap_toggle": "Привязки вкл/выкл", "snap_end": "Привязка: конец или торец", "snap_mid": "Привязка: середина",
    "snap_center": "Привязка: центр", "snap_vertex": "Привязка: острая вершина", "snap_tangent": "Привязка: точка касания гиба",
    "snap_axis": "Привязка: ось отверстия или цилиндра", "snap_port": "Привязка: порт",
    "snap_intersection": "Привязка: пересечение", "snap_perpendicular": "Привязка: перпендикуляр",
    "snap_nearest": "Привязка: ближайшая на объекте", "snap_menu": "Меню привязок", "snap_cycle": "Перебор целей под курсором",
})


# Сверка с 1.4, 6.4–6.6, 7.9 (18.10.2026)
TITLES.update({
    # 6.5 выбор, 6.4 вид, 6.6 модель команд
    "select_other": "Выбрать перекрытый", "select_chain": "Выбрать цепочку", "next_view": "Следующий вид",
    "context_menu": "Контекстное меню", "repeat_last_command": "Повторить последнюю команду",
    "exit_sketch": "Выйти из эскиза", "discard_sketch_changes": "Отменить изменения",
    # 1.4 команды правки эскиза (общие для эскиза пути, вспомогательной геометрии и 2D-эскиза чертежа)
    "sketch_extend": "Удлинить", "sketch_split": "Разбить", "insert_vertex": "Вставить вершину",
    "sketch_join": "Соединить", "sketch_fillet": "Скругление", "sketch_chamfer": "Фаска", "cut_corner": "Срезать угол",
    "sketch_offset": "Смещение", "sketch_move": "Перенос", "sketch_copy": "Копировать", "sketch_rotate": "Поворот",
    "sketch_mirror": "Зеркало", "sketch_pattern_linear": "Линейный массив", "sketch_pattern_circular": "Круговой массив",
    "sketch_scale": "Масштаб", "sketch_stretch": "Растянуть",
    # 7.9 2D-эскиз чертежа
    "sketch_polyline": "Полилиния", "sketch_ellipse": "Эллипс", "sketch_spline": "Сплайн", "sketch_point": "Точка",
    "sketch_centerline": "Осевая", "hatch": "Штриховка", "sketch_on_view": "Эскиз на виде",
    "sketch_on_sheet": "Эскиз на листе", "project_view_geometry": "Спроецировать геометрию вида",
    "bind_to_view": "Привязать к виду", "unbind_from_view": "Отвязать",
})

# Недоступные команды: серые, причина в подсказке (6.14).
DISABLED: Dict[str, str] = {"export_3d": "Будет в следующей версии"}


def enter_action(active_command: bool) -> str:
    """Enter (6.6, 1.4): в активной команде — ОК, без команды — повтор последней команды."""
    return "confirm_command" if active_command else "repeat_last_command"


def esc_action(active_command: bool, in_sketch: bool, has_changes: bool = False) -> Optional[str]:
    """Esc (6.6, 1.2): в команде — отмена (выход из инструмента); без команды в эскизе — выход из эскиза.
    Решение 18.10.2026: без изменений выход без подтверждения; с изменениями — запрос подтверждения."""
    if active_command:
        return "cancel_command"
    if not in_sketch:
        return None
    return "confirm_exit_sketch" if has_changes else "exit_sketch"


def tooltip(command_id: str, keyset=None) -> str:
    """Подсказка кнопки: название, клавиша по текущему набору, причина недоступности (6.14)."""
    c = canonical(command_id)
    text = TITLES[c]
    if keyset is not None:
        keys = [combo for ctx, combo in keyset.bindings if _canonical_or_none(keyset.bindings[(ctx, combo)]) == c]
        if keys:
            from keymap import label
            text += f" ({label(sorted(keys)[0])})"
    if c in DISABLED:
        text += f" — недоступно: {DISABLED[c]}"
    return text


def _canonical_or_none(cmd: str) -> Optional[str]:
    try:
        return canonical(cmd)
    except ValueError:
        return None


# Варианты разделённых кнопок и пункты, названные в 6.1, 6.14, 5.7, 1.8
TITLES.update({
    "new_detail": "Деталь", "new_assembly": "Сборка", "new_drawing": "Чертёж",
    "export_pdf": "PDF", "export_dwg_dxf": "DWG / DXF", "export_csv": "CSV",
    "lib_nomenclature": "Номенклатура", "lib_materials": "Материалы", "lib_benders": "Трубогибы",
    "lib_equipment": "Оборудование", "lib_insulation": "Изоляция", "lib_systems": "Системы",
    "lib_presets": "Пресеты", "lib_property_templates": "Шаблоны свойств",
    "recognize_tubes": "Трубы", "recognize_environment": "Окружение", "recognize_all": "Всё",
    "show_ports": "Порты", "show_port_candidates": "Кандидаты в порты", "show_aux_geometry": "Вспомогательная геометрия",
    "show_reference_geometry": "Опорная геометрия", "show_environment": "Окружение", "show_insulation": "Изоляция",
    "show_forbidden_zones": "Запретные зоны", "show_violations": "Нарушения",
    "update_component": "Обновить компонент",               # 5.7
    "zero_small_rotation": "Обнулить малый поворот",       # 1.8
})
# «Обновить» в 5.9 — это «Обновить компонент» (5.7)
ALIASES["refresh"] = "update_component"
del TITLES["refresh"]


# Компоновка и строка состояния (6.4, 6.14)
TITLES.update({
    "flip_port": "Развернуть", "properties": "Свойства", "show_dependents": "Показать зависимые",
    "hide_isolate": "Скрыть / Изолировать", "autoconstraints": "Автоограничения", "ortho_mode": "Ортогональный режим",
    "check_on_the_fly": "Проверять на лету", "auto_recompute": "Автопересчёт",
})


# Ввод точки (1.2)
TITLES.update({
    "lock_axis_x": "Блокировка оси X", "lock_axis_y": "Блокировка оси Y", "lock_axis_z": "Блокировка оси Z",
    "raise_along_normal": "Поднять по нормали", "lower_along_normal": "Опустить по нормали",
    "switch_cursor_field": "Следующее поле у курсора",
})
