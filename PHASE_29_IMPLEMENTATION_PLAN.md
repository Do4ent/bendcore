# PHASE 29: CommandManager Ribbon UI - Implementation Plan

**Status:** Ready for Development  
**Date:** 2026-10-22  
**Duration:** 7 days (6 dev + 1 QA)  
**Stages:** 6 sequential phases with step-by-step guidance

---

## OVERVIEW

This is the detailed implementation guide for Phase 29. It provides step-by-step coding instructions, file paths, code templates, and verification checkpoints for all 6 stages of development.

**For quick overview:** See PHASE_29_QUICK_START.md  
**For full architecture:** See PHASE_29_ARCHITECTURE.md  
**For command details:** See PHASE_29_COMMAND_REFERENCE.md  

---

## STAGE 1: FOUNDATION (Days 1-2)

### Objective
Create core color scheme, type definitions, and basic React components. Establish project structure.

### Prerequisites
- Node.js 16+ and npm installed
- TypeScript 5+ configured
- Project directory structure in place

---

## Step 1.1: ColorScheme Module

**File:** `/home/claude/bendcore/ui/src/components/Icons/ColorScheme.ts`

Copy the complete ColorScheme.ts code from **PHASE_29_ARCHITECTURE.md Section 5.2** OR from **PHASE_29_COMMAND_REFERENCE.md**.

**Verification:**
```bash
cd /home/claude/bendcore/ui
npm run build  # Should compile with no errors
npx tsc --noEmit  # Should pass type checking
```

**Checklist:**
- [ ] File created at correct path
- [ ] All 11 colors defined with hex and RGB
- [ ] All utility functions implemented
- [ ] Compiles without errors
- [ ] No TypeScript errors

---

## Step 1.2: Type Definitions

**Files:**
- `/home/claude/bendcore/ui/src/types/ribbon.ts`
- `/home/claude/bendcore/ui/src/types/commands.ts`

Copy the complete type definitions from **PHASE_29_ARCHITECTURE.md Section 5.2** OR from **PHASE_29_COMMAND_REFERENCE.md**.

**Key Components:**
- RibbonState interface
- CommandRequest/CommandResponse interfaces
- COMMAND_REGISTRY with 45+ commands
- Command enums

**Verification:**
```bash
npm run lint  # Should pass all checks
npm run build  # Should compile
```

**Checklist:**
- [ ] ribbon.ts created with all interfaces
- [ ] commands.ts created with COMMAND_REGISTRY
- [ ] All 45+ commands defined
- [ ] Type checking passes
- [ ] No linting warnings

---

## Step 1.3: React Components

**Files to Create:**
```
/home/claude/bendcore/ui/src/components/CommandManager/
├── RibbonBar.tsx
├── RibbonTab.tsx
├── RibbonGroup.tsx
├── RibbonButton.tsx
├── RibbonBar.module.css
├── RibbonTab.module.css
├── RibbonGroup.module.css
├── RibbonButton.module.css
└── index.ts
```

Copy code from **PHASE_29_ARCHITECTURE.md Section 5.2** for each component.

**Verification:**
```bash
npm run build  # No errors
npm run lint -- components/CommandManager  # No warnings
```

**Checklist:**
- [ ] RibbonBar component created and imports correctly
- [ ] RibbonTab component created
- [ ] RibbonButton component created
- [ ] RibbonGroup component created
- [ ] CSS modules created with styling
- [ ] index.ts created with exports
- [ ] Build successful
- [ ] No console errors

---

## Step 1.4: useCommandDispatcher Hook

**File:** `/home/claude/bendcore/ui/src/hooks/useCommandDispatcher.ts`

Copy complete hook from **PHASE_29_ARCHITECTURE.md Section 6.3**.

**Key Functions:**
- executeCommand(cmd: CommandRequest)
- validateCommand(cmdId: string)
- queueCommand(cmd: CommandRequest)
- flushCommandQueue()

**Verification:**
```bash
npm run build
npm run lint src/hooks/useCommandDispatcher.ts
```

**Checklist:**
- [ ] Hook file created
- [ ] Integrates with useIpc hook
- [ ] executeCommand validates and sends IPC
- [ ] Error handling implemented
- [ ] Compiles without errors

---

## Stage 1 Completion

### Success Criteria
- [ ] ColorScheme module complete (11 colors)
- [ ] Type definitions complete (ribbon.ts, commands.ts)
- [ ] COMMAND_REGISTRY with all 45+ commands
- [ ] RibbonBar renders with 5 tabs
- [ ] Tab switching works
- [ ] useCommandDispatcher hook functional
- [ ] No TypeScript errors
- [ ] No build warnings
- [ ] Project compiles successfully

### Expected Output
- Ribbon UI renders correctly
- All 5 tabs visible and switchable
- Colors applied to color scheme
- Commands registered in registry
- IPC communication skeleton ready

**Duration:** 16-20 hours  
**Next Stage:** Icons & Visual Design

---

## STAGE 2: ICONS (Days 3-4)

### Objective
Create 11 icon sets for all color categories with proper styling and states.

### Tasks
1. Create 11 icon component files
2. Implement SVG icons (or use Material Design Icons)
3. Apply color mapping per category
4. Add hover/active/disabled states

### Deliverables
- 11 icon category components
- 45+ command icons
- Hover/active/disabled states
- Icon-to-command mapping

---

## STAGE 3: TAB POPULATION (Days 4-5)

### Objective
Populate all 5 tabs with commands and verify structure.

### Tasks
1. Home Tab: 9 commands (Clipboard, File)
2. Part Tab: 11 commands (Sketch, Tube)
3. Assembly Tab: 8 commands (Import, Assembly, Tools)
4. Analysis Tab: 7 commands (Validation, Inspection)
5. Drawing Tab: 6 commands (View, Export)

### Deliverables
- All tabs fully populated
- All commands visible and clickable
- Visual verification complete
- No missing or duplicate commands

---

## STAGE 4: C++ BACKEND (Days 5-6)

### Objective
Implement C++ command handlers and integrate with IPC bridge.

### Files to Create
```
/home/claude/bendcore/src/
├── ribbon_command_handler.hpp
├── ribbon_command_handler.cpp
├── tab_handlers/
│   ├── home_tab_handler.hpp/cpp
│   ├── part_tab_handler.hpp/cpp
│   ├── assembly_tab_handler.hpp/cpp
│   ├── analysis_tab_handler.hpp/cpp
│   └── drawing_tab_handler.hpp/cpp
└── ipc_bridge.cpp [MODIFY - add routing]
```

### Implementation Steps
1. Create CommandHandler interface
2. Implement 5 tab-specific handlers
3. Create RibbonCommandDispatcher
4. Wire into ipc_bridge.cpp
5. Handle command serialization

### Deliverables
- All 45+ commands routed to handlers
- IPC message handling functional
- Commands execute in C++
- Error handling in place

---

## STAGE 5: TESTING (Days 6-7)

### Objective
Comprehensive testing and quality assurance.

### Test Types
1. Unit Tests (React components)
2. Integration Tests (React ↔ C++)
3. Visual Regression Tests
4. Accessibility Tests (WCAG AA)
5. Performance Tests

### Coverage Goal
- 95%+ code coverage
- All critical paths tested
- Edge cases handled
- Error scenarios tested

### Test Commands
```bash
npm test -- CommandManager        # Unit tests
npm test -- integration/ribbon    # Integration tests
npm run test:visual              # Visual regression
npm run audit:a11y               # Accessibility audit
```

---

## STAGE 6: POLISH & DEPLOYMENT (Day 7)

### Objective
Final optimization, documentation, and production readiness.

### Tasks
1. Performance optimization
2. Bundle size reduction
3. Final documentation
4. Code review
5. Deployment preparation

### Verification Checklist
- [ ] All tests passing
- [ ] 95%+ code coverage
- [ ] No TypeScript errors
- [ ] No console warnings
- [ ] Bundle size acceptable
- [ ] Performance metrics met
- [ ] Accessibility compliant
- [ ] Documentation complete

---

## Building & Verification

### Daily Build Verification
```bash
cd /home/claude/bendcore/ui

# Build
npm run build

# Lint
npm run lint

# Type Check
npx tsc --noEmit

# Tests
npm test
```

### Full System Build
```bash
cd /home/claude/bendcore

# C++ build
mkdir -p build
cd build
cmake ..
make

# Back to UI
cd ../ui
npm run build
```

---

## Common Issues & Solutions

### TypeScript Errors
```bash
# Clear and rebuild
rm -rf node_modules package-lock.json
npm install
npm run build
```

### Import Errors
- Verify file paths are correct
- Check index.ts exports in each module
- Ensure imports use correct relative paths

### IPC Connection Issues
- Verify C++ backend is running
- Check IPC message format matches spec
- Review ipc_bridge.cpp for routing errors

### Build Failures
- Clear build cache: `npm run clean`
- Verify dependencies: `npm install`
- Check TypeScript version: `npx tsc --version`

---

## Testing Examples

### Unit Test Example
```typescript
describe('RibbonButton', () => {
  it('renders with correct category color', () => {
    const { container } = render(
      <RibbonButton
        command={mockCommand}
        onClick={jest.fn()}
      />
    );
    expect(container.querySelector('[data-category]')).toBeInTheDocument();
  });
});
```

### Integration Test Example
```typescript
describe('CommandDispatcher', () => {
  it('executes command via IPC', async () => {
    const { executeCommand } = renderHook(() => useCommandDispatcher());
    const result = await executeCommand({ id: 'file.new' });
    expect(result.success).toBe(true);
  });
});
```

---

## Progress Tracking

Use this table to track implementation progress:

| Stage | Task | Status | % Complete | Notes |
|-------|------|--------|-----------|-------|
| 1 | ColorScheme | [ ] | 0% | |
| 1 | Type Definitions | [ ] | 0% | |
| 1 | React Components | [ ] | 0% | |
| 1 | useCommandDispatcher | [ ] | 0% | |
| 2 | Icon Components | [ ] | 0% | |
| 3 | Tab Population | [ ] | 0% | |
| 4 | C++ Handlers | [ ] | 0% | |
| 5 | Unit Tests | [ ] | 0% | |
| 5 | Integration Tests | [ ] | 0% | |
| 6 | Documentation | [ ] | 0% | |

---

## File Checklist

### React/TypeScript Files
- [ ] `/ui/src/components/Icons/ColorScheme.ts`
- [ ] `/ui/src/types/ribbon.ts`
- [ ] `/ui/src/types/commands.ts`
- [ ] `/ui/src/components/CommandManager/RibbonBar.tsx`
- [ ] `/ui/src/components/CommandManager/RibbonTab.tsx`
- [ ] `/ui/src/components/CommandManager/RibbonButton.tsx`
- [ ] `/ui/src/components/CommandManager/RibbonGroup.tsx`
- [ ] `/ui/src/components/CommandManager/RibbonSeparator.tsx`
- [ ] `/ui/src/components/CommandManager/*.module.css` (5 files)
- [ ] `/ui/src/components/CommandManager/index.ts`
- [ ] `/ui/src/hooks/useCommandDispatcher.ts`
- [ ] `/ui/src/components/Icons/` (11 icon category files)

### C++ Files
- [ ] `/src/ribbon_command_handler.hpp`
- [ ] `/src/ribbon_command_handler.cpp`
- [ ] `/src/tab_handlers/home_tab_handler.hpp/cpp`
- [ ] `/src/tab_handlers/part_tab_handler.hpp/cpp`
- [ ] `/src/tab_handlers/assembly_tab_handler.hpp/cpp`
- [ ] `/src/tab_handlers/analysis_tab_handler.hpp/cpp`
- [ ] `/src/tab_handlers/drawing_tab_handler.hpp/cpp`
- [ ] `/src/ipc_bridge.cpp` (MODIFIED)

### Test Files
- [ ] `/ui/src/components/CommandManager/__tests__/RibbonBar.test.tsx`
- [ ] `/ui/src/components/CommandManager/__tests__/RibbonButton.test.tsx`
- [ ] `/ui/src/hooks/__tests__/useCommandDispatcher.test.ts`

---

## Timeline

```
Day 1-2: Stage 1 (Foundation)
Day 3-4: Stage 2 (Icons)
Day 4-5: Stage 3 (Content)
Day 5-6: Stage 4 (C++ Backend)
Day 6-7: Stage 5 (Testing)
Day 7: Stage 6 (Polish)
```

---

## Success Metrics

- **Build:** Compiles with 0 errors, 0 warnings
- **Tests:** 95%+ code coverage, all passing
- **Performance:** < 100ms per command execution
- **Accessibility:** WCAG AA compliance
- **Code Quality:** No TypeScript errors, clean lint

---

## References

| Document | Usage |
|----------|-------|
| PHASE_29_ARCHITECTURE.md | Full system design, code examples |
| PHASE_29_COMMAND_REFERENCE.md | Command details, parameters |
| PHASE_29_QUICK_START.md | Quick reference, getting started |
| PHASE_29_MANIFEST.md | Project overview, checklist |

---

**Ready to start?** Begin with Step 1.1 above.  
**Need architecture details?** See PHASE_29_ARCHITECTURE.md  
**Need quick start?** See PHASE_29_QUICK_START.md  

---

Created: 2026-10-22  
Status: Ready for Development  
Next Phase: Begin Stage 1, Step 1.1

