# Документ детали (`bendcore_modeling_document`)

Фаза 1, пп. 1, 5, 6 `docs/ROADMAP_GENERAL_CAD.md`. Заголовки
`include/bendcore/modeling/part_document.hpp`, `include/bendcore/modeling/basic_features.hpp`,
исходники `src/modeling/document/`, пространство имён `bendcore::modeling`, сборка —
`cmake/modeling_document.cmake` (подключается при `BENDCORE_WITH_OCCT=ON`, зависит от
`bendcore_kernel` и `bendcore_formula`). Топологическое именование —
`docs/modeling/TOPOLOGICAL_NAMING.md`.

## Назначение

Упорядоченное дерево операций детали. Каждая операция: параметры (выражения), ссылки на
топологию (имена из карты элементов), опции; результат — `NamedShape` (форма OCCT + карта
элементов), состояние и сообщения. Перестроение начинается с первой изменившейся операции;
ошибка одной операции не останавливает дерево.

## Интерфейс операции

```cpp
class Feature {
    virtual std::string type() const = 0;
    virtual std::vector<ParameterSpec> parameterSpecs() const = 0;  // имя + выражение по умолчанию
    virtual std::vector<ReferenceSlot> referenceSlots() const = 0;  // имя, вид элемента, одна/несколько
    virtual std::vector<OptionSpec> optionSpecs() const = 0;        // допустимые значения, первое — по умолчанию
    virtual Result<FeatureOutput> execute(const FeatureContext&) const = 0;
};
struct FeatureOutput { TopoDS_Shape shape; opencascade::handle<BRepTools_History> history;
                       std::vector<ElementRole> roles; std::vector<std::string> warnings; };
```

`FeatureContext` даёт входную форму (последняя валидная форма до операции), вычисленные
параметры (мм, рад), разрешённые ссылки по слотам (расщеплённая ссылка — все части) и опции.
`history` связывает элементы входной формы и `roles` (элементы, созданные операцией и
названные ею) с результатом. Операции регистрируются в `FeatureRegistry` (тип → фабрика);
реестр нужен и для загрузки файла.

## Работа с документом

```cpp
PartDocument doc(basic::basicRegistry());
doc.setVariable("H", "20");
doc.setVariable("R", "H / 10");
auto box = doc.addFeature({"Box", "", "", {{"length", "40"}, {"width", "30"}, {"height", "H"}}, {}, {}, false});
doc.rebuild();
NamedShape s = doc.finalShape();
TopoReference edge = makeReference(s, topFrontEdge).value();          // выбор во вьюпорте
doc.addFeature({"Fillet", "", "", {{"radius", "R"}}, {{"edges", {edge}}}, {}, false});
doc.rebuild();
doc.setVariable("H", "35");
RebuildReport rep = doc.rebuild();   // rep.recomputed / reused / fromCache / failed
FeatureInfo fi = doc.feature("Fillet001").value();  // state, messages, referenceReports, output
```

Редактирование: `addFeature` (по умолчанию — в позицию маркера отката, если он есть, иначе
в конец; идентификатор `<Type><NNN>`, если не задан), `removeFeature`, `moveFeature`,
`setSuppressed`, `setParameter`, `setReferences`, `setOption`, `setRollback`. Изменения
вступают в силу при `rebuild()`. Выражения проверяются синтаксически сразу, значения — при
перестроении.

### Состояния

| `FeatureState` | Смысл | Выходная форма |
|---|---|---|
| `Ok` | Построена; ссылки разрешены точно или по истории (сообщение описывает историю) | своя |
| `Warning` | Построена с замечанием: ссылка разрешена по сигнатуре, вырез не снял материал | своя |
| `Error` | Не построена: ошибка выражения, сломанная ссылка, отказ OCCT, невалидный результат (`BRepCheck_Analyzer`), нет тела с положительным объёмом | нет; следующие получают последнюю валидную форму |
| `Suppressed` | Подавлена пользователем | нет; форма проходит дальше |
| `RolledBack` | За маркером отката | нет; не вычисляется |

### Перестроение

Для каждой активной операции вычисляется хэш входов (FNV-1a): хэш предыдущей операции,
идентификатор, тип, подавление, значения параметров (биты `double`), ошибки выражений,
опции, имена и виды ссылок. Совпал с хэшем сохранённого результата — результат
переиспользуется (`reused`), иначе операция выполняется (`recomputed`). Так изменение
переменной пересчитывает только операции, параметры которых от неё зависят, и всё ниже по
дереву; перенос, вставка, удаление, подавление меняют цепочку хэшей начиная с места правки.
После разрешения ссылок документ обновляет их сигнатуры (и имя — при неточном разрешении
с единственным результатом) и хэш пересчитывается с обновлёнными данными, поэтому
следующее перестроение без правок ничего не пересчитывает.

### Выражения

Переменные документа и параметры операций — выражения `bendcore::formula`
(`src/formula.cpp`, проверяется `test_formula_comprehensive`): `+ - * / ^`, скобки, `sqrt`,
`abs`, `sin`, `cos`, `tan`, `min`, `max`, `floor`, `ceil`, `round`; имена переменных — с `$`
или без. Переменные вычисляются в порядке зависимостей; цикл помечает все переменные цикла
ошибкой «cyclic dependency A -> B -> A», неизвестное имя — «unknown variable 'Q'».

Почему не `src/parameters` / `src/rebuild`: `ParameterStore` (`src/parameters/parameter_store.cpp`)
хранит строку `formula`, но не вычисляет её, а `get()` для отсутствующего имени молча
возвращает `0.0`; тест `test_parameter_store` в списке `BENDCORE_UNVERIFIED_TARGETS`, тест
`test_rebuild_engine` закомментирован, `src/rebuild/*.cpp` не входят в сборку. Граф
перестроения детали линеен (каждая операция берёт предыдущую форму), его зависимости
полностью задаются цепочкой хэшей выше.

## Операции (`basic_features.hpp`)

Рабочие операции на OCCT с полной историей; при интеграции их заменят операции из
`src/modeling/features/` (функции, возвращающие форму и `BRepTools_History`) — достаточно
обернуть их в `Feature` и сохранить имена ролей.

| Тип | Параметры | Ссылки / опции | OCCT | Имена |
|---|---|---|---|---|
| `Box` | `length, width, height, x, y, z` | — | `BRepPrimAPI_MakeBox`; при непустом входе `BRepAlgoAPI_Fuse` с историей | роли `Top, Bottom, Front (YMin), Back (YMax), Left (XMin), Right (XMax)` |
| `Cylinder` | `radius, height, x, y, z` (ось Z) | — | `BRepPrimAPI_MakeCylinder`, при входе — Fuse | `Top, Bottom, Side` |
| `PrismCut` | `centerX/Y/Z, sizeU, sizeV, radius, depth` | `face` (одна плоская грань); `profile = rectangle\|circle`, `extent = blind\|through` | профиль на плоскости грани (точка центра проецируется на плоскость, u — проекция глобальной X или Y, v = n × u), `BRepPrimAPI_MakePrism` против наружной нормали, `BRepAlgoAPI_Cut` с историей | `Side±U/±V` или `Side`, `Start`, `Floor` |
| `Fillet` | `radius` | `edges` (несколько) | `BRepFilletAPI_MakeFillet`, история `BRepTools_History(args, maker)` | грани `G[<id>](<ребро>)` |
| `Chamfer` | `distance` | `edges` | `BRepFilletAPI_MakeChamfer` (равносторонняя) | то же |

Ошибки операций — с причиной: неположительный размер, неплоская грань эскиза, ссылка
разрешилась в несколько граней, отказ булевой операции (текст `DumpErrors`), отказ
скругления/фаски («size too large for the adjacent faces?»).

## Формат файла

JSON, `"format": "bendcore-part"`, `"version": 2` (`PartDocument::kFormatVersion`):

```json
{"format": "bendcore-part", "version": 2,
 "variables": [{"name": "H", "expression": "20"}],
 "rollback": null,
 "features": [{"id": "Fillet001", "type": "Fillet", "label": "Fillet001", "suppressed": false,
   "parameters": {"radius": "R"}, "options": {},
   "references": {"edges": [{"name": "E(Box001:Front|Box001:Top)", "kind": "edge",
     "signature": {"geometry": "Line", "center": [20, 0, 20], "direction": [1, 0, 0], "size": 40}}]}}],
 "cache": {"Fillet001": {"hash": "…", "state": "ok", "brep": "<base64 BinTools>",
   "elements": {"face": [...], "edge": [...], "vertex": [...]}, "deletions": {...}, ...}}}
```

- Результаты при загрузке пересчитываются (`fromJson` вызывает `rebuild()`).
- Кэш (`SaveOptions::includeCache`) — BRep (`kernel::writeBrepBinary`, base64), карта
  элементов, журнал удалений, состояние, сообщения и отчёты ссылок каждой построенной
  операции с хэшем входов. При загрузке результат берётся из кэша, только если хэш входов
  совпал (`RebuildReport::fromCache`); повреждённая запись кэша игнорируется и операция
  пересчитывается. Индексы карты сохраняются, т. к. порядок `TopExp::MapShapes` не меняется
  при круговом проходе BRep (`docs/kernel/KERNEL_CORE.md`).
- **Миграция v1 → v2**: в v1 переменные — числа в `"parameters"`, операции — в
  `"operations"` с `"params"` (числа или строки) и `"refs"` (только имена). При загрузке
  числа превращаются в выражения, вид элемента ссылки берётся из слота операции, сигнатура
  отсутствует (`valid = false`) и появляется после первого разрешения. Сохранение — всегда v2.
- Ошибки: не JSON, чужой формат, нет обязательных полей, неверный вид элемента →
  `ParseError`; версия новее поддерживаемой, неизвестный тип операции → `Unsupported`.

## Ограничения

- Дерево линейное: операция видит только последнюю валидную форму (одно тело или компаунд);
  многотельные детали и ссылки на формы произвольных операций — фаза 2.
- Эскизы ещё не подключены: профиль выреза задаётся параметрами (прямоугольник/круг).
  Подключение `src/modeling/sketch/` и операций `src/modeling/features/` — следующая задача.
- Ссылка, разрешившаяся в несколько элементов (расщепление), в слоте «одна ссылка» — ошибка
  с числом элементов.
- Любая сломанная ссылка операции — состояние `Error`; частичное применение (скругление
  оставшихся рёбер) не выполняется.
- Отмена/повтор не встроены: документ — значение, отмену можно строить снимками JSON или
  поверх `src/document` транзакций при интеграции.

## Как тестировать

```bash
cmake -S . -B build-occt -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBENDCORE_WITH_OCCT=ON \
  -DBENDCORE_OCCT_DIR=/home/user/bendcore/third_party/occt-install \
  -DBENDCORE_EIGEN_DIR=/home/user/bendcore/third_party/eigen-5.0.0
cmake --build build-occt --target test_modeling_document_naming test_modeling_document_tree \
  test_modeling_document_io test_modeling_document_survival
ctest --test-dir build-occt -R modeling_document --output-on-failure
```

Эталоны — аналитические формулы: потеря объёма скругления прямого ребра 90°
`(1 − π/4)·r²·L`, фаски `d²/2·L`, скругления кромки отверстия по Паппу
`(1 − π/4)·r²·2π(ρ + r(10 − 3π)/(3(4 − π)))`; положение осей скруглений, плоскостей дна и
фасок проверяется прямым поиском граней по геометрии (не через карту элементов).

- `test_modeling_document_tree` — коробка + скругление верхнего ребра по ссылке, высота 20 →
  35 → 12 → 50: ось скругления на `y = r, z = H − r`, объём по формуле; инкрементальность
  (изменение R не пересчитывает коробку); вставка выреза выше по дереву — ссылка точная;
  паз через ребро — ссылка по истории на 2 части, объём `−(1 − π/4)r²(L − a)`; фаска,
  вставленная выше, удаляет ребро — скругление `Error` с причиной «removed by feature
  'Chamfer001'», следующий вырез строится от фаски; перенос операций (и перенос скругления в
  начало — ошибка «there is no shape before this feature»); подавление; маркер отката и
  вставка в него; слишком большой радиус — ошибка без поломки дерева; переменные, цикл,
  неизвестное имя; замена базовой коробки — разрешение по сигнатуре с `Warning`, затем точно.
- `test_modeling_document_io` — сохранение → загрузка: те же состояния, ссылки, объёмы, имена
  всех элементов; кэш (все 4 операции из кэша; изменённая переменная — все пересчитаны;
  испорченный BRep — пересчитана только эта операция); миграция v1; ошибки формата.
- `test_modeling_document_survival` — критерий фазы 1: модель с 6 ссылками (вырез на верхней
  грани, отверстие в передней, скругление верхнего ребра, кромки выреза, кромки отверстия,
  фаска), 25 сценариев изменения размеров + исходное состояние + возврат к нему. Каждая
  ссылка засчитывается, если операция `Ok` и её грань стоит на ожидаемом месте; объём — по
  формуле. Тест печатает долю выживших ссылок:
  `[naming survival] scenarios: 27, references checked: 162, survived: 162, rate: 100.0 %`
  (порог плана — 95 %).
