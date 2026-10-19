import layout as L
import settings as S
import icons as I
import commands as c


def test_task_panel_tabs_by_document():
    assert L.task_panel_tabs(L.DETAIL) == ["Свойства", "Проверка", "Библиотеки", "Проводник файлов", "Журнал", "Уведомления"]
    assert "Палитра видов" in L.task_panel_tabs(L.DRAWING) and "Проверка" not in L.task_panel_tabs(L.DRAWING)


def test_task_panel_pin_and_autocollapse():
    p = L.TaskPanel(L.DETAIL)
    p.click("Свойства"); assert p.open_tab == "Свойства"
    p.cursor_to_graphics(); assert p.open_tab is None
    p.pinned = True; p.click("Журнал"); p.cursor_to_graphics(); assert p.open_tab == "Журнал"
    p.click("Журнал"); assert p.open_tab is None


def test_status_bar():
    sb = L.StatusBar(); sb.toggle("cursor"); assert "cursor" not in sb.visible
    assert L.sketch_state_text(0) == "Определён"
    assert L.sketch_state_text(3) == "Недоопределён: 3 степеней свободы"
    assert L.selection_text("bend", {"B": 90, "R": 40}) == "B = 90, R = 40"


def test_context_menu():
    assert L.context_menu("tube", in_command=True) == ["confirm_command", "cancel_command", "repeat_last_command"]
    assert L.context_menu("tube")[:2] == ["edit_spec", "bend_table"]
    assert L.context_menu("empty") == ["paste", "select_all", "view_fit"] + L.CONTEXT_MENU_TAIL   # предложение 18.10
    for t in L.OBJECT_TYPES:
        for cmd in L.context_menu(t):
            assert c.canonical(cmd) in c.TITLES
    for cmd in L.STATUS_TOGGLES:
        assert c.canonical(cmd) in c.TITLES


def test_settings_defaults_6_13():
    s = S.Settings()
    assert s.value("keyboard.keyset") == "SolidWorks" and s.value("icons.theme") == "Цветная"
    assert s.value("keyboard.snaps_on") is True


def test_enterprise_lock():
    s = S.Settings()
    s.lock("icons.theme", "Монохромная")
    assert s.get("icons.theme") == ("Монохромная", "enterprise", True)
    try:
        s.set_user("icons.theme", "Цветная")
    except PermissionError as e:
        assert str(e) == "Задано предприятием"
    else:
        raise AssertionError
    assert s.import_({"icons.theme": "Цветная", "keyboard.keyset": "Inventor"}) == {"icons.theme": "Задано предприятием"}
    assert s.value("keyboard.keyset") == "Inventor"
    s.reset("keyboard"); assert s.value("keyboard.keyset") == "SolidWorks"


def test_custom_icon_layers_and_checks():
    base = I.IconSet("base", {"save": I.Icon("save", "<svg>{{outline}}</svg>")})
    ent = {"save": I.CustomIcon("save", "png", "corp.png", 64, 64)}
    mine = {"save": I.CustomIcon("save", "svg", "<svg>{{accent}}</svg>")}
    assert I.resolve_layers("save", base, None, ent, mine) is mine["save"]
    assert I.resolve_layers("save", base, None, ent, {}) is ent["save"]
    assert I.resolve_layers("save", base, None).svg.startswith("<svg>")
    assert I.recolors_with_theme(mine["save"]) and not I.recolors_with_theme(ent["save"])
    assert I.check_custom(I.CustomIcon("x", "png", "a.png", 16, 16))[0]
    assert I.check_custom(I.CustomIcon("x", "png", "a.png", 48, 48))[1]
    assert I.check_custom(I.CustomIcon("x", "svg", "<svg><script/></svg>"))[0]


def test_theme_suggested_by_keyset():
    assert I.suggest_theme("Inventor", {"Синяя": {"keyset": "Inventor"}, "Серая": {}}) == "Синяя"
    assert I.suggest_theme("NX", {"Синяя": {"keyset": "Inventor"}}) is None


def test_context_menu_proposed_sets_18_10():
    import layout as L
    for t in ("segment", "component", "view", "empty"):
        menu = L.context_menu(t)
        assert menu[-3:] == L.CONTEXT_MENU_TAIL, t          # хвост одинаковый
        assert len(menu) > len(L.CONTEXT_MENU_TAIL), t      # частые действия есть
    assert L.context_menu("view", in_command=True)[0] == "confirm_command"


def test_sketch_colors_1_3():
    assert L.sketch_color(0) == "чёрный"
    assert L.sketch_color(2) == "синий"
    assert L.sketch_color(0, conflict=True) == "красный"
    assert L.sketch_color(2, reference=True) == "фиолетовый"
