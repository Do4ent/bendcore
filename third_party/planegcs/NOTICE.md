# PlaneGCS — сторонний код

- Источник: FreeCAD, `src/Mod/Sketcher/App/planegcs`, коммит `8fcd99b` (ветка master, shallow clone).
- Лицензия исходных файлов: LGPL-2.1-or-later (текст: `LICENSE-LGPL-2.1.txt`). Заголовки файлов и авторство сохранены.
- Зависит от Eigen 5.0.0 (MPL-2.0, текст: `COPYING-MPL-2.0.txt`); Eigen не входит в этот каталог, подключается при сборке.

## Изменения относительно FreeCAD

1. Журнал: `Base::Console` / `Base::TimeElapsed` заменены на `planegcs::log` / `planegcs::warning` (`planegcs_compat.h`);
   приёмник журнала по умолчанию отключён, задаётся через `planegcs::setLogSink`.
2. Макрос экспорта `SketcherExport` и заголовок `SketcherGlobal.h` удалены.
3. Boost: граф связности `boost::connected_components` заменён union-find (та же нумерация компонент);
   `boost::math::constants::pi` и `boost/graph/graph_concepts.hpp` удалены.
4. Исходный код остался на C++20 (как в FreeCAD). Eigen 5.0.0 собирается с C++20 под GCC 13.
5. `std::unreachable()` в диспетчере алгоритмов заменён возвратом `SolveStatus::Failed`.
6. Добавлены недостающие стандартные заголовки (`<cmath>`, `<cassert>`).

Алгоритмы, ограничения и числовая часть не менялись.
