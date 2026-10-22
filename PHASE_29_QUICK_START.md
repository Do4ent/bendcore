# PHASE 29: CommandManager Ribbon UI - Quick Start Guide

**📋 Phase Status:** Planning Complete ✅  
**🎯 Objective:** Implement 5-tab CommandManager Ribbon with 11 color categories  
**📅 Timeline:** 7 days (6 dev + 1 QA)  
**📊 Scope:** ~45 commands, 5 tabs, 11 icon categories

---

## 📂 Key Documents Created

1. **PHASE_29_ARCHITECTURE.md** (Comprehensive Design)
   - Full system architecture
   - Component hierarchy
   - Color scheme specification
   - C++ integration points
   - 17 detailed sections

2. **PHASE_29_IMPLEMENTATION_PLAN.md** (Actionable Steps)
   - Stage-by-stage breakdown
   - Code examples for Stage 1
   - Detailed checklists
   - Testing strategy
   - Success criteria

3. **PHASE_29_QUICK_START.md** (This Document)
   - Quick reference
   - Directory structure
   - Key command mappings
   - Getting started steps

---

## 🏗️ Directory Structure (After Implementation)

```
/home/claude/bendcore/ui/src/
├── components/
│   └── CommandManager/                  [NEW]
│       ├── RibbonBar.tsx               [Main ribbon container]
│       ├── RibbonTab.tsx               [Tab content]
│       ├── RibbonButton.tsx            [Individual button]
│       ├── RibbonGroup.tsx             [Button grouping]
│       ├── RibbonSeparator.tsx         [Visual separator]
│       ├── index.ts
│       ├── RibbonBar.module.css
│       ├── RibbonTab.module.css
│       ├── RibbonGroup.module.css
│       ├── RibbonButton.module.css
│       └── __tests__/                  [Unit tests]
│
├── components/Icons/                    [NEW]
│   ├── ColorScheme.ts                  [11 color definitions]
│   ├── TubeIcons.tsx                   [Tube category icons]
│   ├── CreateIcons.tsx                 [Create category icons]
│   ├── DeleteIcons.tsx                 [Delete category icons]
│   ├── ImportIcons.tsx                 [Import category icons]
│   ├── LinksIcons.tsx                  [Links category icons]
│   ├── EditIcons.tsx                   [Edit category icons]
│   ├── UpdateIcons.tsx                 [Update category icons]
│   ├── FreezeIcons.tsx                 [Freeze category icons]
│   ├── AuxiliaryIcons.tsx              [Auxiliary category icons]
│   ├── AxesIcons.tsx                   [RGB Axes category icons]
│   ├── NeutralIcons.tsx                [Neutral category icons]
│   └── index.ts
│
├── hooks/
│   ├── useIpc.ts                       [Existing - IPC integration]
│   ├── useCommandDispatcher.ts         [NEW - Command dispatch]
│   └── useRibbonState.ts               [NEW - Ribbon state management]
│
├── types/
│   ├── ribbon.ts                       [NEW - Ribbon types]
│   ├── commands.ts                     [NEW - Command registry]
│   └── colors.ts                       [NEW - Color types]
│
└── ...
```

---

## 🎨 The 11 Color Categories

| # | Name | Hex | Usage |
|---|------|-----|-------|
| 1 | **Tube** | #1F6FC5 | Tube bodies, main geometry |
| 2 | **Create** | #2E9E5B | New element creation |
| 3 | **Delete** | #D93A3A | Deletion, errors, prohibit |
| 4 | **Import** | #0E97A3 | External data, mesh |
| 5 | **Links** | #8A4FD8 | References, associativity |
| 6 | **Edit** | #E8820C | Parameter modification |
| 7 | **Update** | #F5B517 | Recalculation, sync |
| 8 | **Freeze** | #2D9CDB | Lock, freeze, constraints |
| 9 | **Auxiliary** | #C9920A | Helper geometry |
| 10 | **RGB Axes** | RGB | X(#FF0000) Y(#00FF00) Z(#0000FF) |
| 11 | **Neutral** | #757575 | UI chrome, outlines |

---

## 📋 The 5 Ribbon Tabs

### 🏠 Home Tab
- **Clipboard:** Paste, Copy, Cut
- **File:** New, Open, Save, Print, Options

### 🔧 Part Tab
- **Sketch:** Line, Arc, Point, Dimension, Constraint, Solve
- **Tube:** Blank, Material, Diameter, Technology

### 🔗 Assembly Tab
- **Import:** STEP, IGES, SAT
- **Assembly:** Insert, Port, Trace
- **Tools:** Links, Recognize

### 📊 Analysis Tab
- **Validation:** Check All, Collisions, Gaps
- **Inspection:** Measure, Mass, Report

### 🖼️ Drawing Tab
- **View:** Sheet, View, Annotations
- **Export:** Tables, Unfolding, Export

---

## 🚀 Getting Started (Stage 1 - Foundation)

### Prerequisites
```bash
cd /home/claude/bendcore/ui
npm install                    # Install dependencies
npm run build                  # Verify build works
```

### Step 1: Create ColorScheme Module
```bash
touch src/components/Icons/ColorScheme.ts
# Copy code from PHASE_29_IMPLEMENTATION_PLAN.md - Step 1.1
# Verify: npm run build (should compile with no errors)
```

### Step 2: Create Type Definitions
```bash
touch src/types/ribbon.ts
touch src/types/commands.ts
# Copy code from PHASE_29_IMPLEMENTATION_PLAN.md - Step 1.2
# Verify: npm run build && npm run lint
```

### Step 3: Create Ribbon Components
```bash
mkdir -p src/components/CommandManager
touch src/components/CommandManager/RibbonBar.tsx
touch src/components/CommandManager/RibbonTab.tsx
touch src/components/CommandManager/RibbonButton.tsx
touch src/components/CommandManager/RibbonGroup.tsx
# Copy code from PHASE_29_IMPLEMENTATION_PLAN.md - Steps 1.3-1.5
```

### Step 4: Create Command Dispatcher Hook
```bash
touch src/hooks/useCommandDispatcher.ts
# Copy code from PHASE_29_IMPLEMENTATION_PLAN.md - Step 1.4
```

### Step 5: Verification
```bash
npm run build                  # Should compile with no errors
npm run lint                   # Should have no warnings
npm test -- CommandManager     # Unit tests (once written)
```

---

## 💻 React Component Usage Example

```typescript
import { RibbonBar } from './components/CommandManager';

function App() {
  return (
    <div className="app">
      <RibbonBar
        onCommandExecute={(cmd) => {
          console.log('Command executed:', cmd.id);
        }}
        onError={(error) => {
          console.error('Command failed:', error);
        }}
      />
      <div className="viewport">
        {/* 3D viewport and other content */}
      </div>
    </div>
  );
}
```

---

## 🔗 C++ Integration Overview

### IPC Message Flow
```
React Component
  ↓ onClick()
  ↓
useCommandDispatcher Hook
  ↓ executeCommand()
  ↓
IPC (Electron/Node.js)
  ↓
ipc_bridge.cpp (RibbonCommandDispatcher)
  ↓
Tab-specific Handler (HomeTabHandler, PartTabHandler, etc.)
  ↓
Core Modules (sketch.cpp, tube_model.cpp, analysis.cpp, etc.)
```

### Required C++ Changes
- Create `src/ribbon_command_handler.hpp` (interface)
- Implement tab-specific handlers
- Create `RibbonCommandDispatcher` class
- Wire into `ipc_bridge.cpp`

---

## 📌 Implementation Phases

### Phase 1: Foundation (Days 1-2) ✅ In Plan
- ColorScheme module
- Type definitions
- Component skeleton
- Command dispatcher hook

### Phase 2: Icons (Days 3-4)
- SVG icon components
- Icon mapping
- Hover/active states

### Phase 3: Content (Days 4-5)
- Tab population
- Command registry verification
- Visual testing

### Phase 4: C++ Backend (Days 5-6)
- Handler classes
- Message routing
- Command execution

### Phase 5: Testing (Days 6-7)
- Unit tests
- Integration tests
- Visual regression
- Accessibility audit

---

## ✅ Pre-Implementation Checklist

Before starting development:

- [ ] Read PHASE_29_ARCHITECTURE.md (full understanding)
- [ ] Review PHASE_29_IMPLEMENTATION_PLAN.md (detailed steps)
- [ ] Verify `/home/claude/bendcore/ui` directory structure
- [ ] Check Node.js and npm versions
- [ ] Verify TypeScript configuration
- [ ] Review existing useIpc.ts hook
- [ ] Review BENDCORE_SPECIFICATION_FINAL_LOCKED.md (context)
- [ ] Set up IDE with TypeScript support

---

## 🔍 Common Gotchas & Solutions

### Issue: Colors not appearing
**Solution:** Ensure ColorScheme.ts is properly imported and CSS variables are set

### Issue: Commands not executing
**Solution:** Check COMMAND_REGISTRY has all entries, verify IPC bridge is connected

### Issue: TypeScript errors
**Solution:** Run `npm run lint -- --fix` to auto-fix, verify all types are imported

### Issue: Component not rendering
**Solution:** Verify index.ts exports, check import paths in parent component

---

## 📚 Key Files to Reference

| File | Purpose | Location |
|------|---------|----------|
| ColorScheme.ts | Color definitions | `ui/src/components/Icons/ColorScheme.ts` |
| commands.ts | Command registry | `ui/src/types/commands.ts` |
| ribbon.ts | Type definitions | `ui/src/types/ribbon.ts` |
| useIpc.ts | IPC integration | `ui/src/hooks/useIpc.ts` |
| BENDCORE_SPECIFICATION_FINAL_LOCKED.md | Requirements | `/home/claude/bendcore/` |

---

## 🎓 Learning Resources

- React Hooks: https://react.dev/reference/react
- TypeScript Types: https://www.typescriptlang.org/docs/handbook/
- CSS Modules: https://create-react-app.dev/docs/adding-a-css-modules-stylesheet/
- Electron IPC: https://www.electronjs.org/docs/latest/tutorial/ipc

---

## 👥 Team Roles

**Frontend Developer:**
- Implement React components (RibbonBar, RibbonTab, etc.)
- Create icon components
- Write unit tests
- Handle styling & responsive design

**C++ Developer:**
- Implement CommandHandler interface
- Create tab-specific handlers
- Wire into ipc_bridge.cpp
- Handle command execution

**QA/Testing:**
- Visual regression testing
- Integration testing
- Accessibility audit
- Performance testing

---

## 📞 Troubleshooting

### Build fails
```bash
npm install                    # Reinstall dependencies
npm run clean                  # Clean build artifacts
npm run build                  # Rebuild
```

### TypeScript errors
```bash
npm run lint -- --fix          # Auto-fix linting issues
npx tsc --noEmit               # Type check
```

### Tests failing
```bash
npm test -- --watch            # Run in watch mode
npm test -- --coverage         # Generate coverage report
```

---

## 📊 Progress Tracking

Use this checklist to track implementation progress:

### Stage 1: Foundation
- [ ] ColorScheme module created
- [ ] Type definitions complete
- [ ] RibbonBar component rendering
- [ ] Tab switching working
- [ ] useCommandDispatcher hook functional

### Stage 2: Icons
- [ ] All 11 icon categories implemented
- [ ] Icons rendering correctly
- [ ] Hover states working
- [ ] Color mapping verified

### Stage 3: Content
- [ ] All 5 tabs populated
- [ ] All 45+ commands in registry
- [ ] Visual verification complete
- [ ] No visual glitches

### Stage 4: C++ Backend
- [ ] CommandHandler interface created
- [ ] All tab handlers implemented
- [ ] IPC routing verified
- [ ] Commands executing end-to-end

### Stage 5: Testing
- [ ] Unit tests written and passing
- [ ] Integration tests written and passing
- [ ] Visual regression tests passing
- [ ] Accessibility audit passed
- [ ] Performance acceptable

---

## 🎯 Success Criteria

By end of Phase 29:

- ✅ CommandManager Ribbon fully implemented
- ✅ All 5 tabs with correct commands
- ✅ All 11 colors applied correctly
- ✅ IPC communication working
- ✅ Commands executing in C++ backend
- ✅ No TypeScript errors
- ✅ WCAG AA accessibility compliance
- ✅ 95%+ test coverage
- ✅ Zero console warnings

---

## 📝 Notes

- All color values are LOCKED in specification
- Command registry is definitive source of truth
- Tab structure follows specification Section 6
- Keep component hierarchy clean for maintainability
- Use CSS modules for style scoping
- Implement proper error handling for IPC failures

---

**Ready to start?** → Begin with Stage 1 in PHASE_29_IMPLEMENTATION_PLAN.md

**Questions?** → Reference PHASE_29_ARCHITECTURE.md

**Full Spec?** → See BENDCORE_SPECIFICATION_FINAL_LOCKED.md

---

**Created:** 2026-10-22  
**Version:** 1.0  
**Status:** Ready for Development

---

*This is your quick reference guide. Bookmark it and refer back frequently!*
