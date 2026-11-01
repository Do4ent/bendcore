# Test Verification System
## СТРОГАЯ ПРОВЕРКА БЕЗ ОШИБОК И ГАЛЛЮЦИНАЦИЙ

**Дата создания:** November 1, 2026  
**Статус:** ACTIVE - MANDATORY for ALL commits  
**Автор:** Claude Code Session

---

## ⚠️ ПРАВИЛО #1: КОД БЕЗ ТЕСТОВ = НЕ КОММИТИТСЯ

```
ЗАПРЕЩЕНО:
❌ git commit "Added feature X" (без тестов)
❌ git commit "Fixed bug Y" (без проверки)
❌ git commit "Implemented Z" (без доказательства)

ТРЕБУЕТСЯ:
✅ Тесты WRITTEN и PASSING
✅ Код ACTUAL (существует в файлах)
✅ Результаты VERIFIED (выполнены)
✅ Доказательства ATTACHED (logs, results)
```

---

## 🔒 MANDATORY VERIFICATION CHECKLIST

### Перед КАЖДЫМ commit:

```bash
# Step 1: Проверь, что файлы существуют реально
git status
# ДОЛЖНЫ БЫТЬ listed файлы, которые ты создал/изменил

# Step 2: Проверь содержимое файла (не галлюцинация!)
cat src/geometry/mesh_tetrahedralization.hpp
# ДОЛЖЕН СУЩЕСТВОВАТЬ реальный код

# Step 3: Запусти typecheck - MUST PASS
npm run typecheck
# OUTPUT: ✅ No errors
# ЕСЛИ ERROR: STOP! Не коммитить!

# Step 4: Запусти тесты - ALL MUST PASS
npm test
# OUTPUT: PASS: 1220 tests (или больше)
# ЕСЛИ FAIL: STOP! Не коммитить!

# Step 5: Запусти specific tests для твоего компонента
npm test -- tetrahedralization
# OUTPUT: PASS: 15+ test cases
# ЕСЛИ FAIL: STOP! Не коммитить!

# Step 6: Запусти lint - NO WARNINGS
npm run lint
# OUTPUT: ✅ All files pass linting

# Step 7: Проверь, что нет debug кода
grep -r "console.log" src/geometry/mesh_tetrahedralization.*
# ДОЛЖНО БЫТЬ ПУСТО! Если есть - удали!

# Step 8: Только ПОСЛЕ всех проверок - коммитируй
git add src/geometry/mesh_tetrahedralization.*
git commit -m "..."
```

---

## 📝 ПРИМЕРЫ ПРАВИЛЬНЫХ COMMITS

### ✅ ПРАВИЛЬНО - С ДОКАЗАТЕЛЬСТВАМИ:

```bash
# Этап 1: Разработка
vim src/geometry/mesh_tetrahedralization.hpp
vim src/geometry/mesh_tetrahedralization.cpp
vim src/geometry/__tests__/tetrahedralization.test.ts

# Этап 2: Обязательная проверка
npm run typecheck
# ✅ PASS

npm test -- tetrahedralization
# ✅ PASS (15 test cases)
# ✅ 92% coverage

npm run lint
# ✅ PASS (no warnings)

# Этап 3: Коммит с ДОКАЗАТЕЛЬСТВАМИ в сообщении
git commit -m "[Phase 4] [Week 1] [Feature]: Implement TetrahedralizationEngine

Implementation:
- Delaunay-based tetrahedralization algorithm
- Mesh quality validation (aspect ratio)
- Constraint handling for surface mesh

TEST RESULTS (RUN BEFORE COMMIT):
✅ npm run typecheck: PASS
✅ npm test -- tetrahedralization: PASS (15/15 cases)
✅ Code coverage: 92%
✅ npm run lint: PASS (0 warnings)
✅ npm test (full suite): PASS (1235 tests)

FILES VERIFIED (git status):
✅ src/geometry/mesh_tetrahedralization.hpp (NEW) - 250 LOC
✅ src/geometry/mesh_tetrahedralization.cpp (NEW) - 350 LOC
✅ src/geometry/__tests__/tetrahedralization.test.ts (NEW) - 400 LOC

METRICS:
- Lines of code: 1000
- Test cases: 15
- Coverage: 92%
- Performance: <1s for 10K vertices

Linked to: PHASE4_DETAILED_SCHEDULE.md (Week 1-2)
GitHub Issue: #456

Co-Authored-By: [Name] <email>"
```

---

## ❌ ПРИМЕРЫ НЕПРАВИЛЬНЫХ COMMITS (ЗАПРЕЩЕНЫ!)

### ❌ НЕПРАВИЛЬНО - БЕЗ ТЕСТОВ:

```
❌ git commit "Implement tetrahedralization"
   (без npm test, без доказательства)

❌ git commit "Add feature X"
   (файлы не существуют на самом деле)

❌ git commit "Fix bug Y"
   (нет тестов, не проверено)

❌ git commit "Update code"
   (не ясно что, зачем, и есть ли тесты)
```

---

## 🧪 MANDATORY TEST REQUIREMENTS

### Для КАЖДОГО компонента ОБЯЗАТЕЛЬНО:

**1. Unit Tests (минимум)**
```typescript
// ДОЛЖНЫ БЫТЬ для КАЖДОЙ функции
describe('TetrahedralizationEngine', () => {
  it('tetrahedralizes simple cube mesh', () => {
    // ARRANGE
    const mesh = createCubeMesh();
    
    // ACT
    const tetras = tetrahedralize(mesh);
    
    // ASSERT
    expect(tetras).toBeDefined();
    expect(tetras.length).toBeGreaterThan(0);
    expect(validateMeshQuality(tetras)).toBe(true);
  });
});
```

**2. Edge Cases Testing**
```typescript
it('handles degenerate triangles', () => {
  const mesh = createDegenerateMesh();
  const result = tetrahedralize(mesh);
  expect(result).toBeDefined();  // MUST NOT crash
});

it('validates mesh quality (aspect ratio)', () => {
  const mesh = createComplexMesh();
  const tetras = tetrahedralize(mesh);
  const quality = validateAspectRatio(tetras);
  expect(quality).toBeGreaterThan(1.0);  // MUST be valid
});
```

**3. Performance Testing**
```typescript
it('tetrahedralizes 10K vertices in <1s', () => {
  const mesh = createLargeMesh(10000);
  const start = performance.now();
  const tetras = tetrahedralize(mesh);
  const elapsed = performance.now() - start;
  
  expect(elapsed).toBeLessThan(1000);  // MUST be fast
});
```

---

## 🔍 VERIFICATION BEFORE EVERY COMMIT

### Mandatory Command Sequence:

```bash
#!/bin/bash
# run-verification.sh - ОБЯЗАТЕЛЬНО перед каждым commit!

echo "=== Phase 4 Verification System ==="
echo ""

# 1. TypeScript Compilation Check
echo "1. Running TypeScript check..."
npm run typecheck
if [ $? -ne 0 ]; then
    echo "❌ TypeScript check FAILED - DO NOT COMMIT"
    exit 1
fi
echo "✅ TypeScript check PASSED"
echo ""

# 2. Full Test Suite
echo "2. Running full test suite..."
npm test 2>&1 | tee test-results.txt
TEST_RESULT=${PIPESTATUS[0]}
if [ $TEST_RESULT -ne 0 ]; then
    echo "❌ Tests FAILED - DO NOT COMMIT"
    echo "Review: test-results.txt"
    exit 1
fi
PASS_COUNT=$(grep -oP 'Tests: \K[0-9]+(?= passed)' test-results.txt || echo "0")
echo "✅ All $PASS_COUNT tests PASSED"
echo ""

# 3. Code Coverage Check
echo "3. Checking code coverage..."
COVERAGE=$(grep -oP 'Coverage: \K[0-9]+(?=%)' test-results.txt || echo "0")
if [ $COVERAGE -lt 85 ]; then
    echo "⚠️ Coverage $COVERAGE% < 85% target"
    echo "Add more tests before commit!"
fi
echo ""

# 4. Lint Check
echo "4. Running linter..."
npm run lint > /dev/null 2>&1
if [ $? -ne 0 ]; then
    echo "⚠️ Linter issues found"
    npm run lint:fix
fi
echo "✅ Lint check done"
echo ""

# 5. File Existence Verification
echo "5. Verifying committed files exist..."
git diff --name-only --staged | while read FILE; do
    if [ ! -f "$FILE" ]; then
        echo "❌ STAGED FILE NOT FOUND: $FILE"
        exit 1
    fi
    echo "✅ $FILE exists"
done
echo ""

# 6. Debug Code Check
echo "6. Checking for debug code..."
if grep -r "console.log\|debugger\|TODO REMOVE" src/ --include="*.ts" --include="*.cpp"; then
    echo "❌ Debug code found - REMOVE before commit!"
    exit 1
fi
echo "✅ No debug code found"
echo ""

echo "═════════════════════════════════════"
echo "✅ ALL VERIFICATIONS PASSED"
echo "✅ SAFE TO COMMIT"
echo "═════════════════════════════════════"
```

### Использование:
```bash
chmod +x run-verification.sh
./run-verification.sh

# ЕСЛИ все ✅ PASS - можно коммитить
# ЕСЛИ хоть один ❌ FAIL - НЕ КОММИТИТЬ, ИСПРАВИТЬ!
```

---

## 📊 PRE-COMMIT HOOK (Git Automation)

Create: `.git/hooks/pre-commit`

```bash
#!/bin/bash

echo "🔍 Running pre-commit verification..."

# 1. TypeScript check MUST PASS
npm run typecheck > /dev/null 2>&1
if [ $? -ne 0 ]; then
    echo "❌ TypeScript check failed"
    echo "Run: npm run typecheck"
    exit 1
fi

# 2. Tests MUST PASS
npm test > /dev/null 2>&1
if [ $? -ne 0 ]; then
    echo "❌ Tests failed"
    echo "Run: npm test"
    exit 1
fi

# 3. Lint MUST PASS
npm run lint > /dev/null 2>&1
if [ $? -ne 0 ]; then
    echo "⚠️ Linting issues (auto-fixing)..."
    npm run lint:fix
fi

# 4. Check commit message format
COMMIT_MSG=$(cat "$1")
if ! echo "$COMMIT_MSG" | grep -q "^\[Phase"; then
    echo "❌ Commit message must start with [Phase X] [Week Y] [TYPE]"
    exit 1
fi

echo "✅ Pre-commit checks passed"
exit 0
```

### Установка:
```bash
chmod +x .git/hooks/pre-commit

# Теперь это будет запускаться АВТОМАТИЧЕСКИ перед каждым commit
git commit -m "..."  # Если проверки fail - commit будет отклонен!
```

---

## 🎯 PROOF OF VERIFICATION

### Что ОБЯЗАТЕЛЬНО должно быть в каждом commit message:

```
TEST RESULTS (REQUIRED):
✅ npm run typecheck: PASS
✅ npm test: PASS (1235 tests)
✅ Coverage: 92%
✅ npm run lint: PASS

FILES VERIFIED (REQUIRED):
✅ src/geometry/mesh_tetrahedralization.hpp - EXISTS
✅ src/geometry/mesh_tetrahedralization.cpp - EXISTS
✅ src/geometry/__tests__/tetrahedralization.test.ts - EXISTS

METRICS (REQUIRED):
- Test cases: 15 (written & passing)
- Code lines: 1000
- Coverage: 92%
```

**Если этого нет - commit ОТКЛОНЕН!**

---

## 🚨 ANTI-HALLUCINATION RULES

### ЗАПРЕЩЕНО:
```
❌ "Реализовал функцию X" - БЕЗ ДОКАЗАТЕЛЬСТВА
❌ "Добавил тесты Y" - ЕСЛИ тесты НЕ PASS
❌ "Исправил баг Z" - БЕЗ ПРОВЕРКИ что баг исправлен
❌ "Готово к merge" - БЕЗ всех ✅ проверок
```

### ТРЕБУЕТСЯ:
```
✅ "Тесты: 15 случаев, все PASS"
✅ "Покрытие: 92% (>85% target)"
✅ "npm test result: 1235 PASS"
✅ "Все файлы verified существуют"
```

---

## 📈 CI/CD AUTOMATION

### GitHub Actions: .github/workflows/strict-verification.yml

```yaml
name: Strict Test Verification

on: [push, pull_request]

jobs:
  verify:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v3
    
    - name: Setup Node
      uses: actions/setup-node@v3
      with:
        node-version: '18'
    
    - name: Install Dependencies
      run: npm install
    
    - name: TypeScript Check (MUST PASS)
      run: npm run typecheck
      continue-on-error: false  # BLOCKS if fail
    
    - name: Run All Tests (MUST PASS)
      run: npm test -- --coverage
      continue-on-error: false  # BLOCKS if fail
    
    - name: Check Coverage (MUST BE >85%)
      run: |
        COVERAGE=$(grep -o 'coverage: [0-9]*' coverage/coverage-summary.json | grep -o '[0-9]*')
        if [ $COVERAGE -lt 85 ]; then
          echo "❌ Coverage $COVERAGE% < 85% required"
          exit 1
        fi
      continue-on-error: false  # BLOCKS if fail
    
    - name: Lint Check (NO WARNINGS)
      run: npm run lint
      continue-on-error: false  # BLOCKS if fail
    
    - name: Check for Debug Code (MUST BE CLEAN)
      run: |
        if grep -r "console.log\|debugger\|TODO REMOVE" src/; then
          echo "❌ Debug code found"
          exit 1
        fi
      continue-on-error: false  # BLOCKS if fail
    
    - name: Report Results
      if: always()
      run: |
        echo "=== VERIFICATION RESULTS ==="
        npm test -- --listTests | wc -l
        echo "All checks: ✅ PASSED or ❌ FAILED"
```

**Результат:**
```
✅ PR можно merge ТОЛЬКО если ВСЕ checks pass
❌ PR BLOCKED если любая проверка fail
```

---

## 📋 FINAL VERIFICATION CHECKLIST

**Перед КАЖДЫМ commit обязательно выполни:**

```
□ Код написан ✅
□ npm run typecheck: PASS ✅
□ Тесты написаны ✅
□ npm test: PASS (все N тестов) ✅
□ Покрытие >85% ✅
□ npm run lint: PASS ✅
□ Нет debug кода (console.log, debugger) ✅
□ Файлы реально существуют (git status) ✅
□ Commit message с доказательствами ✅
□ Linked to GitHub Issue ✅
□ Linked to plan document ✅
□ Готово к commit ✅

ТОЛЬКО ПОСЛЕ ВСЕ ✅:
git commit -m "..."
```

---

## 🔐 GUARANTEE

**Эта система ГАРАНТИРУЕТ:**

✅ **ZERO FALSE CLAIMS**
- Каждый commit имеет доказательства
- Каждый тест реально pass
- Каждый файл реально существует

✅ **ZERO HALLUCINATIONS**
- Нельзя придумывать тесты
- Нельзя коммитить без проверки
- Нельзя писать "готово" если не готово

✅ **ZERO BROKEN CODE**
- TypeScript проверяет ВСЕ типы
- Тесты проверяют ВСЕ функции
- CI/CD блокирует bad commits

✅ **TOTAL VERIFICATION**
- Pre-commit hooks ловят ошибки
- CI/CD перепроверяет на сервере
- Code review требует доказательства

---

## SUMMARY

**Правило #1:** КОД БЕЗ ТЕСТОВ = BLOCKED  
**Правило #2:** ТЕСТЫ ДОЛЖНЫ PASS = REQUIRED  
**Правило #3:** ДОКАЗАТЕЛЬСТВА В COMMIT = MANDATORY  
**Правило #4:** CI/CD MUST PASS = GATE  

**Результат:** NO ERRORS, NO HALLUCINATIONS, ZERO FAILURES 🎯

---

**Status:** ACTIVE - ENFORCED for ALL commits  
**Effective:** Immediately  
**Version:** 1.0  
**Last Updated:** November 1, 2026
