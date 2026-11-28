# STEP-файлы эталонных деталей

26 эталонных деталей фаз 1 и 2 (`docs/modeling/REFERENCE_PARTS.md`) в исходных размерах —
для сверки массы и открытия в сторонней CAD (критерий фазы 2 `docs/ROADMAP_GENERAL_CAD.md`).

- Источник: сгенерированы тестом `tests/modeling/test_modeling_document_reference_parts.cpp`
  из документа детали bendcore (OCCT 8.0.1, `kernel::writeStep`, AP214, миллиметры). Лицензия —
  MIT, как весь bendcore.
- `manifest.json`: для каждой детали — имя, файл, материал, плотность (кг/м³), объём (мм³) и
  масса (кг) **по аналитической формуле** из размеров, число тел, исходные значения переменных.
- Тест `ModelingReferenceParts.CommittedStepFilesMatchFormulas` читает закоммиченные файлы и
  сверяет объём, массу и число тел с формулами (отн. 1e-6).

Перегенерация (после изменения деталей):

```bash
cmake --build build-occt --target test_modeling_document_reference_parts
BENDCORE_WRITE_REFERENCE_STEP=1 build-occt/test_modeling_document_reference_parts
```

Сверка в сторонней CAD (выполняет человек): открыть файл, задать материал с плотностью из
манифеста, сравнить массу с `mass_kg` — допуск плана 0,1 %.
