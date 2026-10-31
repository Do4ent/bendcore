# BendCore: План действий на следующие этапы

**Дата**: 31 октября 2026  
**Статус**: Готов к выполнению после слияния PR  
**Приоритет**: P0 (Immediate) → P1 (Next Sprint) → P2 (Before Release)

---

## ФАЗА 1: СЛИЯНИЕ И ИНТЕГРАЦИЯ (IMMEDIATE) ✓

### Шаг 1.1: Создать Pull Request
- [ ] **Статус**: Ветка `independent-reaudit-fixes` готова на GitHub
- **Действие**: Создать PR через GitHub interface
  - Заголовок: "Independent Re-Audit: Fix P0 CI Defects and Document P1 Gaps"
  - Описание: В файле `/tmp/pr_body.txt` (или используйте описание из docs)
  - Base branch: `main`
  - Compare branch: `independent-reaudit-fixes`
- **URL**: https://github.com/Do4ent/bendcore/pull/new/independent-reaudit-fixes

### Шаг 1.2: Review и слияние PR
- [ ] **Статус**: Ожидание review
- **Действие**: Запросить review у team lead/maintainer
- **Ожидаемые вопросы**:
  - "Почему предыдущий отчёт был неправильным?" → Смотреть INDEPENDENT_REAUDIT_REPORT.md
  - "Что делать с API кодом?" → Смотреть API_MIGRATION_PLAN.md
  - "Готовы ли мы к production?" → НЕТ, смотреть REMAINING_RISKS.md
- **Merge criteria**:
  - ✓ P0 дефекты исправлены и проверены
  - ✓ Все документы созданы с доказательствами
  - ✓ Тесты продолжают проходить (1217 + 130 = 1347 тестов)
  - ✓ Нет регрессии

---

## ФАЗА 2: WINDOWS BUILD VERIFICATION (1-2 недели)

### Шаг 2.1: Triggeр Windows сборки
- [ ] **Статус**: Ожидание GitHub Actions
- **Условие**: После слияния PR в main
- **Действие**: Windows workflow автоматически триггерится на push в main
- **Мониторинг**: https://github.com/Do4ent/bendcore/actions
  - Workflow: "Windows Build & Release"
  - Ищем: `build-windows` job
  - Проверяем: Успех или ошибка

### Шаг 2.2: Анализ результатов Windows сборки
- [ ] **Если УСПЕХ**:
  - Проверить, что артефакты загружены:
    - `windows-builds` artifact содержит .exe и .msi
    - `windows-checksums` artifact содержит checksums.txt
  - Скачать артефакты локально
  - Перейти к шагу 2.3

- [ ] **Если ОШИБКА**:
  - Прочитать лог workflow
  - Определить причину:
    - electron-builder ошибка
    - Missing dependencies
    - Path issue
    - NSIS configuration
  - Исправить в новом PR
  - Вернуться к шагу 2.1

### Шаг 2.3: Тестирование на Windows машине
- [ ] **Требуется**: Windows 10/11 машина (локально или VM)
- **Действие 1**: Тестирование NSIS installer
  ```bash
  # Скачанный файл:
  BendCore-CAD-1.0.0-PRODUCTION-x64.exe
  
  # Запустить:
  ./BendCore-CAD-1.0.0-PRODUCTION-x64.exe
  
  # Проверить:
  - [ ] Installer запускается без ошибок
  - [ ] Выбор директории для установки работает
  - [ ] Installation progress показывает
  - [ ] Desktop shortcut создан
  - [ ] Start Menu entry создан
  ```

- **Действие 2**: Тестирование приложения
  ```bash
  # После установки:
  
  # Запустить из Start Menu или Desktop shortcut
  # Проверить:
  - [ ] Главное окно открывается
  - [ ] 3D viewport рендерится
  - [ ] Меню работает
  - [ ] Можно создать new design
  - [ ] Нет ошибок консоли
  ```

- **Действие 3**: Тестирование uninstall
  ```bash
  # Control Panel → Programs → Uninstall
  # Проверить:
  - [ ] Uninstall запускается
  - [ ] Удаление завершается успешно
  - [ ] Files удалены из Program Files
  - [ ] Shortcuts удалены
  ```

### Шаг 2.4: Документирование результатов
- [ ] **Создать**: Windows Build Test Report
  - Дата тестирования
  - OS (Windows версия)
  - Результаты каждого теста
  - Скриншоты (если есть проблемы)
  - Версия приложения
  - Сигнатура (если applicable)
- **Место**: docs/audit/WINDOWS_BUILD_TEST_REPORT.md

---

## ФАЗА 3: E2E И CAD ACCEPTANCE ТЕСТИРОВАНИЕ (2-3 недели)

### Шаг 3.1: Исправить Playwright конфигурацию
- [ ] **Файл**: `playwright.config.ts`
- **Проблема**: webServer ожидает npm workspaces
- **Решение**:
  ```typescript
  webServer: [
    {
      // Было: command: 'npm run dev --workspace=@bendcore/backend'
      // Стало:
      command: 'cd backend && npm run dev',
      url: 'http://localhost:3001/api/health',
      reuseExistingServer: !process.env.CI,
      timeout: 120_000,
    },
    {
      command: 'npm run dev',
      url: 'http://localhost:3000',
      reuseExistingServer: !process.env.CI,
      timeout: 120_000,
    }
  ]
  ```
- **Тестирование**:
  ```bash
  npm run test:e2e
  # Ожидается: All E2E tests pass or failures identified
  ```
- **PR**: Создать отдельный PR для этого изменения

### Шаг 3.2: Запустить E2E тесты
- [ ] **Статус**: После исправления playwright config
- **Действие**:
  ```bash
  npm run test:e2e
  ```
- **Ожидание**: 4 test suites (parallel-offset, combined-3d, offset-integration, production)
- **Результат**:
  - [ ] Все проходят → Переход к 3.3
  - [ ] Некоторые падают → Debug и fix, затем 3.3

### Шаг 3.3: Создать CAD acceptance тест suite
- [ ] **Файлы для создания**:
  - Sample CAD files (minimal, medium, complex)
  - Acceptance test scenarios
  - Validation checklist

- [ ] **Сценарий 1: Simple tube**
  ```typescript
  // tests/acceptance/simple-tube.test.ts
  // - Create straight tube
  // - Validate geometry
  // - Save/reload cycle
  // - Undo/redo
  ```

- [ ] **Сценарий 2: Multi-bend assembly**
  ```typescript
  // tests/acceptance/multi-bend.test.ts
  // - Create 3+ bend tube
  // - Verify tangency
  // - Calculate dimensions
  // - Export if supported
  ```

- [ ] **Сценарий 3: Complex parametric design**
  ```typescript
  // tests/acceptance/parametric.test.ts
  // - Create design with formulas
  // - Change parameters
  // - Verify feature updates
  // - Constraint propagation
  ```

- **Документирование**: tests/acceptance/README.md с описанием каждого сценария

### Шаг 3.4: Запустить CAD acceptance тесты
- [ ] **Команда**:
  ```bash
  npm run test:acceptance  # (если добавлен скрипт)
  # Или:
  npm run test:e2e -- acceptance/
  ```
- **Ожидание**: Все сценарии проходят или failures выявлены
- **Результат**: docs/audit/CAD_ACCEPTANCE_TEST_RESULTS.md

---

## ФАЗА 4: API CODE RESOLUTION (Параллельно с Фазой 2-3)

### Шаг 4.1: Команда решает статус API кода
- [ ] **Встреча**: Team lead + архитектор
- **Вопросы**:
  - Нужен ли этот код для production?
  - Используется ли он где-то?
  - Какова история (почему он существует)?
- **Возможные решения**:
  1. Мигрировать в backend (если нужен)
  2. Удалить (если устаревший)
  3. Документировать как deprecated (если неясно)

### Шаг 4.2: Выполнить решение
- [ ] **Если мигрировать**:
  - Перемещение: `src/api/` → `backend/src/api/`
  - Обновить imports
  - Добавить тесты
  - Обновить documentation
  - PR в main

- [ ] **Если удалить**:
  - Удалить `src/api/` directory
  - Удалить `tsconfig.api.json`
  - Удалить `npm run typecheck:api` script
  - Обновить documentation
  - PR в main

- [ ] **Если оставить deprecated**:
  - Уже сделано (смотреть API_MIGRATION_PLAN.md)
  - Добавить комментарий в src/api/README.md
  - Создать GitHub issue для tracking

---

## ФАЗА 5: CODE SIGNING И PRODUCTION READINESS (4-8 недель)

### Шаг 5.1: Получить код signing сертификат
- [ ] **Когда**: Перед release в production
- **Поставщик**: DigiCert, Sectigo, или аналог
- **Тип**: EV (Extended Validation) для Windows
- **Стоимость**: ~$300-500/год
- **Время**: 1-3 дня на валидацию

### Шаг 5.2: Настроить signing в CI
- [ ] **Конфиг**: electron-builder + GitHub Actions
- **Файлы**:
  - Сохранить .pfx файл в GitHub Secrets
  - Обновить electron-builder config
  - Обновить Windows workflow
- **Тестирование**: Сборка должна подписать .exe файлы
- **Верификация**: Проверить подпись на Windows машине

### Шаг 5.3: Performance и stress тестирование
- [ ] **Большой дизайн**: 10K+ features
- **Метрики**:
  - Load time
  - Memory usage
  - UI responsiveness
  - Save/reload performance
- **Документирование**: docs/performance/BENCHMARKS.md

---

## ФАЗА 6: FINAL VERIFICATION BEFORE RELEASE (1-2 недели перед release)

### Шаг 6.1: Финальная проверка всех компонентов
- [ ] **Checklist**:
  - ✓ Frontend тесты pass (1217)
  - ✓ Backend тесты pass (130)
  - ✓ TypeScript strict mode pass
  - ✓ E2E тесты pass
  - ✓ CAD acceptance pass
  - ✓ Windows build + signature
  - ✓ Performance acceptable
  - ✓ Documentation complete

### Шаг 6.2: Create release candidate
- [ ] **Действие**:
  - Tag: v1.0.0-rc.1
  - Create GitHub Release
  - Include all artifacts (Windows, Linux, etc)
  - Document release notes
  - Test install on clean machine

### Шаг 6.3: Community testing (опционально)
- [ ] **Если open source**:
  - Release RC для community testing
  - Собрать feedback
  - Fix critical issues
  - Release v1.0.0

---

## МЕТРИКИ УСПЕХА

### Фаза 2: Windows Build
- [ ] Windows artifact generation: **SUCCESS**
- [ ] Application launch: **SUCCESS**
- [ ] No SmartScreen warning: **PASS** (after signing)

### Фаза 3: E2E & CAD
- [ ] E2E test suite: **ALL PASS**
- [ ] CAD acceptance: **ALL PASS**
- [ ] Real file handling: **VERIFIED**

### Фаза 4: API Resolution
- [ ] API code status: **DECIDED**
- [ ] Implementation: **COMPLETE**

### Фаза 5: Production Ready
- [ ] Code signing: **IMPLEMENTED**
- [ ] Performance acceptable: **VERIFIED**
- [ ] Compliance check: **PASS**

### Фаза 6: Release Ready
- [ ] All tests: **PASS**
- [ ] RC tested: **VERIFIED**
- [ ] Documentation: **COMPLETE**

---

## TIMELINE

| Фаза | Задача | Дата старта | Дата окончания | Owner |
|------|--------|------------|---------------|-------|
| 1 | Merge PR | Oct 31 | Nov 2 | Team Lead |
| 2 | Windows Build Test | Nov 2 | Nov 15 | DevOps + QA |
| 3 | E2E & CAD Tests | Nov 2 | Nov 20 | QA + Dev |
| 4 | API Resolution | Nov 2 | Nov 15 | Architecture |
| 5 | Code Signing | Nov 15 | Dec 15 | DevOps |
| 6 | Final Verification | Dec 15 | Dec 22 | QA |
| | **RELEASE** | | **Dec 23** | |

---

## RISK MITIGATION

### Риск 1: Windows build fails
- **Вероятность**: Medium
- **Влияние**: High (delay release)
- **Mitigation**: Early testing (Phase 2), debug logs
- **Contingency**: Fix in separate PR, re-trigger workflow

### Риск 2: E2E tests reveal major issues
- **Вероятность**: Medium
- **Влияние**: High (require fixes)
- **Mitigation**: Fix Playwright config first, run early
- **Contingency**: Create defect fixes, add regression tests

### Риск 3: CAD acceptance fails on real data
- **Вероятность**: Medium-High
- **Влияние**: Critical (geometry correctness)
- **Mitigation**: Start with simple cases, iterate
- **Contingency**: Engage CAD engineer, analyze geometry

### Риск 4: Code signing certificate delays
- **Вероятность**: Low
- **Влияние**: Medium (slight delay)
- **Mitigation**: Request early (Phase 5)
- **Contingency**: Release unsigned RC first, sign later

---

## DECISION POINTS

### DP-1: API Code Status (Phase 4)
- **Опции**: Migrate, Delete, Keep
- **Owner**: Team Lead + Architecture
- **Deadline**: Nov 15
- **Impact**: Affects Phase 4 timeline

### DP-2: Release Scope (Phase 6)
- **Опции**: Include all fixes, or delay for more work
- **Owner**: Product Manager
- **Deadline**: Dec 15
- **Impact**: Release date

---

## SUCCESS CRITERIA

✅ **Project is Production-Ready** when:
1. All P0/P1 fixes merged and verified
2. Windows build tested and working
3. E2E tests all pass
4. CAD acceptance tests all pass
5. Performance acceptable
6. Code signed
7. Documentation complete
8. Team agreement on readiness

---

**Plan Owner**: Claude Code (Autonomous Audit)  
**Next Review**: After Phase 1 (PR merged)  
**Last Updated**: 2026-10-31
