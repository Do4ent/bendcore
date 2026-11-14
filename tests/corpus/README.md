# Корпус STEP- и IGES-моделей bendcore

Корпус для регрессии импорта (docs/ROADMAP_GENERAL_CAD.md, фаза 0, п. 6).
Правило: в корпус попадают только файлы с подтверждённой лицензией на распространение;
источник и лицензия каждого файла указаны ниже.

## Состав

| Каталог | Источник | Лицензия |
|---|---|---|
| `generated/` | Сгенерирован программой `tests/corpus/generate_corpus.cpp` из этого репозитория (OCCT 8.0.1, `STEPCAFControl_Writer`) | MIT, как весь bendcore |
| `external/` | Публичные модели сторонних CAD. **Сейчас отсутствует** — см. «Внешние модели» | — |

### generated/ — синтетические модели с аналитическими эталонами

Ожидаемые значения в `generated/manifest.json` вычислены генератором **по формулам из
размеров**, а не измерением OCCT. Числа граней выведены из топологии построения.

| Файл | Протокол | Модель | Объём V | Граней |
|---|---|---|---|---|
| `box_100x60x20.step` | AP203 | параллелепипед a·b·c | abc | 6 |
| `cylinder_r20_h50.step` | AP214 | цилиндр | πr²h | 3 |
| `tube_R30_r10_h40.step` | AP214 | цилиндр с соосным сквозным отверстием (булева разность) | π(R²−r²)h | 4 |
| `plate_120x80x10_hole_d20.step` | AP214 | пластина с центральным отверстием | abc − πr²c | 7 |
| `torus_R50_r10.step` | AP242 | тор | 2π²Rr² | 1 |
| `sphere_R25.step` | AP214 | шар | 4/3·πR³ | 1 |
| `frustum_R30_R10_h40.step` | AP214 | усечённый конус | πh/3·(R1²+R1R2+R2²) | 3 |
| `plate_100x60x10_corner_fillets_r8.step` | AP214 | пластина, 4 вертикальных ребра скруглены | c(ab − (4−π)r²) | 10 |
| `box_80x60x40_all_fillets_r5.step` | AP214 | все 12 рёбер скруглены (сумма Минковского) | a'b'c' + 2r(a'b'+b'c'+c'a') + πr²(a'+b'+c') + 4/3πr³ | 26 |
| `l_bracket_60x40x8_w30.step` | AP214 | уголок: выдавленный L-профиль | (Lt + (H−t)t)·w | 8 |
| `multibody_two_boxes.step` | AP214 | одна деталь из двух тел (compound) | 1000 + 1000 | 12 |
| `assembly_bracket_pins_ap214.step` | AP214 | сборка XCAF: BASE_PLATE (красная) ×1, PIN (синий) ×2, SPACER (зелёная) ×1 | abc + 2πrp²hp + π(Rs²−rs²)hs | 12+4=16 |
| `assembly_bracket_pins_ap242.step` | AP242 | та же сборка в AP242 | то же | 16 |

IGES-копии части моделей (тот же генератор, `IGESCAFControl_Writer`, единица мм) перечислены
в разделе `iges` манифеста: `file`, `model` (имя модели в `models`, чьи формулы — эталон) и
`mode` (`brep` — тела, тип 186; `faces` — только обрезанные поверхности, тип 144, читатель
сшивает их в тело):

| Файл | Модель | Режим |
|---|---|---|
| `box_100x60x20.igs` | `box_100x60x20` | brep |
| `cylinder_r20_h50.igs` | `cylinder_r20_h50` | brep |
| `l_bracket_60x40x8_w30.igs` | `l_bracket_60x40x8_w30` | brep |
| `plate_120x80x10_hole_d20_faces.igs` | `plate_120x80x10_hole_d20` | faces |
| `assembly_bracket_pins.igs` | `assembly_bracket_pins_ap214` | brep (4 тела под одним корнем) |

Их читают `test_kernel_iges` (native), `tests/wasm/exchange.test.mjs` (WASM) и сквозной тест
`tests/e2e-app/exchange.spec.ts`.

Для каждой модели манифест содержит также площадь поверхности (формула в поле
`formula`), точные габариты, ожидаемое ключевое слово `FILE_SCHEMA`, имя верхней формы,
а для сборок — имена деталей, число вхождений и цвета. Допуск объёма и площади —
относительный 1e-6, у модели со скруглением всех рёбер 1e-4 (угловые сопряжения OCCT
строит приближёнными поверхностями); габариты — абсолютный 1e-4 мм.

Перегенерация (после изменения генератора):

```bash
cmake --build build-occt --target generate_corpus
build-occt/generate_corpus tests/corpus/generated
```

Файлы в `generated/` закоммичены намеренно: тест `corpus_step_committed` читает
неизменные файлы и ловит регрессии читателя и OCCT; тест `corpus_step_fresh` дополнительно
проверяет цикл «запись текущим OCCT → чтение» на свежесгенерированных файлах.

## Внешние модели

Цель фазы 0 — 50–100 файлов разных CAD. Первый кандидат — NIST MBE PMI CAD models
(NIST CTC/FTC/STC, работы правительства США, public domain по 17 U.S.C. §105).

Попытка загрузки 2026-11-06 из рабочего окружения заблокирована сетевым прокси:

```
$ curl -sS -o /dev/null -w "%{http_code}\n" --max-time 20 https://www.nist.gov/document/nist-pmi-step-files
curl: (56) CONNECT tunnel failed, response 403
# состояние прокси: "gateway answered 403 to CONNECT (policy denial or upstream failure)", host www.nist.gov:443

$ curl -sS -o /dev/null -w "%{http_code}\n" --max-time 20 https://s3.amazonaws.com/nist-el/mfg_digitalthread/NIST_MBE_PMI_CTC_STEP.zip
403
```

Файлы не подменялись ничем другим. Чтобы добавить внешние модели:

1. Положить файлы в `tests/corpus/external/`, в этот README — источник (URL), дату
   загрузки, лицензию и SHA-256 каждого файла.
2. Создать `tests/corpus/external/manifest.json` в формате `generated/manifest.json`
   (поля `file`, `name`, `schema_keyword`, `free_shapes`, `top_name`, `solids`, `faces`,
   `volume`, `area`, допуски, габариты; для сборок `components`). Эталонные значения —
   из независимого источника (документация NIST, отчёт исходной CAD), не из OCCT.
3. Тест `corpus_step_external` регистрируется автоматически при наличии манифеста.
