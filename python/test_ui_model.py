import ui_model as U
from ui_model import State, Flag, TreeNode, PropertyField, PropertyPanel, DIFFERENT
import units


def _raises(fn, exc):
    try:
        fn()
    except exc:
        return True
    return False


# ---------- 6.2 ----------
def test_state_icons_match_spec():
    assert U.icon_key(State.NORMAL) is None
    assert U.icon_key(State.WARNING) == "triangle_yellow"
    assert U.icon_key(State.ERROR_GEOM) == U.icon_key(State.ERROR_TECH) == "circle_red"
    assert U.icon_key(State.MARK_CROSS) == "mark_red" and U.icon_key(State.MARK_GAP) == "mark_orange"
    assert U.icon_key(State.MARK_FLAG) == "flag_orange" and U.icon_key(State.NEEDS_RECALC) == "bolt"


def test_node_icons_combine_state_and_flags():
    n = TreeNode("t", "Труба 1", State.WARNING, {Flag.FROZEN})
    assert n.icons() == ["triangle_yellow", "lock"]
    assert TreeNode("b", "Тело", flags={Flag.DO_NOT_RECOGNIZE}).icons() == ["no_recognize"]


def test_folder_counters_instead_of_aggregated_icon():
    folder = TreeNode("f", "Трубы", children=[
        TreeNode("a", "Труба 1", State.ERROR_TECH, kind="tube"),
        TreeNode("b", "Труба 2", State.WARNING, kind="tube"),
        TreeNode("c", "Труба 3", kind="tube")])
    assert folder.icons() == []                       # у папки своего значка нет
    objs, viol = folder.counters()
    assert objs == 3 and viol == {"technological": 1, "warning": 1}


def test_label_modes_and_tooltip():
    n = TreeNode("t", "Труба 1", kind="tube",
                 props={"sortament": "Cu 3/8\" (9,52) × 0,76", "designation": "АБВ.001", "bends": 4})
    assert U.node_label(n) == "Труба 1 [Cu 3/8\" (9,52) × 0,76]"
    assert U.node_label(n, "designation") == "АБВ.001"
    assert "Гибов: 4" in U.node_tooltip(n)
    n.state = State.ERROR_GEOM
    assert "Геометрическая ошибка" in U.node_tooltip(n)


def test_root_order_hides_empty():
    nodes = {k: TreeNode(k, k, children=[TreeNode(k + "1", "x")]) for k in ("tubes", "ports", "import")}
    nodes["parameters"] = TreeNode("parameters", "Параметры")           # пустой — скрыт
    assert [n.key for n in U.root_children(nodes)] == ["import", "ports", "tubes"]


def test_tube_order_and_instance_collapse():
    t = [TreeNode("1", "Б", kind="tube", props={"system": "Хладагент", "created": 2}),
         TreeNode("2", "А", kind="tube", props={"system": "Вода", "created": 1})]
    assert [x.name for x in U.sort_tubes(t)] == ["А", "Б"]
    assert [x.name for x in U.sort_tubes(t, "by_name")] == ["А", "Б"]
    valves = [TreeNode(str(i), "Клапан", instance_of="valve") for i in range(24)] + [TreeNode("x", "Рама")]
    rows = U.collapse_instances(valves)
    assert U.collapsed_label(*rows[0]) == "Клапан ×24" and len(rows) == 2


def test_search_filter_rename_folder():
    root = TreeNode("r", "Сборка", children=[
        TreeNode("t1", "Труба 1", State.WARNING, kind="tube", props={"material": "Cu"}),
        TreeNode("f", "Папка", children=[TreeNode("t2", "Труба 2", kind="tube", props={"from_import": True})])])
    assert [n.key for n in U.search(root, "cu")] == ["t1"]
    assert [c.key for c in U.only_violations(root).children] == ["t1"]
    assert _raises(lambda: U.rename(root, "t2", "Труба 1"), ValueError)       # имя уникально
    U.rename(root, "t2", "Подача")
    assert root.find("t2").props["original_name"] == "Труба 2"
    U.delete_folder(root, "f")
    assert [c.key for c in root.children] == ["t1", "t2"]


def test_table_view():
    rows = [TreeNode("1", "Б", kind="tube", props={"bends": 3, "system": "Вода"}),
            TreeNode("2", "А", kind="tube", props={"bends": 5, "system": "Вода"})]
    tv = U.table_view(rows, ["name", "bends"], sort_by="bends")
    assert [r["name"] for r in tv] == ["Б", "А"]
    assert U.table_view(rows, ["name"], where=lambda r: r["bends"] > 4) == [{"name": "А"}]


# ---------- 6.3 ----------
def test_units_input_6_15():
    assert units.parse_length("1in") == 25.4
    assert abs(units.parse_length("3/8in") - 9.525) < 1e-9
    assert abs(units.parse_length("1 1/2in") - 38.1) < 1e-9
    assert units.parse_length("25,4mm") == 25.4
    assert abs(units.parse_length("12", doc_unit="in") - 304.8) < 1e-9
    assert units.parse_angle("90°30'") == 90.5 and units.parse_angle("90,5") == 90.5
    assert U.parse_length_mm("1in") == 25.4


def test_units_output_6_15():
    assert units.format_length(9.525, "in", inch_fraction=16) == "3/8\""
    assert units.format_length(314.325, "in", inch_fraction=8) == "12 3/8\""
    assert units.format_angle(90.5, "dms") == "90°30′"
    assert units.format_angle(90.0) == "90,0°"
    assert units.format_tube_size(9.52, 0.76, "3/8\"") == "3/8\" (9,52) × 0,76"
    assert units.format_tube_size(9.52, 0.76, "3/8\"", mode="mm_in") == "9,52 (3/8\") × 0,76"
    assert units.format_tube_size(12.0, 1.0, "", mode="in") == "12,00 × 1,00"   # метрическая — всегда мм


def test_multi_selection():
    v = U.common_values([{"S": 100, "A": 0}, {"S": 100, "A": 30}], ["S", "A"])
    assert v["S"] == 100 and v["A"] is DIFFERENT


def test_commit_and_readonly():
    calls = []
    p = PropertyPanel(apply=calls.append)
    p.show([PropertyField("S", "S", 100), PropertyField("N", "N", 5, editable=False, reason_disabled="системный")])
    p.commit({"S": 120})
    assert calls == [{"S": 120}]
    assert _raises(lambda: p.commit({"N": 6}), PermissionError)
    assert _raises(lambda: p.commit({"S": DIFFERENT}), ValueError)
    p.command_active = True
    assert _raises(lambda: p.commit({"S": 130}), PermissionError)       # 6.3: во время команды только чтение


def test_pin_document_groups_search():
    p = PropertyPanel(apply=lambda e: None)
    doc = [PropertyField("units", "Единицы", "мм", group="Документ")]
    p.select(None, [], doc)
    assert p.fields == doc                                              # без выбора — свойства документа
    p.select("bend1", [PropertyField("B", "B", 90, group="Гиб"), PropertyField("R", "R", 40, False, group="Гиб")])
    p.pinned = True
    p.select("bend2", [PropertyField("B", "B", 45)])
    assert p.target == "bend1"                                          # «булавка»
    p.collapsed.add("Гиб")
    assert p.visible() == [] and [f.name for f in p.visible("b")] == ["B"]


def test_reset_override():
    calls = []
    p = PropertyPanel(apply=calls.append)
    p.show([PropertyField("gap", "Норма зазора", 2.0, overridden=True, inherited=1.0)])
    p.reset_override("gap")
    assert calls == [{"gap": 1.0}]


def test_property_sets_cover_spec_types():
    assert {"bend", "segment", "vertex", "spiral", "sketch", "assembly", "document"} <= set(U.PROPERTY_SETS)
    spiral = {n for n, _, _ in U.PROPERTY_SETS["spiral"]}
    assert spiral == {"R", "p", "n", "direction", "L_spiral"}            # без начального угла (1.2.1)


def test_bookmarks_simple_6_2_18_10():
    from ui_model import Bookmarks
    b = Bookmarks()
    b.add("Хладагент, этаж 2", ["T1", "T2", "T1"])
    assert b.keys("Хладагент, этаж 2") == ["T1", "T2"]          # повторы убраны, порядок сохранён
    b.rename("Хладагент, этаж 2", "Этаж 2")
    assert b.names() == ["Этаж 2"]
    b2 = Bookmarks.from_dict(b.to_dict())
    assert b2.keys("Этаж 2") == ["T1", "T2"]
    try:
        b.add("Этаж 2", ["X"])
        raise AssertionError("дубль имени должен отвергаться")
    except ValueError:
        pass
    b.remove("Этаж 2")
    assert b.names() == []
