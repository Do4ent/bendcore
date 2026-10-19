import commands as c
import ribbon as r


def test_every_ribbon_menu_and_panel_command_in_registry():
    for cmd in r.all_commands():
        assert c.canonical(cmd) in c.TITLES, cmd


def test_tabs_by_document_type_6_1():
    assert r.visible_tabs(r.DETAIL, in_sketch=False) == ["Труба", "Анализ", "Вывод"]
    assert r.visible_tabs(r.DETAIL, in_sketch=True) == ["Труба", "Эскиз", "Анализ", "Вывод"]
    assert r.visible_tabs(r.ASSEMBLY, in_sketch=False) == ["Сборка", "Труба", "Анализ", "Вывод"]
    assert r.visible_tabs(r.DRAWING, in_sketch=True) == ["Чертёж", "Аннотации", "Эскиз"]


def test_tube_tab_groups_6_14():
    groups = {g.name: g.items for g in r.TUBE_TAB.groups}
    assert groups["Заготовка"] == ["trim_allowance", "feed_from_end", "minimize_rotation", "springback_compensation"]
    assert "path_source" in groups["Правка"]


def test_path_sketch_has_line_and_spiral_only_for_path():
    build = next(g for g in r.PATH_SKETCH_TAB.groups if g.name == "Построение")
    assert build.items == ["sketch_line", "spiral"]       # остальные инструменты — вспомогательная геометрия (1.3)


def test_menu_shows_only_applicable_items():
    m = r.menu(r.DETAIL)
    assert "component" not in m["Вставка"] and "forbidden_zone" not in m["Вставка"]
    assert "component" in r.menu(r.ASSEMBLY)["Вставка"]
    assert "drawing_view" in r.menu(r.DRAWING)["Вставка"]


def test_export_3d_visible_but_disabled():
    assert "export_3d" in r.OUTPUT_TAB.groups[0].items
    assert "export_3d" in c.DISABLED
    assert "Будет в следующей версии" in c.tooltip("export_3d")


def test_update_component_kept_from_5_9():
    names = [x for g in r.ASSEMBLY_TAB.groups for it in g.items for x in r.commands_of(it)]
    assert "update_component" in names and "refresh_import" in names
