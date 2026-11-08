# Чтение DXF — `bendcore_dxf`

Заголовок: `include/bendcore/dxf_reader.hpp`, реализация: `src/dxf_reader.cpp`.
Библиотека `bendcore_dxf` не зависит от OCCT и собирается во всех нативных конфигурациях;
её использует импорт (`ImportWorkflow::import_dxf_file`, `bendcore_import`).
Заменяет прежний `DXFReader`, который вместо разбора возвращал фиксированные координаты
(см. `docs/STUB_REMOVAL.md`, раздел 6).

## API

```cpp
#include <bendcore/dxf_reader.hpp>
using namespace bendcore::dxf;

auto doc = readDxfFile("part.dxf");            // Result<Document>
if (!doc) { /* doc.error().code: IoError, ParseError, Unsupported */ }
auto mm = scaleToMillimeters(doc.value());     // координаты в мм по $INSUNITS
for (const Polyline& p : mm.value().polylines)
    for (std::size_t i = 0; i + 1 < p.vertices.size(); ++i)
        if (p.vertices[i].bulge != 0)
            auto arc = bulgeToArc(p.vertices[i].point, p.vertices[i + 1].point, p.vertices[i].bulge);
```

`Document` содержит `version` ($ACADVER), `codepage` ($DWGCODEPAGE), `insunits`
($INSUNITS, 0 — переменной нет), `layers` (имя, цвет ACI, тип линии, заморожен/заблокирован),
`lines`, `circles`, `arcs`, `polylines`, `texts`, а также `skipped` (тип → число
непрочитанных примитивов) и `warnings` (данные прочитаны с потерей).
У каждого примитива — `EntityProps`: слой, цвет ACI (256 — по слою), тип линии,
признак пространства листа (67), дескриптор (5).

## Что читается

| Объект | Группы | Примечание |
|---|---|---|
| HEADER | `$ACADVER` (1), `$DWGCODEPAGE` (3), `$INSUNITS` (70) | |
| TABLES / LAYER | 2, 62, 6, 70 | отрицательный цвет — слой выключен |
| LINE | 10/20, 11/21 | мировая СК |
| CIRCLE, ARC | 10/20, 40, 50/51 | углы приводятся к [0°, 360°) |
| LWPOLYLINE | 90, 70 (бит 1 — замкнута), 10/20, 42 | выпуклость — у вершины, с которой начинается сегмент |
| POLYLINE / VERTEX / SEQEND | 70 полилинии и вершины, 10/20, 42 | 2D и 3D (Z отбрасывается); у сглаженной сплайном (бит 4) управляющие точки (флаг вершины 16) пропускаются; сети (биты 16, 64) — в `skipped` |
| TEXT | 1, 7, 10/20, 11/21, 40, 41, 50, 51, 72, 73 | |
| MTEXT | 1 и 3 (части), 7, 10/20, 11/21, 40, 50, 71 | направление 11/21 имеет приоритет над углом 50 (градусы, как в ezdxf) |

Разделы CLASSES, BLOCKS, OBJECTS не читаются; примитивы блоков попадают в чертёж только
через INSERT, а INSERT учитывается в `skipped`. Прочие типы (SPLINE, ELLIPSE, HATCH,
DIMENSION, …) — тоже в `skipped`.

Выпуклость: `bulge = tg(θ/4)`, θ — центральный угол, `+` — против часовой стрелки.
`bulgeToArc`: хорда c, центр на левой нормали к хорде на расстоянии `(c/2)(1−b²)/(2b)`
от её середины, радиус `(c/2)(1+b²)/(2|b|)`.

Направление выдавливания (210/220/230) для CIRCLE, ARC, LWPOLYLINE, 2D POLYLINE: `(0,0,1)`
— без преобразования; `(0,0,−1)` (зеркальные дуги, частые в файлах AutoCAD) — X меняет
знак, углы дуги θ → 180° − θ с перестановкой начала и конца, выпуклость меняет знак;
любое другое направление — примитив не читается, `skipped["<ТИП> (OCS)"]`. TEXT и MTEXT
читаются только при `(0,0,1)`.

Z и отметка (30, 31, 38) отбрасываются; число таких примитивов — в `warnings`.

### Строки

* AC1021 (R2007) и новее — UTF-8;
* более ранние: ASCII; байты ≥ 0x80 — по `$DWGCODEPAGE` (`ANSI_1251`, `ANSI_1252`); при
  другой кодовой странице корректный UTF-8 сохраняется, иначе байты заменяются на U+FFFD с
  предупреждением;
* `\U+XXXX` раскрывается всегда; в TEXT и MTEXT — `%%c` → Ø, `%%d` → °, `%%p` → ±,
  `%%%` → %, `%%nnn` → символ с кодом nnn, `%%u`/`%%o`/`%%k` удаляются;
* MTEXT: `\P`, `\N` → перевод строки, `\~` → U+00A0, `\\ \{ \}` → символ, `{ }` и
  `\L \l \O \o \K \k` удаляются, коды с параметром до `;` (`\f \F \H \W \Q \T \A \C \c \p`)
  удаляются, дробь `\Sa^b;` → `a/b`, `^I` → табуляция.

## Ошибки

| Код | Когда |
|---|---|
| `IoError` | файл не открывается |
| `Unsupported` | двоичный DXF (`AutoCAD Binary DXF`) |
| `ParseError` | нечётное число строк, нечисловой групповой код или значение числовой группы (с номером строки), секция без ENDSEC, нет ни одной секции, VERTEX/SEQEND вне POLYLINE, радиус ≤ 0, полилиния без вершин |
| `InvalidInput` | `scaleToMillimeters` без единиц ($INSUNITS 0 или неизвестный код); `bulgeToArc` с b = 0 или совпадающими концами |

Отсутствие `EOF`, расхождение числа вершин с группой 90, POLYLINE без SEQEND — предупреждения.

## Тесты

* `test_dxf_reader` (ctest `dxf_reader`, все нативные конфигурации) — ручные фикстуры:
  CRLF и коды с пробелами, слои, выпуклости с аналитическими центрами и радиусами
  (полуокружность, четверть в обе стороны, 270°), POLYLINE/VERTEX со сплайном и сетью,
  ANSI_1251, `\U+XXXX`, коды `%%`, UTF-8 в R2018, MTEXT с форматированием, OCS (0,0,−1) и
  наклонная, блоки, Z, единицы, ошибки.
* `test_dxf_reader_roundtrip` (ctest `dxf_reader_roundtrip`, конфигурация с OCCT, где есть
  `bendcore_drawing_v2`): эталонный лист A3 из `writeDxf` (рамка, основная надпись на
  кириллице, все типы примитивов, штриховка) читается с совпадением каждого примитива до
  5·10⁻⁷ мм (точность записи). При `BENDCORE_PYTHON_EZDXF=<python с ezdxf>` — файлы R12
  (ANSI_1251), R2000 (дюймы), R2018, записанные `tests/dxf/ezdxf_fixture.py`, сверяются с
  тем, что читает из них сам ezdxf, и с `ezdxf.math.bulge_to_arc`.

```bash
python3 -m venv /tmp/ezdxf && /tmp/ezdxf/bin/pip install ezdxf
BENDCORE_PYTHON_EZDXF=/tmp/ezdxf/bin/python ctest --test-dir build-occt -R dxf_reader
```
