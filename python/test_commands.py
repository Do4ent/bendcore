import keymap as k
import commands as c


def test_every_keyset_command_resolves():
    for name in k.KEYSET_PAIRS:
        eff, _ = k.effective(name)
        for cmd in set(eff.bindings.values()):
            assert c.canonical(cmd) in c.TITLES, (name, cmd)
    for style in k.NAVIGATION_PAIRS:
        for cmd in set(k.navigation(style).bindings.values()):
            assert c.canonical(cmd) in c.TITLES


def test_aliases_point_to_real_commands():
    for alias, target in c.ALIASES.items():
        assert target in c.TITLES, (alias, target)


def test_same_action_across_sets_is_same_command():
    assert c.canonical(k.solidworks().command_for("F")) == c.canonical(k.nx().command_for("Home")) == \
        c.canonical(k.inventor().command_for("Home")) == "view_fit"
    assert c.canonical(k.solidworks().command_for("Ctrl+B")) == c.canonical(k.nx().command_for("Ctrl+B")) == \
        c.canonical(k.creo().command_for("Ctrl+A")) == "rebuild"


def test_unknown_command_rejected():
    try:
        c.canonical("no_such_command")
    except ValueError:
        return
    raise AssertionError


def test_enter_and_esc_by_editor_state():
    assert c.enter_action(active_command=True) == "confirm_command"
    assert c.enter_action(active_command=False) == "repeat_last_command"      # 1.4
    assert c.esc_action(active_command=True, in_sketch=True) == "cancel_command"
    assert c.esc_action(active_command=False, in_sketch=True) == "exit_sketch"  # 1.2: повторно — из эскиза
    assert c.esc_action(active_command=False, in_sketch=False) is None


def test_tooltip_shows_key_of_current_set():
    eff, _ = k.effective("SolidWorks")
    assert c.tooltip("undo", eff) == "Отменить (Ctrl+Z (Я))"
    eff_nx, _ = k.effective("Siemens NX")
    assert c.tooltip("fit_to_screen", eff_nx).startswith("Вписать в окно (Home")


def test_esc_confirms_only_when_sketch_has_changes_18_10():
    assert c.esc_action(active_command=False, in_sketch=True, has_changes=False) == "exit_sketch"
    assert c.esc_action(active_command=False, in_sketch=True, has_changes=True) == "confirm_exit_sketch"
    assert c.esc_action(active_command=True, in_sketch=True, has_changes=True) == "cancel_command"
