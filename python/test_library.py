import os
from library import Store, DIAMETER_SCHEMA, apply, preview, load_csv, LAYER_PRIORITY

HERE = os.path.dirname(os.path.abspath(__file__))


def test_tubebender_csv_loads_clean():
    rows = load_csv(os.path.join(HERE, "data", "tubebender_copper.csv"))
    st = Store()
    pv = apply(rows, DIAMETER_SCHEMA, st, "предприятия", mode="заменить", only_ok=False)
    assert not pv.errors, pv.errors
    assert len(pv.added) == 10
    assert st.get("диаметры", (19.05, "медь"))["Ls_mm"] == 105.0


def test_validation_and_dup_key():
    rows = [
        {"od_mm": "6,35", "group": "медь", "R_mm": "15", "Lmin_mm": "30", "N_mm": "25"},
        {"od_mm": "6.35", "group": "медь", "R_mm": "15", "Lmin_mm": "30", "N_mm": "25"},   # дубликат
        {"od_mm": "9", "group": "медь", "R_mm": "-1", "Lmin_mm": "30", "N_mm": "25"},      # R ≤ 0
        {"od_mm": "x", "group": "медь", "R_mm": "10", "Lmin_mm": "30", "N_mm": "25"},      # не число
    ]
    pv = preview(rows, DIAMETER_SCHEMA, Store(), "предприятия")
    msgs = " | ".join(p.message for p in pv.errors)
    assert "дубликат" in msgs and "больше 0" in msgs and "не число" in msgs, msgs
    assert len(pv.added) == 1


def test_only_ok_rows_applied():
    rows = [
        {"od_mm": "6.35", "group": "медь", "R_mm": "15", "Lmin_mm": "30", "N_mm": "25"},
        {"od_mm": "9.53", "group": "медь", "R_mm": "", "Lmin_mm": "35", "N_mm": "30"},      # R пусто
    ]
    st = Store()
    apply(rows, DIAMETER_SCHEMA, st, "предприятия", only_ok=True)
    assert st.get("диаметры", (6.35, "медь")) is not None
    assert st.get("диаметры", (9.53, "медь")) is None


def test_all_or_nothing_when_errors():
    rows = [
        {"od_mm": "6.35", "group": "медь", "R_mm": "15", "Lmin_mm": "30", "N_mm": "25"},
        {"od_mm": "9.53", "group": "медь", "R_mm": "", "Lmin_mm": "35", "N_mm": "30"},
    ]
    st = Store()
    apply(rows, DIAMETER_SCHEMA, st, "предприятия", only_ok=False)
    assert st.get("диаметры", (6.35, "медь")) is None


def test_layer_priority_user_over_enterprise():
    st = Store()
    st.layer("системный", "диаметры")[(6.35, "медь")] = {"od_mm": 6.35, "group": "медь", "R_mm": 15.0,
                                                         "Lmin_mm": 30.0, "N_mm": 25.0}
    st.layer("предприятия", "диаметры")[(6.35, "медь")] = dict(st.layer("системный", "диаметры")[(6.35, "медь")], R_mm=16.0)
    st.layer("пользовательский", "диаметры")[(6.35, "медь")] = dict(st.layer("системный", "диаметры")[(6.35, "медь")], R_mm=17.0)
    assert st.get("диаметры", (6.35, "медь"))["R_mm"] == 17.0
    assert LAYER_PRIORITY["пользовательский"] > LAYER_PRIORITY["предприятия"] > LAYER_PRIORITY["системный"]


def test_add_mode_skips_existing():
    st = Store()
    rows = [{"od_mm": "6.35", "group": "медь", "R_mm": "15", "Lmin_mm": "30", "N_mm": "25"}]
    apply(rows, DIAMETER_SCHEMA, st, "предприятия", mode="добавить")
    pv = preview(rows, DIAMETER_SCHEMA, st, "предприятия", mode="добавить")
    assert not pv.added and any(s.get("reason") == "ключ уже есть" for s in pv.skipped)


def test_rollback_on_exception():
    st = Store()
    st.layer("предприятия", "диаметры")[(1.0, "медь")] = {"x": 1}
    st.begin()
    st.layer("предприятия", "диаметры")[(2.0, "медь")] = {"x": 2}
    st.rollback()
    assert (2.0, "медь") not in st.layer("предприятия", "диаметры")
    assert (1.0, "медь") in st.layer("предприятия", "диаметры")


def test_replace_mode_keeps_unchanged_rows_10_4():
    """«Заменить слой»: корректные строки, совпадающие с текущими записями, не должны теряться."""
    row_a = {"od_mm": "6.35", "group": "медь", "R_mm": "15", "Lmin_mm": "30", "N_mm": "25"}
    row_b = {"od_mm": "9.53", "group": "медь", "R_mm": "18", "Lmin_mm": "35", "N_mm": "30"}
    st = Store()
    apply([row_a, row_b], DIAMETER_SCHEMA, st, "предприятия", mode="заменить")
    # повторный импорт того же файла + одна новая строка: обе старые записи без изменений должны остаться
    row_c = {"od_mm": "12.7", "group": "медь", "R_mm": "20", "Lmin_mm": "40", "N_mm": "35"}
    apply([row_a, row_b, row_c], DIAMETER_SCHEMA, st, "предприятия", mode="заменить")
    assert st.get("диаметры", (6.35, "медь")) is not None
    assert st.get("диаметры", (9.53, "медь")) is not None
    assert st.get("диаметры", (12.7, "медь")) is not None


def test_xlsx_units_and_mapping_10_4(tmp_path=None):
    import tempfile, os as _os
    from openpyxl import Workbook
    from library import load_xlsx, convert_units, map_columns, Store
    d = tempfile.mkdtemp()
    path = _os.path.join(d, "t.xlsx")
    wb = Workbook(); ws = wb.active
    ws.append(["Наружный", "Группа", "Радиус", "Мин", "N", "Ls"])
    ws.append([0.75, "медь", 1.5, 30, 25, None])            # 0,75 дюйма = 19,05 мм
    wb.save(path)
    rows = load_xlsx(path)
    assert rows[0]["Наружный"] == "0.75"
    rows = convert_units(rows, {"Наружный": "дюймы"})
    assert abs(float(rows[0]["Наружный"]) - 19.05) < 1e-9
    mapped = map_columns(rows, {"Наружный": "od_mm", "Группа": "group", "Радиус": "R_mm", "Мин": "Lmin_mm", "N": "N_mm"})
    assert mapped[0]["od_mm"] and "Ls" not in mapped[0]
    st = Store()
    st.remember_mapping("диаметры", {"Наружный": "od_mm"})
    assert st.saved_mapping("диаметры") == {"Наружный": "od_mm"}
