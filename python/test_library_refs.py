from library import Store, TOOL_SCHEMA, PRESET_SCHEMA, apply, preview


def _store_with(lib, key, rec, layer="предприятия"):
    st = Store()
    st.layer(layer, lib)[key] = rec
    return st


def test_tool_chain_reference_10_4():
    st = _store_with("позиции оборудования", ("T-01",), {"позиция": "T-01"})
    st.layer("предприятия", "строки профилей")[("ТГ-22", 22.0)] = {"профиль": "ТГ-22", "od_mm": 22.0}
    ok_row = {"позиция": "T-01", "профиль": "ТГ-22", "od_mm": "22"}
    bad_od = {"позиция": "T-01", "профиль": "ТГ-22", "od_mm": "28"}
    bad_pos = {"позиция": "T-99", "профиль": "ТГ-22", "od_mm": "22"}
    pv = preview([ok_row, bad_od, bad_pos], TOOL_SCHEMA, st, "предприятия")
    rows_with_errors = {p.row for p in pv.errors}
    assert 2 not in rows_with_errors                       # корректная строка
    assert 3 in rows_with_errors and 4 in rows_with_errors  # ∅ без строки профиля, позиция без записи


def test_preset_optional_empty_reference_ok_10_4():
    st = Store()
    st.layer("предприятия", "трубогибы")[("ТГ-22",)] = {"имя": "ТГ-22"}
    st.layer("предприятия", "номенклатура")[("CU-22",)] = {"код": "CU-22"}
    row = {"имя": "Медь 22", "назначение": "CU-22", "трубогиб": "ТГ-22", "инструмент навивки": "",
           "изоляция": "", "система": ""}
    pv = preview([row], PRESET_SCHEMA, st, "предприятия")
    assert not pv.errors, [p.message for p in pv.errors]
    row2 = dict(row, изоляция="ИЗ-9")                       # задана, но записи нет
    pv2 = preview([row2], PRESET_SCHEMA, st, "предприятия")
    assert any("изоляция" in p.message for p in pv2.errors)
