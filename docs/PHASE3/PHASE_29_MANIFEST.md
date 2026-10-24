# PHASE 29: CommandManager Ribbon UI - Project Manifest

**Status:** Planning Phase COMPLETE ✅  
**Date:** 2026-10-22  
**Duration:** 7 days (6 dev + 1 QA)  
**Deliverables:** 4 documents + ready-to-code templates

---

## 📦 Deliverables

This project includes comprehensive documentation and implementation planning for Phase 29 of BendCore CAD development.

### Documents Created

1. **PHASE_29_ARCHITECTURE.md** (17 sections, ~3,500 words)
   - Complete system architecture
   - Component hierarchy and specifications
   - Color scheme (11 categories - LOCKED)
   - C++ integration architecture
   - Testing strategy
   - References and sign-off

2. **PHASE_29_IMPLEMENTATION_PLAN.md** (~4,000 words)
   - Stage-by-stage breakdown
   - Code examples for Stage 1 (Foundation)
   - Step-by-step implementation guide
   - File paths and file creation instructions
   - Checklists for each step
   - Testing and verification procedures

3. **PHASE_29_COMMAND_REFERENCE.md** (~2,500 words)
   - Complete command registry (45+ commands)
   - Command metadata and parameters
   - Color category mapping
   - C++ handler signatures
   - Validation rules
   - Testing checklist

4. **PHASE_29_QUICK_START.md** (~1,500 words)
   - Quick reference guide
   - Getting started steps
   - Directory structure
   - Common gotchas and solutions
   - Progress tracking
   - Success criteria

---

## 🎯 Project Overview

### Objective
Implement CommandManager Ribbon UI with 5 contextual tabs and 11 color-coded icon categories, fully integrated with C++ backend via IPC.

### Scope
- **Tabs:** 5 (Home, Part, Assembly, Analysis, Drawing)
- **Commands:** 45+
- **Color Categories:** 11 (LOCKED specification)
- **Integration:** React + TypeScript (frontend), C++ (backend)
- **IPC:** Electron/Node.js message passing

### Key Metrics
- **Lines of Code:** ~2,500 (React) + ~1,500 (C++)
- **Files to Create:** 20+
- **Complexity:** Medium-High
- **Team Size:** 1-2 developers

---

## 📂 Project Structure (Post-Implementation)

```
/home/claude/bendcore/
├── ui/src/
│   ├── components/
│   │   ├── CommandManager/          [NEW - 5 files]
│   │   └── Icons/                   [NEW - 13 files]
│   ├── hooks/
│   │   ├── useCommandDispatcher.ts  [NEW]
│   │   └── useRibbonState.ts        [NEW - Optional]
│   └── types/
│       ├── ribbon.ts                [NEW]
│       └── commands.ts              [NEW]
│
├── src/
│   ├── ribbon_command_handler.hpp   [NEW]
│   ├── ribbon_command_handler.cpp   [NEW]
│   ├── tab_handlers/                [NEW - 5 files]
│   └── ipc_bridge.cpp               [MODIFIED - integration]
│
└── [Documentation files - this directory]
    ├── PHASE_29_ARCHITECTURE.md
    ├── PHASE_29_IMPLEMENTATION_PLAN.md
    ├── PHASE_29_COMMAND_REFERENCE.md
    ├── PHASE_29_QUICK_START.md
    └── PHASE_29_MANIFEST.md (this file)
```

---

## 🎨 The 11 Color Categories (LOCKED)

| # | Name | Hex | RGB | Usage |
|---|------|-----|-----|-------|
| 1 | Tube | #1F6FC5 | (31, 111, 197) | Tube bodies, main elements |
| 2 | Create | #2E9E5B | (46, 158, 91) | Creation commands |
| 3 | Delete | #D93A3A | (217, 58, 58) | Deletion, errors |
| 4 | Import | #0E97A3 | (14, 151, 163) | Imported data |
| 5 | Links | #8A4FD8 | (138, 79, 216) | References |
| 6 | Edit | #E8820C | (232, 130, 12) | Parameter change |
| 7 | Update | #F5B517 | (245, 181, 23) | Recalculation |
| 8 | Freeze | #2D9CDB | (45, 156, 219) | Lock, freeze |
| 9 | Auxiliary | #C9920A | (201, 146, 10) | Helper geometry |
| 10 | RGB Axes | RGB | Multi | X/Y/Z axes |
| 11 | Neutral | #757575 | (117, 117, 117) | UI chrome |

---

## 📋 The 5 Ribbon Tabs

| Tab | Groups | Commands | Purpose |
|-----|--------|----------|---------|
| **Home** | Clipboard, File | 9 | Document management |
| **Part** | Sketch, Tube | 11 | Sketch editing & tube creation |
| **Assembly** | Import, Assembly, Tools | 8 | Environment integration |
| **Analysis** | Validation, Inspection | 7 | Validation & reporting |
| **Drawing** | View, Export | 6 | Technical drawing |

---

## 🚀 Implementation Phases

### Phase 1: Foundation (Days 1-2)
**Deliverable:** Ribbon UI renders with tab switching
- ColorScheme module (11 colors)
- Type definitions (ribbon.ts, commands.ts)
- React component skeleton (RibbonBar, RibbonTab, RibbonButton, RibbonGroup)
- Command dispatcher hook
- **Status:** Fully documented in IMPLEMENTATION_PLAN.md

### Phase 2: Icons (Days 3-4)
**Deliverable:** All icons render with colors
- 11 icon category components
- SVG icons for all 45+ commands
- Hover/active/disabled states
- Icon-to-command mapping

### Phase 3: Content (Days 4-5)
**Deliverable:** All tabs populated with commands
- Tab structure verification
- Command registry population
- Visual testing

### Phase 4: C++ Backend (Days 5-6)
**Deliverable:** IPC communication working
- CommandHandler interface
- Tab-specific handlers
- Message routing
- Command execution

### Phase 5: Testing & Polish (Days 6-7)
**Deliverable:** Production-ready code
- Unit tests (95%+ coverage)
- Integration tests
- Visual regression tests
- Accessibility audit
- Performance optimization

---

## 🔧 Key Technologies

### Frontend
- **React 18+** - Component framework
- **TypeScript 5+** - Type safety
- **CSS Modules** - Style scoping
- **React Hooks** - State management

### Backend
- **C++17+** - Core implementation
- **Electron/Node IPC** - Message passing
- **JSON** - Message serialization

### Tools
- **npm** - Package management
- **CMake** - C++ build
- **Jest** - Testing framework

---

## 📚 Documentation Reference

### For Getting Started
1. Read **PHASE_29_QUICK_START.md** (15 min read)
2. Review **PHASE_29_ARCHITECTURE.md** Section 3-5 (30 min)
3. Start implementing Stage 1 from **PHASE_29_IMPLEMENTATION_PLAN.md**

### For Implementation Details
1. Follow **PHASE_29_IMPLEMENTATION_PLAN.md** step-by-step
2. Reference **PHASE_29_COMMAND_REFERENCE.md** for command details
3. Check **PHASE_29_ARCHITECTURE.md** Section 6-8 for C++ integration

### For Command Definitions
- **PHASE_29_COMMAND_REFERENCE.md** - All 45+ commands with parameters
- **PHASE_29_ARCHITECTURE.md** Section 2 - Tab structure

### For Testing
- **PHASE_29_ARCHITECTURE.md** Section 11 - Testing strategy
- **PHASE_29_IMPLEMENTATION_PLAN.md** - Test examples

---

## ✅ Success Criteria

By end of Phase 29:

✅ **Functional Requirements**
- [ ] All 5 tabs render correctly
- [ ] All 45+ commands accessible
- [ ] Tab switching works smoothly
- [ ] Commands dispatch to C++ backend
- [ ] C++ handlers execute correctly
- [ ] No command failures in happy path

✅ **Visual/Design Requirements**
- [ ] All 11 colors applied correctly
- [ ] Icons render correctly for all commands
- [ ] Hover/active states work
- [ ] Responsive layout on different screen sizes
- [ ] No visual glitches or misalignment

✅ **Code Quality Requirements**
- [ ] 95%+ TypeScript coverage
- [ ] Zero TypeScript errors
- [ ] All linting rules pass
- [ ] WCAG AA accessibility compliance
- [ ] No console warnings

✅ **Testing Requirements**
- [ ] Unit tests: 95%+ coverage
- [ ] Integration tests passing
- [ ] Visual regression tests passing
- [ ] Accessibility audit passed
- [ ] Performance acceptable (< 100ms per command)

---

## 🤝 Team Responsibilities

### Frontend Developer
- Implement React components
- Create icon components
- Write unit tests
- Handle styling & responsive design
- Visual verification

### C++ Developer
- Implement CommandHandler interface
- Create tab-specific handlers
- Wire into ipc_bridge.cpp
- Handle command execution
- C++ unit tests

### QA/Testing
- Integration testing
- Visual regression testing
- Accessibility audit
- Performance testing
- Cross-browser testing

---

## 📊 Project Statistics

| Metric | Value |
|--------|-------|
| Total Documents | 4 |
| Total Pages | ~20 |
| Commands Defined | 45+ |
| Color Categories | 11 |
| React Components | 5 |
| Icon Categories | 11 |
| C++ Handlers | 5 |
| Type Definitions | 8+ |
| Estimated LOC (React) | 2,500 |
| Estimated LOC (C++) | 1,500 |
| Test Coverage Goal | 95%+ |

---

## 🔍 Quality Assurance Checklist

### Before Starting Implementation
- [ ] Read all 4 documents
- [ ] Understand architecture completely
- [ ] Review BENDCORE_SPECIFICATION_FINAL_LOCKED.md
- [ ] Set up development environment
- [ ] Verify dependencies installed

### During Development (Daily)
- [ ] Run `npm run build` without errors
- [ ] Run `npm run lint` without warnings
- [ ] Run tests with `npm test`
- [ ] Update progress in tracking sheet
- [ ] Review each day's code changes

### Before Release (End of Phase)
- [ ] All success criteria met
- [ ] All tests passing (95%+ coverage)
- [ ] No TypeScript errors
- [ ] No console warnings/errors
- [ ] Accessibility audit passed
- [ ] Code review completed
- [ ] Performance benchmarks acceptable

---

## 🚨 Known Constraints & Dependencies

### Constraints
- Color values are LOCKED - no modifications allowed
- Tab structure follows specification - cannot be changed
- Command registry is source of truth - all commands must exist there
- IPC message format must match specification
- Keyboard shortcuts are defined - conflicts must be resolved

### Dependencies
- Existing useIpc.ts hook (working)
- C++ ipc_bridge.cpp (partially implemented)
- BendCore core modules (sketch.cpp, tube_model.cpp, etc.)
- React 18+ with TypeScript support
- Electron or Node.js IPC mechanism

### External References
- BENDCORE_SPECIFICATION_FINAL_LOCKED.md (source of truth)
- bendcore_detailed_verification.md (verification)
- Assembly-Color.dc.html artifact (reference UI)

---

## 📞 Quick Links

| Resource | Location |
|----------|----------|
| Architecture | `/home/claude/bendcore/PHASE_29_ARCHITECTURE.md` |
| Implementation Plan | `/home/claude/bendcore/PHASE_29_IMPLEMENTATION_PLAN.md` |
| Command Reference | `/home/claude/bendcore/PHASE_29_COMMAND_REFERENCE.md` |
| Quick Start | `/home/claude/bendcore/PHASE_29_QUICK_START.md` |
| Source Repo | `github.com/Do4ent/bendcore` |
| Specification | `/home/claude/bendcore/BENDCORE_SPECIFICATION_FINAL_LOCKED.md` |

---

## 🎓 Learning Path

1. **Day 1 Morning:** Read all 4 documents (2-3 hours)
2. **Day 1 Afternoon:** Set up environment, review code structure (1-2 hours)
3. **Days 2-3:** Implement Stage 1 (Foundation) following step-by-step guide
4. **Days 3-4:** Implement Stage 2 (Icons)
5. **Days 4-5:** Implement Stages 3-4 (Content & Backend)
6. **Days 6-7:** Testing, optimization, and final polish

---

## 📝 Notes for Developers

- **This is NOT a quick task.** Allocate full 7 days for quality implementation.
- **Follow the specification exactly.** All requirements are locked.
- **Test frequently.** Don't wait until the end.
- **Ask questions early.** The architecture docs are comprehensive but contact with questions.
- **Use the checklists.** They're designed to keep you on track.
- **Review progress daily.** Update tracking as you go.

---

## 🎯 Final Checklist

Before considering Phase 29 complete:

- [ ] All 4 documents read and understood
- [ ] Environment set up successfully
- [ ] Stage 1 code compiling with no errors
- [ ] All 5 tabs rendering
- [ ] All 45+ commands in registry
- [ ] All 11 colors implemented
- [ ] IPC communication working
- [ ] All C++ handlers implemented
- [ ] Unit tests passing (95%+ coverage)
- [ ] Integration tests passing
- [ ] Visual regression tests passing
- [ ] Accessibility audit passed
- [ ] No console warnings/errors
- [ ] Code reviewed and approved
- [ ] Specification compliance verified

---

## 📞 Support & Questions

If you have questions about:
- **Architecture:** See PHASE_29_ARCHITECTURE.md (all sections)
- **Implementation:** See PHASE_29_IMPLEMENTATION_PLAN.md (step-by-step)
- **Commands:** See PHASE_29_COMMAND_REFERENCE.md (detailed)
- **Getting Started:** See PHASE_29_QUICK_START.md (practical)

---

## 📄 Document Versions

| Document | Status | Lines | Updated |
|----------|--------|-------|---------|
| PHASE_29_ARCHITECTURE.md | Locked | 800+ | 2026-10-22 |
| PHASE_29_IMPLEMENTATION_PLAN.md | Locked | 900+ | 2026-10-22 |
| PHASE_29_COMMAND_REFERENCE.md | Locked | 650+ | 2026-10-22 |
| PHASE_29_QUICK_START.md | Locked | 450+ | 2026-10-22 |
| PHASE_29_MANIFEST.md | Locked | 300+ | 2026-10-22 |

---

**Project Status:** ✅ Ready for Development  
**Planning Phase:** ✅ Complete  
**Documentation:** ✅ Complete and Locked  
**Implementation Phase:** ⏳ Ready to Start

**Created:** 2026-10-22  
**By:** Claude Code Agent  
**For:** BendCore CAD Team

---

*This manifest is your master document for Phase 29. Print it, bookmark it, and refer back frequently. Everything you need is in the 4 accompanying documents.*
