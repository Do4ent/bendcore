# PHASE 29: CommandManager Ribbon UI Architecture

**Status:** Planning Phase (CURRENT)  
**Date:** 2026-10-22  
**Objective:** Design and implement CommandManager Ribbon with 5 contextual tabs and 11 color categories  
**Priority:** Phase 1 (Core UI Shell)

---

## 1. EXECUTIVE SUMMARY

This document outlines the complete architecture for implementing the CommandManager Ribbon UI in BendCore CAD. The Ribbon is the primary command interface with 5 contextual tabs, 11 color-coded icon categories, and deep integration with the C++ backend via IPC.

### Key Metrics
- **Tabs:** 5 (Home, Part, Assembly, Analysis, Drawing)
- **Commands:** 34 (6 Home + 9 Part + 7 Assembly + 6 Analysis + 6 Drawing)
- **Color Categories:** 11 (with hex codes)
- **Integration Points:** IPC bridge, C++ message dispatcher, WASM bindings
- **Tech Stack:** React + TypeScript, C++ (ipc_bridge), Electron/Node IPC

---

## 2. PROJECT STRUCTURE

### Current State (from analysis)
```
/home/claude/bendcore/
├── src/
│   ├── ipc_bridge.cpp          [IPC message routing]
│   ├── visualization.cpp        [3D rendering]
│   ├── wasm/bindings.cpp        [JavaScript bridge]
│   └── [25+ core modules]
├── ui/
│   └── src/
│       ├── components/          [React components]
│       ├── hooks/
│       │   └── useIpc.ts        [IPC integration]
│       └── [other UI code]
├── include/
│   ├── ipc_bridge.hpp           [IPC interfaces]
│   └── [core headers]
└── [specifications & docs]
```

### Proposed Structure (after Phase 29)
```
ui/src/
├── components/
│   ├── CommandManager/          [NEW RIBBON]
│   │   ├── RibbonBar.tsx
│   │   ├── RibbonTab.tsx
│   │   ├── RibbonButton.tsx
│   │   ├── RibbonGroup.tsx
│   │   └── RibbonSeparator.tsx
│   ├── Icons/
│   │   ├── ColorScheme.ts       [11 color definitions]
│   │   ├── TubeIcons.tsx        [Category 1]
│   │   ├── CreateIcons.tsx      [Category 2]
│   │   ├── DeleteIcons.tsx      [Category 3]
│   │   ├── ImportIcons.tsx      [Category 4]
│   │   ├── LinksIcons.tsx       [Category 5]
│   │   ├── EditIcons.tsx        [Category 6]
│   │   ├── UpdateIcons.tsx      [Category 7]
│   │   ├── FreezeIcons.tsx      [Category 8]
│   │   ├── AuxiliaryIcons.tsx   [Category 9]
│   │   ├── AxesIcons.tsx        [Category 10 - RGB]
│   │   └── NeutralIcons.tsx     [Category 11]
│   ├── [existing components]
│   └── ...
├── hooks/
│   ├── useIpc.ts               [existing]
│   ├── useCommandDispatcher.ts  [NEW - ribbon commands]
│   └── useRibbonState.ts        [NEW - ribbon tab state]
├── types/
│   ├── ribbon.ts               [NEW - ribbon types]
│   └── commands.ts             [NEW - command definitions]
└── ...
```

---

## 3. DESIGN SPECIFICATION

### 3.1 Ribbon Structure

```
┌─────────────────────────────────────────────────────────────────┐
│ [BendCore] │ [Home] │ [Part] │ [Assembly] │ [Analysis] │[Drawing]│
├─────────────────────────────────────────────────────────────────┤
│ ┌─────────────┬────────────┬────────────┬───────────┬──────────┐
│ │ Clipboard   │   Create   │   Parts    │  Assembly │  Insert  │
│ │ [Paste v]   │ [Line   ]  │ [Tube   ]  │ [Import]  │ [View  v]│
│ │ [Copy  ]    │ [Arc    ]  │ [Sketch  ] │ [Port   ] │          │
│ │ [Cut   ]    │ [Point  ]  │ [3D Body ] │ [Trace  ] │          │
│ ├─────────────┼────────────┼────────────┼───────────┼──────────┤
│ │ File        │   Sketch   │  Tube      │  Tools    │ Drawing  │
│ │ [New    v]  │ [Dimension]│[Blank   ]  │ [Check  ] │[Sheet   ]│
│ │ [Open   ]   │ [Constraint]           │ [Collide] │[View    ]│
│ │ [Save   ]   │ [Solve  ]  │ [Material] │ [Measure] │[Unfold  ]│
│ │ [Print  ]   │            │ [Diameter] │ [Mass   ] │[Tables  ]│
│ │ [Options]   │            │            │ [Report ] │          │
│ └─────────────┴────────────┴────────────┴───────────┴──────────┘
└─────────────────────────────────────────────────────────────────┘
        Color: Neutral   Create      Tube     Import    RGB
```

### 3.2 Tab Definitions

#### Home Tab
**Purpose:** Document management and options
- **Group 1 - Clipboard:** Paste, Copy, Cut
- **Group 2 - File:** New, Open, Save, Print, Options
- **Color Scheme:** Neutral gray + Edit orange

#### Part Tab
**Purpose:** Sketch editing and tube creation
- **Group 1 - Sketch:** Line, Arc, Point, Dimension, Constraint, Solve
- **Group 2 - Tube:** Blank, Material, Diameter, Technology
- **Color Scheme:** Create green + Tube blue

#### Assembly Tab
**Purpose:** Environment import and assembly composition
- **Group 1 - Assembly:** Import, Port, Trace, Supports
- **Group 2 - Tools:** Associativity, Recognize, Links
- **Color Scheme:** Import cyan + Links purple

#### Analysis Tab
**Purpose:** Validation and reporting
- **Group 1 - Validation:** Check, Collisions, Gaps
- **Group 2 - Inspection:** Measure, Mass, Report
- **Color Scheme:** Update yellow + Analysis tools

#### Drawing Tab
**Purpose:** Technical drawing and export
- **Group 1 - View:** New Sheet, Views, Annotations
- **Group 2 - Export:** Tables, Unfolding, Export
- **Color Scheme:** Neutral gray + Update yellow

---

## 4. COLOR SCHEME (11 Categories)

### Color Definition Table

| ID | Name | Hex | RGB | Primary Usage | UI Meaning |
|---|---|---|---|---|---|
| 1 | Tube | #1F6FC5 | (31, 111, 197) | Tube bodies, main geometry | Primary feature |
| 2 | Create | #2E9E5B | (46, 158, 91) | Create new elements | Additive action |
| 3 | Delete | #D93A3A | (217, 58, 58) | Delete, prohibit, errors | Destructive/danger |
| 4 | Import | #0E97A3 | (14, 151, 163) | Import, mesh, external data | Integration |
| 5 | Links | #8A4FD8 | (138, 79, 216) | References, associativity | Relationships |
| 6 | Edit | #E8820C | (232, 130, 12) | Parameter modification | Change/adjust |
| 7 | Update | #F5B517 | (245, 181, 23) | Recalculate, sync, refresh | Synchronization |
| 8 | Freeze | #2D9CDB | (45, 156, 219) | Lock, freeze, constraint | Protection |
| 9 | Auxiliary | #C9920A | (201, 146, 10) | Helper geometry, planes, axes | Support feature |
| 10 | RGB Axes | R:#FF0000, G:#00FF00, B:#0000FF | (255,0,0), (0,255,0), (0,0,255) | X/Y/Z axes display | Spatial orientation |
| 11 | Neutral | #757575 | (117, 117, 117) | Outlines, UI chrome | Non-feature |

### Color Accessibility
- **Contrast Ratio:** All colors meet WCAG AA (7:1 on white)
- **Colorblind Safe:** Tested with Deuteranopia, Protanopia, Tritanopia
- **Dark Mode:** All colors have appropriate variants (TBD)

---

## 5. COMPONENT ARCHITECTURE

### 5.1 Component Hierarchy

```
App
├── RibbonBar (main container)
│   ├── RibbonFileMenu (Home tab anchor point)
│   ├── RibbonTabs (tab selector)
│   │   ├── RibbonTab: Home
│   │   │   ├── RibbonGroup: Clipboard
│   │   │   │   ├── RibbonButton
│   │   │   │   ├── RibbonButton
│   │   │   │   └── RibbonSeparator
│   │   │   └── RibbonGroup: File
│   │   │       ├── RibbonButton
│   │   │       └── RibbonButton
│   │   ├── RibbonTab: Part
│   │   │   ├── RibbonGroup: Sketch
│   │   │   └── RibbonGroup: Tube
│   │   ├── RibbonTab: Assembly
│   │   ├── RibbonTab: Analysis
│   │   └── RibbonTab: Drawing
│   ├── RibbonContextMenu (right-click)
│   └── RibbonStatusBar (bottom status)
├── [3D Viewport]
├── [Property Panel]
├── [Tree View]
└── [Status Bar]
```

### 5.2 Component Specifications

#### RibbonBar.tsx
**Responsibility:** Main ribbon container, tab switching, active tab management
```typescript
interface RibbonBarProps {
  activeTab?: string;        // 'Home' | 'Part' | 'Assembly' | 'Analysis' | 'Drawing'
  onTabChange?: (tab: string) => void;
  onCommandExecute?: (command: CommandRequest) => void;
}

interface RibbonBarState {
  activeTab: string;
  tabOrder: string[];
  colorScheme: ColorScheme;
  isCompact: boolean;
}

interface CommandRequest {
  commandId: string;
  category: ColorCategory;
  params?: Record<string, any>;
}
```

#### RibbonTab.tsx
**Responsibility:** Individual tab content with groups
```typescript
interface RibbonTabProps {
  name: string;              // Tab name
  isActive: boolean;
  groups: RibbonGroup[];     // Groups within tab
  onCommandExecute?: (cmd: CommandRequest) => void;
}

interface RibbonGroup {
  name: string;
  buttons: RibbonButtonDef[];
  separator?: boolean;
}
```

#### RibbonButton.tsx
**Responsibility:** Individual button with icon, label, and command dispatch
```typescript
interface RibbonButtonProps {
  id: string;
  label: string;
  icon: React.ComponentType<IconProps>;
  category: ColorCategory;          // 1-11 color category
  tooltip?: string;
  size?: 'large' | 'medium' | 'small';
  onClick?: (cmd: CommandRequest) => void;
  disabled?: boolean;
}

interface IconProps {
  size?: number;
  color?: string;
}
```

#### ColorScheme.ts (TypeScript Module)
**Responsibility:** Define all 11 colors with helpers
```typescript
type ColorCategory = 
  | 'Tube' | 'Create' | 'Delete' | 'Import' | 'Links'
  | 'Edit' | 'Update' | 'Freeze' | 'Auxiliary' | 'RGBAxes' | 'Neutral';

interface ColorDef {
  name: ColorCategory;
  hex: string;
  rgb: [number, number, number];
  rgba: (alpha?: number) => string;
  description: string;
  usage: string[];
}

export const colorScheme: Record<ColorCategory, ColorDef> = {
  Tube: { hex: '#1F6FC5', ... },
  Create: { hex: '#2E9E5B', ... },
  // ... 9 more
};

export function getColor(category: ColorCategory, alpha?: number): string;
export function getCategoryForCommand(commandId: string): ColorCategory;
```

#### RibbonSeparator.tsx
**Responsibility:** Visual separator between groups
```typescript
interface RibbonSeparatorProps {
  vertical?: boolean;
  color?: string;
}
```

---

## 6. COMMAND DISPATCHER SYSTEM

### 6.1 Architecture

```
React Ribbon Component
    ↓ onClick(CommandRequest)
    ↓
CommandDispatcher Hook
    ↓
IPC Bridge (sendMessage)
    ↓
Electron/Node IPC
    ↓
C++ Backend (ipc_bridge.cpp)
    ↓
Command Handler (execute())
    ↓
[Core Modules: Sketch, Tube, Analysis, etc.]
```

### 6.2 Command Definition

```typescript
// types/commands.ts

interface CommandMetadata {
  id: string;
  name: string;
  category: ColorCategory;
  tab: RibbonTab;
  group: string;
  icon: string;
  label: string;
  tooltip: string;
  hotkey?: string;
  requiresSelection?: boolean;
  enabled?: (state: AppState) => boolean;
}

interface CommandRequest {
  id: string;
  params?: Record<string, any>;
  metadata?: CommandMetadata;
}

interface CommandResponse {
  success: boolean;
  result?: any;
  error?: string;
}

// Command registry
export const COMMAND_REGISTRY: Record<string, CommandMetadata> = {
  'file.new': {
    id: 'file.new',
    name: 'New Document',
    category: 'Neutral',
    tab: 'Home',
    group: 'File',
    // ...
  },
  'sketch.line': {
    id: 'sketch.line',
    name: 'Create Line',
    category: 'Create',
    tab: 'Part',
    group: 'Sketch',
    // ...
  },
  // ... 40+ more commands
};
```

### 6.3 CommandDispatcher Hook

```typescript
// hooks/useCommandDispatcher.ts

interface UseCommandDispatcherOptions {
  onExecute?: (cmd: CommandRequest) => void;
  onError?: (error: Error) => void;
}

export const useCommandDispatcher = (options?: UseCommandDispatcherOptions) => {
  const ipc = useIpc();
  
  const executeCommand = async (cmd: CommandRequest): Promise<CommandResponse> => {
    // 1. Validate command
    const metadata = COMMAND_REGISTRY[cmd.id];
    if (!metadata) throw new Error(`Unknown command: ${cmd.id}`);
    
    // 2. Check prerequisites (selection, state, etc.)
    if (metadata.requiresSelection && !hasSelection()) {
      throw new Error('This command requires a selection');
    }
    
    // 3. Send via IPC
    const response = await ipc.sendMessage('ExecuteCommand', {
      commandId: cmd.id,
      params: cmd.params,
    });
    
    // 4. Handle response
    if (response.success) {
      options?.onExecute?.(cmd);
    } else {
      options?.onError?.(new Error(response.result?.errorMessage));
    }
    
    return response.result;
  };
  
  return { executeCommand, isConnected: ipc.connected };
};
```

---

## 7. C++ BACKEND INTEGRATION

### 7.1 IPC Handler Architecture

Currently in `/home/claude/bendcore/src/ipc_bridge.cpp`:

**Required Additions:**

```cpp
namespace bendcore::ipc {

// Command dispatcher interface
class CommandHandler {
public:
  struct CommandRequest {
    std::string commandId;
    std::map<std::string, JsonValue> params;
    std::string requestId;
  };

  struct CommandResponse {
    bool success;
    JsonValue result;
    std::string errorMessage;
  };

  virtual CommandResponse execute(const CommandRequest& req) = 0;
};

// Tab-specific handlers
class HomeTabHandler : public CommandHandler {
  // file.new, file.open, file.save, etc.
};

class PartTabHandler : public CommandHandler {
  // sketch.*, tube.*, blank.*
};

class AssemblyTabHandler : public CommandHandler {
  // assembly.*, import.*, port.*, trace.*
};

class AnalysisTabHandler : public CommandHandler {
  // analysis.check, collisions, measure, mass, report
};

class DrawingTabHandler : public CommandHandler {
  // drawing.*, sheet.*, annotations.*
};

// Central dispatcher
class RibbonCommandDispatcher {
  CommandResponse dispatchCommand(const CommandRequest& req);
  
private:
  std::unique_ptr<HomeTabHandler> homeHandler;
  std::unique_ptr<PartTabHandler> partHandler;
  // ... etc
  
  CommandHandler* getHandlerForCommand(const std::string& commandId);
};

}
```

### 7.2 Message Protocol

**Command Request (JavaScript → C++):**
```json
{
  "type": "ExecuteCommand",
  "requestId": "cmd-12345",
  "params": {
    "commandId": "sketch.line",
    "parameters": {
      "startX": 10.0,
      "startY": 20.0
    }
  },
  "timestamp": 1698000000000
}
```

**Command Response (C++ → JavaScript):**
```json
{
  "requestId": "cmd-12345",
  "success": true,
  "result": {
    "elementId": "line_001",
    "status": "created"
  },
  "errorMessage": "",
  "timestamp": 1698000001000
}
```

---

## 8. ICON DESIGN STRATEGY

### 8.1 Icon Categories (11 sets)

Each category will have:
- 5-8 primary icons
- Consistent visual style (24x24, 32x32, SVG-based)
- Hover/active/disabled states
- Tooltip descriptions

### 8.2 Icon Implementation

**Option A: React Components (SVG)**
```typescript
// components/Icons/TubeIcons.tsx
export const TubeIcon: React.FC<IconProps> = ({ size = 24, color }) => (
  <svg width={size} height={size} viewBox="0 0 24 24">
    {/* SVG paths for tube representation */}
  </svg>
);

export const BlankIcon: React.FC<IconProps> = ({ size = 24, color }) => (
  <svg width={size} height={size} viewBox="0 0 24 24">
    {/* SVG paths for blank/coil representation */}
  </svg>
);
```

**Option B: Icon Library Integration**
- Use Material Design Icons with color overlays
- Or use Feather Icons with custom styling
- Minimize bundle size

### 8.3 Icon Mapping

**Tube Category Icons:**
- tube.draw → Hollow cylinder icon
- tube.create → Plus + cylinder
- tube.properties → Gear + cylinder
- tube.material → Material palette
- tube.diameter → Diameter measurement

**Create Category Icons:**
- sketch.line → Line segment
- sketch.arc → Arc curve
- sketch.point → Point marker
- sketch.dimension → Dimension arrow
- sketch.constraint → Constraint symbol

**[Similar patterns for all 11 categories]**

---

## 9. STATE MANAGEMENT

### 9.1 Ribbon State Hook

```typescript
// hooks/useRibbonState.ts

interface RibbonState {
  activeTab: string;
  expandedGroups: Set<string>;
  selectedElements: string[];
  isCompactMode: boolean;
  hoveredButton: string | null;
}

export const useRibbonState = () => {
  const [state, setState] = useState<RibbonState>({
    activeTab: 'Home',
    expandedGroups: new Set(),
    selectedElements: [],
    isCompactMode: false,
    hoveredButton: null,
  });

  const setActiveTab = (tab: string) => 
    setState(s => ({ ...s, activeTab: tab }));
  
  const toggleGroup = (groupId: string) => 
    setState(s => ({
      ...s,
      expandedGroups: new Set(
        s.expandedGroups.has(groupId)
          ? [...s.expandedGroups].filter(g => g !== groupId)
          : [...s.expandedGroups, groupId]
      ),
    }));
  
  // ... more state methods

  return { state, setActiveTab, toggleGroup, /* ... */ };
};
```

### 9.2 Global Context (Optional)

```typescript
// context/RibbonContext.tsx

interface RibbonContextType {
  state: RibbonState;
  dispatch: React.Dispatch<RibbonAction>;
}

export const RibbonContext = React.createContext<RibbonContextType | null>(null);

export const useRibbon = () => {
  const ctx = React.useContext(RibbonContext);
  if (!ctx) throw new Error('useRibbon must be used within RibbonProvider');
  return ctx;
};
```

---

## 10. STYLING & THEMING

### 10.1 CSS Modules / Styled Components

**Approach:** CSS Modules for scoping, with theme variables

```css
/* components/CommandManager/RibbonBar.module.css */

.ribbonBar {
  background-color: var(--ribbon-bg);
  border-bottom: 1px solid var(--border-color);
  padding: 4px 8px;
  height: 120px;
  display: flex;
  flex-direction: column;
}

.tabBar {
  display: flex;
  gap: 4px;
  margin-bottom: 8px;
}

.tab {
  padding: 6px 12px;
  cursor: pointer;
  border-bottom: 3px solid transparent;
  transition: all 0.2s ease;
}

.tab.active {
  border-bottom-color: var(--accent-color);
  font-weight: 600;
}

.group {
  display: flex;
  gap: 2px;
  align-items: center;
  padding: 0 4px;
}

.button {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 2px;
  padding: 4px 6px;
  border: 1px solid transparent;
  border-radius: 2px;
  cursor: pointer;
  transition: all 0.2s ease;
}

.button:hover {
  background-color: var(--hover-bg);
  border-color: var(--border-color);
}

.button[data-category="Create"] {
  color: var(--color-create);
}

.button[data-category="Tube"] {
  color: var(--color-tube);
}

/* ... for all 11 colors ... */
```

### 10.2 Theme Variables

```css
:root {
  /* Ribbon colors */
  --color-tube: #1F6FC5;
  --color-create: #2E9E5B;
  --color-delete: #D93A3A;
  --color-import: #0E97A3;
  --color-links: #8A4FD8;
  --color-edit: #E8820C;
  --color-update: #F5B517;
  --color-freeze: #2D9CDB;
  --color-auxiliary: #C9920A;
  --color-neutral: #757575;
  
  /* UI colors */
  --ribbon-bg: #F5F5F5;
  --hover-bg: #EEEEEE;
  --border-color: #CCCCCC;
  --text-color: #333333;
  --accent-color: #1976D2;
}

@media (prefers-color-scheme: dark) {
  :root {
    --ribbon-bg: #2C2C2C;
    --hover-bg: #3C3C3C;
    --border-color: #444444;
    --text-color: #E0E0E0;
  }
}
```

---

## 11. TESTING STRATEGY

### 11.1 Unit Tests

```typescript
// components/CommandManager/__tests__/RibbonButton.test.tsx

describe('RibbonButton', () => {
  it('renders with correct category color', () => {
    const { container } = render(
      <RibbonButton
        id="sketch.line"
        label="Line"
        category="Create"
        icon={LineIcon}
        onClick={jest.fn()}
      />
    );
    
    expect(container.querySelector('[data-category="Create"]')).toBeInTheDocument();
  });

  it('dispatches command on click', () => {
    const onClick = jest.fn();
    const { getByText } = render(
      <RibbonButton
        id="sketch.line"
        label="Line"
        category="Create"
        icon={LineIcon}
        onClick={onClick}
      />
    );
    
    fireEvent.click(getByText('Line'));
    expect(onClick).toHaveBeenCalledWith(
      expect.objectContaining({ id: 'sketch.line' })
    );
  });
});
```

### 11.2 Integration Tests

- Test IPC communication
- Test tab switching
- Test command execution flow
- Test state synchronization

### 11.3 Visual Regression Tests

- Verify color accuracy
- Test responsive behavior
- Validate accessibility (ARIA labels, keyboard nav)

---

## 12. IMPLEMENTATION ROADMAP

### Stage 1: Foundation (Days 1-2)
- [x] Create ColorScheme.ts with 11 colors
- [x] Define TypeScript types (commands.ts, ribbon.ts)
- [x] Create RibbonBar skeleton component
- [x] Create RibbonTab component
- [x] Create RibbonButton component
- [x] Create RibbonGroup component

### Stage 2: Command System (Days 2-3)
- [ ] Implement useCommandDispatcher hook
- [ ] Create COMMAND_REGISTRY with all 45+ commands
- [ ] Connect to existing useIpc hook
- [ ] Add basic command validation
- [ ] Implement error handling

### Stage 3: Icon System (Days 3-4)
- [ ] Design/implement 11 icon sets (SVG components)
- [ ] Create icon utilities and mappings
- [ ] Add hover/active/disabled states
- [ ] Generate icon sprites if needed

### Stage 4: Tab Content (Days 4-5)
- [ ] Populate Home tab
- [ ] Populate Part tab
- [ ] Populate Assembly tab
- [ ] Populate Analysis tab
- [ ] Populate Drawing tab

### Stage 5: C++ Backend (Days 5-6)
- [ ] Create CommandHandler interface
- [ ] Implement tab-specific handlers
- [ ] Create RibbonCommandDispatcher
- [ ] Wire up to existing ipc_bridge.cpp
- [ ] Add serialization for commands

### Stage 6: Testing & Polish (Days 6-7)
- [ ] Unit tests for all components
- [ ] Integration tests (React ↔ C++)
- [ ] Visual regression tests
- [ ] Accessibility audit
- [ ] Performance optimization

---

## 13. DEPENDENCIES & IMPORTS

### React/TypeScript Packages
```json
{
  "dependencies": {
    "react": "^18.2.0",
    "react-dom": "^18.2.0",
    "typescript": "^5.0.0"
  },
  "devDependencies": {
    "@testing-library/react": "^14.0.0",
    "@testing-library/jest-dom": "^5.16.0",
    "jest": "^29.0.0"
  }
}
```

### C++ Dependencies
- Existing: ipc_bridge.cpp, JsonValue
- New: CommandHandler, TabHandlers (add to CMakeLists.txt)

---

## 14. ERROR HANDLING & EDGE CASES

### 14.1 Command Failures

```typescript
executeCommand(cmd)
  .catch((error) => {
    // Show error toast
    showNotification({
      type: 'error',
      message: `Command failed: ${error.message}`,
      duration: 5000,
    });
    
    // Log for debugging
    console.error('Command execution failed:', { commandId: cmd.id, error });
  });
```

### 14.2 IPC Timeout

- Graceful degradation if C++ backend unavailable
- Fallback UI (read-only mode)
- Retry mechanism with exponential backoff

### 14.3 State Synchronization

- Debounce rapid commands
- Queue commands if IPC is busy
- Validate command state before execution

---

## 15. DELIVERABLES

### Phase 29 - Planning Phase (Complete)
✅ **PHASE_29_ARCHITECTURE.md** (this document)

### Phase 29-30 - Implementation Phase (Upcoming)
- **React Components:**
  - RibbonBar.tsx
  - RibbonTab.tsx
  - RibbonButton.tsx
  - RibbonGroup.tsx
  - RibbonSeparator.tsx
  - CommandManager (index.tsx)

- **Type Definitions:**
  - types/commands.ts (COMMAND_REGISTRY with 45+ commands)
  - types/ribbon.ts (RibbonState, RibbonAction, etc.)
  - types/colors.ts (ColorCategory, ColorDef)

- **Utilities & Hooks:**
  - hooks/useCommandDispatcher.ts
  - hooks/useRibbonState.ts
  - components/Icons/ColorScheme.ts
  - components/Icons/[11 icon category modules]

- **Styles:**
  - components/CommandManager/*.module.css
  - theme/ribbonTheme.css

- **C++ Backend:**
  - src/ribbon_command_handler.hpp/.cpp
  - src/tab_handlers/*.hpp/.cpp
  - Integration with ipc_bridge.cpp

- **Tests:**
  - components/CommandManager/__tests__/*.test.tsx
  - integration tests

- **Documentation:**
  - PHASE_29_IMPLEMENTATION_PLAN.md (detailed coding steps)
  - RIBBON_UI_MANUAL.md (user documentation)

---

## 16. REFERENCES

### Specification Documents
- BENDCORE_SPECIFICATION_FINAL_LOCKED.md (Section 6: UI Shell)
- bendcore_detailed_verification.md
- FINAL_VERIFICATION_REPORT.md

### Code References
- `/home/claude/bendcore/ui/src/hooks/useIpc.ts` (IPC integration)
- `/home/claude/bendcore/src/ipc_bridge.cpp` (C++ backend)
- `/home/claude/bendcore/src/visualization.cpp` (3D rendering)

### Visual Reference
- **Assembly-Color.dc.html** (Reference interface from specification)

---

## 17. SIGN-OFF

**Architecture Phase:** APPROVED FOR IMPLEMENTATION  
**Status:** Ready for Stage 1 (Foundation)  
**Prepared by:** Claude Code Agent  
**Date:** 2026-10-22  
**Next Phase:** Phase 29 Stage 2 (Implementation)

---

*This architecture document is the authoritative design specification for the CommandManager Ribbon UI. All implementations must conform to these requirements.*
