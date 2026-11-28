# Значки интерфейса

Реестр значков ленты и панелей всех редакторов: один модуль `index.tsx` — «имя → SVG»
(`ICONS`, `Icon`, `iconSvg`, `iconEntry`). Каталог всех значков — страница `web/icons`
(`npm run dev:icons`, порт 3210; на сервере оболочки — `/icons.html`), снимок —
[`docs/ui/img/icon_catalog.png`](../../../docs/ui/img/icon_catalog.png).

| Файл | Что внутри |
|---|---|
| `index.tsx` | реестр: общие действия (`GENERAL_ICONS`: `new`, `open`, `save`, `undo`, `redo`, `export`, `import`, `print`, `measure`, `search`, `settings`, `select`, `fit`, `delete`…) → имя Tabler; значки САПР; `FALLBACK_ICON = 'feature'` для команды без своего значка |
| `tabler.ts` | значки Tabler Icons, **сгенерирован** `scripts/gen_tabler_icons.mjs` из SVG-файлов пакета `@tabler/icons`; руками не править |
| `cad.ts` | значки САПР, нарисованные для bendcore: эскиз и ограничения, операции детали и анализ, сборка, чертёж |
| `__tests__/icons.test.ts` | Jest: версия и лицензия пакета, совпадение `tabler.ts` с пакетом, стиль каждого значка, полнота наборов, нет повторов |

## Стиль

Все значки — в стиле Tabler Icons, чтобы общие и свои значки не различались на ленте:

- сетка 24×24, рисунок в пределах 2…22 (отступ 2 px, как у Tabler);
- только контур: `stroke-width="2"`, `stroke-linecap="round"`, `stroke-linejoin="round"`,
  `fill="none"`, цвет — `currentColor` (значок принимает цвет текста кнопки: обычный, активный,
  затенённый);
- элементы — `path`, `circle`, `ellipse`; без заливок, своих цветов, толщин, `transform` и текста;
- точка — отрезок нулевой длины (`h.01`) или маленькая окружность; штриховая линия — отдельными
  отрезками (без `stroke-dasharray`, который плохо масштабируется);
- значок должен читаться в 24 px и в 16 px: не больше 4–6 деталей, промежутки между линиями
  не меньше 2 единиц сетки. Обозначения ЕСКД передаются их графикой: шероховатость — знак
  ГОСТ 2.309 с полкой, резьба — тонкая 3/4 окружности, база — треугольник с рамкой, разрез —
  штриховка под 45°.

Проверка: Jest (`icons.test.ts`) — элементы и атрибуты, окружности в пределах сетки; Playwright
(`tests/e2e-app/ribbon.spec.ts`) — каждый значок в 16 px рисует что-то размером больше 6 единиц
и не выходит за 24×24; снимок каталога — для проверки глазами.

## Как добавить значок

1. Нарисуйте его в `cad.ts` (функции `p(...)` — пути, `c(...)` — окружности, `e(...)` —
   эллипсы), раздел `section` — его место в каталоге.
2. Откройте каталог (`npm run dev:icons`) и посмотрите значок в 32/24/16 px на светлом и тёмном
   фоне; обновите снимок: `BENDCORE_ICON_CATALOG_SCREENSHOT=$PWD/docs/ui/img/icon_catalog.png
   npm run test:e2e:app -- -g "icon catalog"`.
3. Общий значок Tabler: добавьте имя в `TABLER_NAMES` (`scripts/gen_tabler_icons.mjs`), выполните
   `node scripts/gen_tabler_icons.mjs` и свяжите действие с ним в `GENERAL_ICONS`.

Команда, для которой значка нет (например, операция, которую ядро добавит в каталог позже),
показывается с общим значком операции `feature` и своей подписью (`commandIcon`,
`src/ui/app/ribbonModel.ts`).

## Лицензии

**Tabler Icons** — пакет `@tabler/icons` версии **3.53.0** (точная версия в `package.json`,
devDependency: значки копируются в `tabler.ts` при генерации, в сборку пакет не входит).
Лицензия проверена (`npm view @tabler/icons-react license` и `npm view @tabler/icons license` —
`MIT`) и закреплена тестом по `package-lock.json`:

```
MIT License

Copyright (c) 2020-2026 Paweł Kuna

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

Используются 20 значков Tabler (список — `TABLER_NAMES`). Пакет React-компонентов
`@tabler/icons-react` не подключён: реестру нужна разметка SVG, а не компоненты, и SVG-файлы
пакета `@tabler/icons` дают её без лишнего кода в сборке.

**Значки САПР** (`cad.ts`, 127 значков) нарисованы для bendcore и распространяются по лицензии
MIT проекта (`LICENSE` в корне репозитория).
