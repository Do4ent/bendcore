import keymap as k
from keymap import (normalize, label, load_pairs, customize, KeySet, solidworks, inventor, nx, creo, navigation,
                    effective, reset, GLOBAL, PATH_SKETCH, SKETCH_2D, ASSEMBLY, DRAWING, POINT_INPUT)


def _raises(fn, exc=ValueError):
    try:
        fn()
    except exc:
        return True
    return False


# ---------- нормализация и подписи ----------
def test_normalize():
    assert normalize("shift+ctrl+z") == "Ctrl+Shift+Z"
    assert normalize("ctrl+ENTER") == "Ctrl+Enter"
    assert normalize("F5") == "F5" and normalize("f10") == "F10"
    assert normalize("alt+drag") == "Alt+Drag" and normalize("ALT+click") == "Alt+Click"
    assert normalize("esc") == "Esc" and normalize("page up") == "PageUp"
    assert normalize("mmb+doubleclick") == "MMB+DoubleClick"


def test_mouse_buttons_and_nx_aliases():
    assert normalize("Ctrl+MMB") == "Ctrl+MMB"
    assert normalize("Ctrl+Shift+MB1") == "Ctrl+Shift+LMB"
    assert normalize("mb2+drag") == "MMB+Drag"
    assert normalize("RMB+UpRight") == "RMB+UpRight"
    assert _raises(lambda: normalize("MB1+MB2"))          # аккорды не поддерживаются
    assert _raises(lambda: normalize("win+a"))


def test_sequences():
    assert normalize("s>l") == "S>L" and normalize("S > V") == "S>V"


def test_physical_keys_and_label():
    assert normalize("Ctrl+я") == "Ctrl+Z" and normalize("Ctrl+ф") == "Ctrl+A"
    assert label("L") == "L (Д)"                           # 6.14: подпись «L (Д)»
    assert label("Ctrl+Z") == "Ctrl+Z (Я)"
    assert label("F5") == "F5"


# ---------- наборы ----------
def test_all_sets_load_without_conflicts():
    for f in (solidworks, inventor, nx, creo):
        assert f().bindings


def test_solidworks_contexts():
    ks = solidworks()
    assert ks.command_for("S") == "shortcut_bar"
    assert ks.command_for("S", context=PATH_SKETCH) == "sketch_toolbar"
    assert ks.command_for("L", context=SKETCH_2D) == "sketch_line"
    assert ks.command_for("L") is None
    assert ks.command_for("Ctrl+Z", context=PATH_SKETCH) == "undo"
    assert ks.command_for("Tab", context=ASSEMBLY) == "hide_component"
    assert ks.command_for("Alt+Drag", context=ASSEMBLY) == "smart_mate"
    assert ks.command_for("RMB+Up") == "view_top"
    assert ks.command_for("RMB+Up", context=PATH_SKETCH) == "smart_dimension"
    assert ks.command_for("RMB+Right", context=DRAWING) == "projected_view"


def test_inventor_contexts():
    ks = inventor()
    assert ks.command_for("R", context=PATH_SKETCH) == "sketch_rectangle" and ks.command_for("R") == "revolve"
    assert ks.command_for("E", context=SKETCH_2D) == "finish_sketch" and ks.command_for("E") == "extrude"
    assert ks.command_for("C", context=ASSEMBLY) == "assembly_constraint" and ks.command_for("C") == "chamfer"
    assert ks.command_for("RMB+UpLeft", context=SKETCH_2D) == "sketch_trim"


def test_nx_and_creo():
    assert nx().command_for("C", context=PATH_SKETCH) == "sketch_circle"
    assert nx().command_for("Ctrl+Shift+MB2") == "radial_menu_2"
    assert creo().command_for("S>L", context=PATH_SKETCH) == "sketch_line"
    assert creo().command_for("Ctrl+R") == "repaint" and creo().command_for("Ctrl+F") == "find"
    assert creo().command_for("MMB+Click", context=SKETCH_2D) == "confirm_command"


def test_conflicts_in_loading():
    ks, report = load_pairs("t", [(GLOBAL, "Ctrl+Z", "undo"), (GLOBAL, "ctrl+z", "redo")])
    assert (GLOBAL, "Ctrl+Z") not in ks.bindings and any("конфликт" in r for r in report)
    ks, report = load_pairs("t", [(GLOBAL, "Ctrl+8", "x"), (GLOBAL, "ctrl+8", "x")])
    assert report == []


# ---------- навигация отдельно от набора (6.4, 6.10) ----------
def test_navigation_is_separate_setting():
    assert solidworks().command_for("MMB+Drag") is None            # в наборе клавиш навигации нет
    assert navigation("SolidWorks").command_for("Ctrl+MMB+Drag") == "pan"
    assert navigation("Inventor").command_for("MMB+Drag") == "pan"
    eff, _ = effective("SolidWorks", "Siemens NX")                  # клавиши SolidWorks, мышь как в NX
    assert eff.command_for("Shift+MMB+Drag") == "pan"
    assert eff.command_for("Ctrl+Z") == "undo"


# ---------- программные привязки (6.4, 6.5, 6.6, 6.14) ----------
def test_program_bindings_fill_free_keys():
    eff, _ = effective("SolidWorks")
    assert eff.command_for("Enter") == "confirm_command" and eff.command_for("Esc") == "cancel_command"
    assert eff.command_for("F3") == "snap_toggle"
    assert eff.command_for("MMB+DoubleClick") == "view_fit"
    assert eff.command_for("Alt+Click") == "select_other"                         # 6.5, в детали
    assert eff.command_for("Alt+Click", context=ASSEMBLY) == "toggle_transparency" # набор в сборке перекрывает


def test_program_bindings_do_not_override_set():
    eff, report = effective("Inventor")
    assert eff.command_for("F3") == "zoom"                  # навигация Inventor занимает F3
    assert eff.command_for("S") == "new_2d_sketch"          # S в Inventor — новый эскиз
    assert eff.command_for("Shift+S") == "shortcut_bar"     # решение 18.10.2026
    assert eff.command_for("Ctrl+F3") == "snap_toggle"      # решение 18.10.2026
    assert not any("snap_toggle" in r or "shortcut_bar" in r for r in report)


def test_point_input_overrides_context():
    eff, _ = effective("SolidWorks")
    assert eff.command_for("Tab", context=ASSEMBLY) == "hide_component"
    assert eff.command_for("Tab", context=ASSEMBLY, point_input=True) == "snap_cycle"
    assert eff.command_for("Alt+O", context=PATH_SKETCH, point_input=True) == "snap_port"
    assert eff.command_for("Alt+O", context=PATH_SKETCH) is None


def test_default_gestures_for_sets_without_wheel():
    eff, _ = effective("Siemens NX")
    assert eff.command_for("RMB+Up") == "view_top"
    assert eff.command_for("RMB+Down", context=PATH_SKETCH) == "sketch_line"


def test_text_field_not_intercepted():
    eff, _ = effective("SolidWorks")
    assert eff.command_for("S", in_text_field=True) is None
    assert eff.command_for("Space", in_text_field=True) is None
    assert eff.command_for("Ctrl+Z", in_text_field=True) == "undo"
    assert eff.command_for("Esc", in_text_field=True) == "cancel_command"


# ---------- настройка ----------
def test_customize_warns_and_keeps_base():
    base = solidworks()
    mine, warnings = customize(base, {(GLOBAL, "Ctrl+Z"): None, (GLOBAL, "Ctrl+Y"): "repeat_last_command"})
    assert base.command_for("Ctrl+Z") == "undo"
    assert mine.command_for("Ctrl+Z") is None and mine.command_for("ctrl+y") == "repeat_last_command"
    assert any("Ctrl+Y" in w for w in warnings)
    assert reset("SolidWorks").command_for("Ctrl+Y") == "redo"


def test_table_and_keys_for():
    ks = solidworks()
    assert (GLOBAL, "Ctrl+Z") in ks.keys_for("undo")
    rows = ks.table()
    assert ("undo", "Ctrl+Z (Я)", GLOBAL) in rows


def test_inventor_and_nx_free_shortcut_bar_and_snap_toggle_18_10():
    import keymap as k
    for name in ("Inventor", "Siemens NX"):
        ks, report = k.effective(name)
        assert not report, (name, report)
    ks, report = k.effective("Inventor")
    assert ks.command_for("Ctrl+F3", k.GLOBAL) == "snap_toggle"
    assert ks.command_for("F3", k.GLOBAL) == "zoom"
    assert ks.command_for("Shift+S", k.GLOBAL) == "shortcut_bar"


def test_catia_set_and_navigation_18_10():
    import keymap as k
    ks, report = k.effective("CATIA")
    assert not report, report                                       # конфликтов нет
    assert ks.command_for("Ctrl+F3", k.GLOBAL) == "snap_toggle"
    assert ks.command_for("F3", k.GLOBAL) == "toggle_tree"
    assert ks.command_for("Alt+Click", k.ASSEMBLY) == "hide_temporarily"
    assert ks.command_for("DoubleClick", k.GLOBAL) == "edit_path_sketch"   # решение 18.10.2026
