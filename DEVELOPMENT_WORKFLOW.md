# BendCore Development Workflow
## Строгая система отслеживания изменений

**Дата создания:** November 1, 2026  
**Версия:** 1.0  
**Статус:** ACTIVE для Phase 4+

---

## 📋 ЦЕЛЬ

Гарантировать, что **ВСЕ** изменения в коде:
1. ✅ Документированы в GitHub Issues/Projects
2. ✅ Заняты в Git commits с правильным форматом
3. ✅ Связаны с соответствующим документом плана
4. ✅ Имеют соответствующие тесты
5. ✅ Проходят code review перед merge

---

## 🔄 WORKFLOW ПРОЦЕСС

### Phase 1: Planning (Planning Board)

**Для каждой недели разработки:**

1. **Создать GitHub Project для недели**
   ```
   Title: Phase 4 - Week 1 (Jan 15-19, 2027)
   Columns:
   - Planned (документы плана)
   - In Progress (текущие работы)
   - In Review (code review)
   - Done (merged & tested)
   ```

2. **Создать Issues для каждого компонента**
   ```markdown
   Title: [Phase 4] Week 1: Implement TetrahedralizationEngine
   
   Body:
   - [ ] Read PHASE4_DETAILED_SCHEDULE.md Week 1-2 section
   - [ ] Create src/geometry/mesh_tetrahedralization.hpp
   - [ ] Create src/geometry/mesh_tetrahedralization.cpp
   - [ ] Create tests: src/geometry/__tests__/tetrahedralization.test.ts
   - [ ] Write 15+ test cases
   - [ ] Code review
   - [ ] Merge to phase-4 branch
   
   Linked Document: PHASE4_DETAILED_SCHEDULE.md (Week 1-2)
   Assignee: [C++ Lead]
   Labels: phase-4, geometry, critical
   ```

3. **Assign Issues to Project**
   - Drag into "Planned" column
   - Set priority & due date

---

### Phase 2: Development (Main Branch)

**Для каждого Issue (компонента):**

#### Step 1: Create Feature Branch
```bash
git checkout -b feature/phase4-tetrahedralization
# OR
git checkout -b phase4/week1-tetrahedralization
```

**Naming Convention:**
```
phase4/week[N]-[component-name]
feature/phase4-[component-name]
fix/phase4-[bug-name]
docs/phase4-[doc-name]
test/phase4-[feature-name]
```

#### Step 2: Work on Component
```
Week 1, Day 1-2:
  └── Read docs, plan, create branch

Week 1, Day 3-5:
  ├── Create files from plan
  ├── Implement algorithm
  ├── Write tests
  ├── Pass type checking (npm run typecheck)
  └── Local testing (npm test)

Daily Commits:
  git add src/geometry/mesh_tetrahedralization.*
  git commit -m "WIP: Implement Delaunay tetrahedralization (Day 1-2 analysis)"
```

#### Step 3: Comprehensive Testing
```bash
# TypeScript
npm run typecheck        # Must pass
npm test -- tetrahedralization  # 15+ cases

# C++
cd backend && npm test && cd ..

# Integration
npm run test:e2e

# Performance
npm run perf:tetrahedralization
```

#### Step 4: Code Cleanup & Documentation
```
Before submitting PR:
  - [ ] Remove console.log / debug code
  - [ ] Add JSDoc/Doxygen comments
  - [ ] Verify imports/exports
  - [ ] Update CHANGELOG.md
  - [ ] Verify no compiler warnings
  - [ ] Run prettier/eslint
```

```bash
npm run lint:fix
npm run format
```

#### Step 5: Final Commit Before PR
```bash
git status  # Verify all changes
git add src/geometry/mesh_tetrahedralization.*
git add src/geometry/__tests__/tetrahedralization.test.ts
git commit -m "$(cat <<'EOF'
[Phase 4] Implement TetrahedralizationEngine for FEA mesh

Implementation:
- Delaunay-based tetrahedralization algorithm
- Mesh quality validation (aspect ratio)
- Constraint handling for surface mesh

Tests:
- 15+ tetrahedralization test cases
- Quality validation tests
- Edge case handling

Files:
- src/geometry/mesh_tetrahedralization.hpp (NEW)
- src/geometry/mesh_tetrahedralization.cpp (NEW)
- src/geometry/__tests__/tetrahedralization.test.ts (NEW)

Metrics:
- Code: 350 LOC
- Tests: 450 LOC
- Coverage: 92%

Linked to: PHASE4_DETAILED_SCHEDULE.md Week 1-2
GitHub Issue: #[issue-number]

Co-Authored-By: [Name] <[email]>
Claude-Session: [session-link]
EOF
)"
```

---

### Phase 3: Review & Merge

#### Step 1: Create Pull Request
```bash
git push -u origin feature/phase4-tetrahedralization

# OR use GitHub CLI
gh pr create \
  --title "[Phase 4] Implement TetrahedralizationEngine" \
  --body "$(cat <<'EOF'
## Summary
Implements Delaunay-based tetrahedralization for FEA mesh generation.

## Test Plan
- ✅ 15+ tetrahedralization test cases pass
- ✅ Code coverage: 92%
- ✅ npm run typecheck: PASS
- ✅ npm test: PASS (all 1220+ tests)

## Linked Document
PHASE4_DETAILED_SCHEDULE.md - Week 1-2

## Acceptance Criteria
- [x] Algorithm implemented
- [x] Tests passing
- [x] Code review approved
- [x] Performance <1s for typical mesh
EOF
)"
```

#### Step 2: Code Review Checklist
```markdown
## Code Review Checklist

Reviewer checks:
- [ ] Code follows C++20 standards (-Wall -Wextra -Werror)
- [ ] Type safety (strict mode, no implicit conversions)
- [ ] Tests cover all code paths (90%+ coverage)
- [ ] No memory leaks (Valgrind clean)
- [ ] Performance targets met (<1s for 10K vertices)
- [ ] Documentation complete (Doxygen)
- [ ] Linked to plan document
- [ ] All commits follow format
- [ ] No debug code / console.log left
- [ ] CHANGELOG.md updated

Reviewer approval:
  / Approve (merge when ready)
  / Request Changes (specify why)
  / Comment (non-blocking feedback)
```

#### Step 3: Address Review Comments
```bash
# Make changes
vim src/geometry/mesh_tetrahedralization.cpp

# New commit (don't amend!)
git add src/geometry/mesh_tetrahedralization.cpp
git commit -m "Review feedback: Optimize aspect ratio calculation (PR #123)"

# Push
git push origin feature/phase4-tetrahedralization
```

#### Step 4: Merge to Phase 4 Branch
```bash
# When approved:
gh pr merge [PR-number] \
  --squash \
  --delete-branch \
  --message "Merge: [Phase 4] Implement TetrahedralizationEngine"

# OR manually
git checkout phase-4
git pull origin phase-4
git merge feature/phase4-tetrahedralization
git push origin phase-4

# Verify on GitHub
# Check: All status checks passed ✅
```

---

## 📊 TRACKING MATRIX

### GitHub Issues Template
```markdown
## Type
- [ ] Feature
- [ ] Bug Fix
- [ ] Documentation
- [ ] Test

## Phase & Week
Phase: 4
Week: 1 (Jan 15-19, 2027)

## Linked Documents
- [x] PHASE4_DETAILED_SCHEDULE.md (Week 1-2)
- [x] IMPLEMENTATION_MASTER_PLAN.md (Section 4.1)

## Acceptance Criteria
- [ ] Code implemented per plan
- [ ] Tests written (minimum coverage specified in plan)
- [ ] Code review approved
- [ ] Merged to phase-4 branch
- [ ] Verified in integration tests

## Definition of Done
- [ ] All code committed to GitHub
- [ ] All tests passing
- [ ] Documentation updated
- [ ] No compiler warnings
- [ ] Code review approved
- [ ] Merged and verified
```

---

## 🎯 COMMIT MESSAGE FORMAT

**MUST follow this format for all commits:**

```
[PHASE] [WEEK] [TYPE]: Brief description

Detailed explanation of changes
- What was changed
- Why it was changed
- How to test

Files:
- file1.cpp (NEW|MODIFY|DELETE)
- file2.test.ts (NEW|MODIFY)

Tests:
- [x] Unit tests: N cases
- [x] Integration tests: M cases
- [x] Coverage: X%

Linked to: DOCUMENT_NAME.md (Section)
GitHub Issue: #N
Closes: #N (if applicable)

Co-Authored-By: [Name] <email>
Claude-Session: [link]
```

**Examples:**

```
[Phase 4] [Week 1] [Feature]: Implement TetrahedralizationEngine

Implements Delaunay-based tetrahedralization algorithm for FEA mesh generation.

Files:
- src/geometry/mesh_tetrahedralization.hpp (NEW)
- src/geometry/mesh_tetrahedralization.cpp (NEW)
- src/geometry/__tests__/tetrahedralization.test.ts (NEW)

Tests:
- [x] Unit tests: 15 cases
- [x] Integration tests: 5 cases
- [x] Coverage: 92%

Linked to: PHASE4_DETAILED_SCHEDULE.md (Week 1-2)
GitHub Issue: #456
Closes: #456
```

```
[Phase 4] [Week 2] [Fix]: Handle degenerate triangles in tetrahedralization

Fixes: Triangle collapse causes infinite loop in aspect ratio optimization.

Files:
- src/geometry/mesh_tetrahedralization.cpp (MODIFY)
- src/geometry/__tests__/tetrahedralization.test.ts (MODIFY)

Tests:
- [x] Added 3 degenerate triangle test cases
- [x] All 18 tests pass

Linked to: PHASE4_DETAILED_SCHEDULE.md (Week 1-2)
GitHub Issue: #457
Closes: #457
```

---

## ✅ WEEKLY VERIFICATION CHECKLIST

**Every Friday (end of week):**

```markdown
## Week N Completion Checklist

### Code
- [ ] All planned features implemented
- [ ] All code committed to GitHub
- [ ] All PRs merged to phase-4 branch
- [ ] npm run typecheck: PASS
- [ ] npm test: PASS (all 1220+ tests + new tests)
- [ ] Cross-platform build: PASS (Windows/Linux/WASM)

### Documentation
- [ ] Code comments complete (Doxygen)
- [ ] CHANGELOG.md updated
- [ ] API docs generated
- [ ] User guide updated (if applicable)

### Testing
- [ ] Unit tests: 100% pass
- [ ] Integration tests: 100% pass
- [ ] Performance targets met
- [ ] Code coverage: >85% for new code
- [ ] No memory leaks (Valgrind clean)

### Issues & Projects
- [ ] All week's Issues moved to "Done"
- [ ] GitHub Project updated
- [ ] Next week's Issues created and planned

### Code Review
- [ ] All PRs code-reviewed
- [ ] All feedback addressed
- [ ] All PRs merged

### Status Report
Generate: `git log --oneline origin/phase-4...origin/ccr-68cf0b86-6w8ffc --since="7 days ago"`

Report format:
```
Week [N] Status Report
- Issues closed: N
- PRs merged: M
- Code written: X LOC
- Tests added: Y
- Bugs fixed: Z
```
```

---

## 🔔 REMINDERS - NEVER FORGET!

### Before Every Commit
1. ✅ Read relevant section of PHASE4_DETAILED_SCHEDULE.md
2. ✅ Check acceptance criteria match
3. ✅ Run `npm run typecheck` - must pass
4. ✅ Run `npm test` - all tests must pass
5. ✅ Write comprehensive commit message
6. ✅ Include GitHub Issue #
7. ✅ Include document link
8. ✅ Include Co-Authored-By

### Before Every PR
1. ✅ All tests pass (1220+ Jest, 72 C++, + new)
2. ✅ Coverage >85% for new code
3. ✅ Documentation complete
4. ✅ Code review checklist filled
5. ✅ Linked documents referenced
6. ✅ GitHub Issue referenced
7. ✅ Ready for code review approval

### Before Every Merge
1. ✅ All code review comments addressed
2. ✅ All status checks passed (✅)
3. ✅ Weekly verification checklist done
4. ✅ Phase 4 branch is green
5. ✅ No merge conflicts
6. ✅ Ready for next week's work

---

## 📈 METRICS DASHBOARD

**Track these metrics weekly:**

```
Phase 4 Progress Dashboard
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Week 1-2: FEA Solver
  Issues Created: 3
  Issues Completed: __/3 (0%)
  Code Written: ____/900 LOC
  Tests Written: ____/35 cases
  PRs Merged: __/3
  Code Coverage: __/85%

Week 3: Stress Visualization
  [Similar tracking]

Week 4: Parametric Design
  [Similar tracking]

Overall Status:
  Total LOC Written: ____/8000
  Total Tests: ____/120
  Total PRs: ____/40
  Estimated Completion: ____%
```

---

## 🚀 TOOLS & AUTOMATION

### GitHub Actions (CI/CD)
```yaml
# .github/workflows/phase4-ci.yml
name: Phase 4 CI

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: TypeScript Check
        run: npm run typecheck
      - name: Jest Tests
        run: npm test
      - name: C++ Tests
        run: cd backend && npm test && cd ..
      - name: Code Coverage
        run: npm test -- --coverage
      - name: Lint
        run: npm run lint
```

### Issue Templates
```markdown
# .github/ISSUE_TEMPLATE/phase4-feature.md
---
name: Phase 4 Feature
about: Track Phase 4 implementation tasks

---

## Phase & Week
Phase: 4
Week: [1-12]

## Linked Documents
- PHASE4_DETAILED_SCHEDULE.md
- IMPLEMENTATION_MASTER_PLAN.md

## Tasks
- [ ] Task 1
- [ ] Task 2

## Acceptance Criteria
- [ ] Code merged
- [ ] Tests passing
- [ ] Documentation complete
```

---

## 📞 ESCALATION PROCESS

**If something is blocked:**

1. **Comment on GitHub Issue** with blockers
2. **Tag PM/Tech Lead** for decision
3. **Create BLOCKER label** on issue
4. **Notify in standup** (if daily standup exists)
5. **Document** in issue why blocked
6. **Don't commit** if blocker unresolved

---

## SUMMARY: THE CHECKLIST YOU'LL FOLLOW

### Every Day:
- [ ] Read PHASE4_DETAILED_SCHEDULE.md for today's tasks
- [ ] Create/update GitHub Issue for your task
- [ ] Commit work at end of day with proper format
- [ ] Push to feature branch

### Every PR:
- [ ] Link to GitHub Issue
- [ ] Link to plan document
- [ ] Include acceptance criteria
- [ ] Code review checklist
- [ ] All tests pass

### Every Week:
- [ ] Friday: Weekly verification checklist
- [ ] Update GitHub Project status
- [ ] Create next week's Issues
- [ ] Generate status report
- [ ] Merge all PRs to phase-4 branch

### Every Phase:
- [ ] All documentation updated
- [ ] Changelog complete
- [ ] Performance benchmarks met
- [ ] Cross-platform builds verified
- [ ] Stakeholder sign-off obtained

---

**This workflow GUARANTEES no changes are forgotten!** 🎯
