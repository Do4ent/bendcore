# BendCore Phase 32 - Professional UI Integration Guide

## Overview

Phase 32 completes the professional UI layer of BendCore CAD with:
1. **Context Menus** (ПКМ - Right-click) for all object types
2. **Properties Panel** (Right sidebar) for real-time editing
3. **Status Bar** (Bottom bar) for real-time feedback
4. **Extended Keyboard Shortcuts** (15 new shortcuts, 28 total)

All components are fully integrated with the C++ backend via IPC commands.

## Architecture

```
┌─────────────────────────────────────────────────┐
│              React Frontend (TypeScript)         │
├─────────────────────────────────────────────────┤
│  RibbonBar │ ContextMenu │ PropertiesPanel     │
│  StatusBar │ FeatureTree │ SearchBar            │
└──────────────────┬──────────────────────────────┘
                   │ IPC Bridge
                   ↓
┌─────────────────────────────────────────────────┐
│         Electron IPC (bendcore:command)          │
└──────────────────┬──────────────────────────────┘
                   │
                   ↓
┌─────────────────────────────────────────────────┐
│          C++ Backend (CommandRouter)             │
├─────────────────────────────────────────────────┤
│ ContextManager     │ PropertiesManager         │
│ StatusManager      │ ShortcutManager           │
│ FeatureTree        │ Analysis Modules          │
└─────────────────────────────────────────────────┘
```

## Component Integration

### 1. Context Menus

**Location:** `src/ui/ContextMenu.tsx` (350 lines)

#### Context Menu Types

| Type | Trigger | Actions |
|------|---------|---------|
| **Tube** | Right-click on tube in viewport | Bend Table, Edit, Hide, Isolate, Properties |
| **Vertex** | Right-click on path point | Edit, Delete, Show Dependencies, Properties |
| **Port** | Right-click on port | Edit Parameters, Delete, Hide, Properties |
| **Empty Space** | Right-click in void | New Tube, New Sketch, Paste |
| **Tree Node** | Right-click in Feature Tree | Rename, Edit, Delete, Duplicate, Hide, Properties |
| **Confirmation** | During command | OK, Cancel, Repeat |

#### Usage Example

```typescript
import { ContextMenu, createTubeContextMenu } from './ui/ContextMenu';

const actions = createTubeContextMenu((cmd) => handleCommand(cmd));

<ContextMenu
  isOpen={isOpen}
  position={{ x: 100, y: 100 }}
  actions={actions}
  onClose={() => setIsOpen(false)}
/>
```

#### IPC Commands

```typescript
// Get context menu for object type
bendcore:command → get-context-menu
  args: { objectType: "tube" | "vertex" | "port" | "empty-space" | "tree-node" }
  returns: { menu: ContextAction[] }

// Execute context menu action
bendcore:command → execute-context-action
  args: { actionId: string, objectId?: string }
  returns: { success: boolean }
```

### 2. Properties Panel

**Location:** `src/ui/PropertiesPanel.tsx` (300 lines)

#### Property Types by Object

| Object Type | Property Groups | Examples |
|------------|-----------------|----------|
| **Document** | Document Properties | Name, Standard (GOST/ISO/DIN/ANSI), Description, Author |
| **Tube** | Basic, Dimensions, Bending, State | OD, WT, Material, Bend Radius, Color, Visible |
| **Sketch** | Basic, Constraints | Name, Plane, Fully Constrained, Dimension Count |
| **Port** | Port Properties | Name, Port Type, Visible |
| **Command Params** | Dynamic | Based on active command |

#### Features

- Real-time validation with error messages
- Virtual scrolling for large property lists
- Search/filter by property name or label
- Pin panel to keep it visible
- Undo/redo support for property changes
- Dark mode support
- Responsive layout (collapsible groups)

#### Usage Example

```typescript
import PropertiesPanel from './ui/PropertiesPanel';
import { createTubePropertyGroup } from './ui/PropertyGroups';

const selectedObject = {
  id: 'tube-1',
  type: 'tube',
  name: 'Tube_1',
  groups: createTubePropertyGroup({ od: 12.7, wt: 1.6 }),
};

<PropertiesPanel
  selectedObject={selectedObject}
  onPropertyChange={handlePropertyChange}
  isPinned={true}
/>
```

#### IPC Commands

```typescript
// Get properties for selected object
bendcore:command → get-properties
  args: { objectId: string, objectType: "tube" | "sketch" | "port" }
  returns: { groups: PropertyGroup[] }

// Update a property value
bendcore:command → set-property
  args: { objectId: string, propertyName: string, value: any }
  returns: { success: boolean, newValue: any }
```

### 3. Status Bar

**Location:** `src/ui/StatusBar.tsx` (180 lines)

#### Status Bar Sections (Left to Right)

1. **Step Hint** (250px) - Current operation description
2. **Coordinates** (200px) - Real-time cursor position (X, Y, Z)
3. **Active Plane** (150px) - Current sketch plane (clickable)
4. **Snap Mode** (150px) - Toggle and snap type indicator (clickable)
5. **Units & Standard** (150px) - mm/in and GOST/ISO/DIN/ANSI (clickable)
6. **Violations** (200px, expandable) - Error/Warning/Info counts (clickable)
7. **Recalc Status** (150px) - Calculation spinner or status icon

#### Status Updates

```typescript
interface StatusBarState {
  stepHint?: string;
  cursorX?: number;
  cursorY?: number;
  cursorZ?: number;
  activePlane?: string;
  snapMode?: boolean;
  snapType?: 'grid' | 'endpoint' | 'midpoint' | 'intersection';
  units?: 'mm' | 'in' | 'cm';
  standard?: 'GOST' | 'ISO' | 'DIN' | 'ANSI';
  violations?: { errors: number; warnings: number; info: number };
  isCalculating?: boolean;
}
```

#### Usage Example

```typescript
import StatusBar from './ui/StatusBar';

<StatusBar
  state={{
    stepHint: 'Click point for sketch path',
    cursorX: 10.5,
    cursorY: 20.3,
    cursorZ: 0,
    violations: { errors: 1, warnings: 2, info: 0 },
  }}
  onStatusClick={(section) => console.log(`Clicked: ${section}`)}
/>
```

#### IPC Commands

```typescript
// Get current application status
bendcore:command → get-status
  returns: { status: StatusBarState }

// Listen for status changes (via IPC event)
window.electron.ipc.on('status-updated', (status: StatusBarState) => {})
```

### 4. Extended Keyboard Shortcuts

**Location:** `src/ui/KeyboardShortcuts.ts` (updated with 15 new shortcuts)

#### Shortcut Groups

| Category | Count | Shortcuts |
|----------|-------|-----------|
| File Operations | 7 | Ctrl+N, O, S, Shift+S, P, , (comma), Alt+F4 |
| Sketches | 2 | Ctrl+Shift+K, Ctrl+K |
| Tubes | 2 | Ctrl+T, Ctrl+E |
| Analysis | 3 | Ctrl+G, Ctrl+M, Ctrl+Alt+C |
| Drawing | 1 | Ctrl+D |
| **Editing** | **6** | Ctrl+A, Z, Y, X, C, V |
| **Deletion** | **2** | Delete, F2 |
| **Modes** | **1** | F3 |
| **Navigation** | **2** | Ctrl+F, Ctrl+H |
| **Help** | **2** | F1, Ctrl+? |

**Total: 28 shortcuts**

#### Shortcut Handler Registration

```cpp
// In src/bendcore/shortcuts.cpp::build_default_shortcuts()
shortcuts_.push_back({"Ctrl+A", "select-all", "Select All"});
shortcuts_.push_back({"Ctrl+Z", "undo", "Undo"});
shortcuts_.push_back({"Ctrl+Y", "redo", "Redo"});
// ... and 12 more in Phase 32
```

#### Frontend Shortcut Initialization

```typescript
import { initializeShortcutListeners } from './ui/KeyboardShortcuts';

useEffect(() => {
  initializeShortcutListeners((commandId) => {
    handleCommand(commandId);
  });
}, []);
```

## IPC Command Reference

### Phase 32 New Commands (11 total)

#### Context Menu Commands

```typescript
// 1. Get context menu for object type
POST /ipc: bendcore:command
{
  "command": "get-context-menu",
  "args": {
    "objectType": "tube" | "vertex" | "port" | "empty-space" | "tree-node"
  }
}
Response: {
  "success": true,
  "data": {
    "menu": [
      { "id": "action1", "label": "Action 1", "icon": "📊", "shortcut": "Ctrl+X" }
    ]
  }
}

// 2. Execute context menu action
POST /ipc: bendcore:command
{
  "command": "execute-context-action",
  "args": {
    "actionId": "bend-table",
    "objectId": "tube-1"
  }
}
Response: { "success": true }
```

#### Properties Commands

```typescript
// 3. Get properties for object
POST /ipc: bendcore:command
{
  "command": "get-properties",
  "args": {
    "objectId": "tube-1",
    "objectType": "tube"
  }
}
Response: {
  "success": true,
  "data": {
    "groups": [
      {
        "id": "tube-basic",
        "title": "Basic Properties",
        "properties": [
          {
            "name": "tubeName",
            "label": "Name",
            "type": "text",
            "value": "Tube_1"
          }
        ]
      }
    ]
  }
}

// 4. Set property value
POST /ipc: bendcore:command
{
  "command": "set-property",
  "args": {
    "objectId": "tube-1",
    "propertyName": "tubeName",
    "value": "Tube_Updated"
  }
}
Response: { "success": true, "data": { "newValue": "Tube_Updated" } }
```

#### Status Commands

```typescript
// 5. Get current status
POST /ipc: bendcore:command
{
  "command": "get-status"
}
Response: {
  "success": true,
  "data": {
    "status": {
      "stepHint": "Click point for sketch path",
      "cursorX": 10.5,
      "cursorY": 20.3,
      "cursorZ": 0,
      "snapMode": true,
      "violations": { "errors": 0, "warnings": 1, "info": 0 }
    }
  }
}

// Status updates are pushed via IPC event:
window.electron.ipc.on('status-updated', (status) => {})
```

#### Editing Commands

```typescript
// 6-16. Editing operations
POST /ipc: bendcore:command
{
  "command": "select-all" | "undo" | "redo" | "cut" | "copy" | "paste" | 
             "delete" | "rename" | "toggle-snap" | "find" | "history"
}
```

## File Structure

### React Components (TypeScript)

```
src/ui/
├── ContextMenu.tsx           (350 lines) - Right-click menus
├── ContextMenu.css           (150 lines) - Menu styling
├── PropertiesPanel.tsx       (300 lines) - Properties sidebar
├── PropertyGroups.tsx        (200 lines) - Property builders
├── PropertiesPanel.css       (180 lines) - Panel styling
├── StatusBar.tsx             (180 lines) - Status bar
├── StatusBar.css             (120 lines) - Status bar styling
├── KeyboardShortcuts.ts      (updated)   - 28 shortcuts
└── index.tsx                 (updated)   - Main App layout
```

### C++ Backend

```
src/bendcore/
├── context_manager.hpp       (50 lines)
├── context_manager.cpp       (100 lines)
├── properties_manager.hpp    (60 lines)
├── properties_manager.cpp    (120 lines)
├── status_manager.hpp        (40 lines)
├── status_manager.cpp        (80 lines)
└── shortcuts.cpp             (updated)   - 11 new shortcuts registered
```

### Integration Points

```
src/
├── ipc_commands.hpp          (updated) - 11 new command declarations
├── ipc_commands.cpp          (updated) - 11 new command implementations
└── ipc_bridge.cpp            (existing) - IPC message handling
```

## Usage Scenarios

### Scenario 1: Editing a Tube Property

1. User selects tube in viewport → `PropertiesPanel` shows properties
2. User changes "Outer Diameter" field
3. Component calls `handlePropertyChange('tube-1', 'outerDiameter', 13.5)`
4. Frontend sends IPC command: `set-property`
5. Backend updates the tube object
6. Backend sends `status-updated` event (if changed)
7. StatusBar updates to reflect changes

### Scenario 2: Right-Click Context Menu

1. User right-clicks on tube → viewport mouse handler triggered
2. Generates context position `{ x: 234, y: 567 }`
3. Determines object type from clicked coordinates
4. Calls `get-context-menu` IPC command
5. Backend returns menu actions
6. `ContextMenu` renders with keyboard navigation
7. User selects action → executes associated command

### Scenario 3: Keyboard Shortcut

1. User presses Ctrl+Z (Undo)
2. `initializeShortcutListeners` captures keydown event
3. Matches shortcut to command: "undo"
4. Calls `handleCommand('undo')`
5. Sends IPC command: `bendcore:command` → `undo`
6. Backend executes undo operation
7. StatusBar updates hint to "Undo: [description of undone action]"

## Dark Mode Support

All Phase 32 components support light/dark mode via CSS variables:

```css
:root {
  --context-bg: #ffffff;
  --context-hover: #f0f0f0;
  --panel-bg: #ffffff;
  --status-bg: #f5f5f5;
}

@media (prefers-color-scheme: dark) {
  :root:not([data-theme="light"]) {
    --context-bg: #2a2a2a;
    --context-hover: #3a3a3a;
    --panel-bg: #2a2a2a;
    --status-bg: #2a2a2a;
  }
}
```

## Testing

### Unit Tests

```typescript
// tests/ui/ContextMenu.test.ts
test('ContextMenu renders at correct position', () => {});
test('ContextMenu closes on Escape', () => {});
test('ContextMenu navigates with arrow keys', () => {});

// tests/ui/PropertiesPanel.test.ts
test('PropertiesPanel displays correct properties for object type', () => {});
test('PropertiesPanel validates input', () => {});

// tests/ui/StatusBar.test.ts
test('StatusBar updates cursor position on mouse move', () => {});
test('StatusBar shows violation indicators', () => {});
```

### Integration Tests

```typescript
// tests/integration/Phase32.test.ts
test('Selecting tube shows properties in panel', async () => {
  // 1. Click on tube
  // 2. Verify properties panel appears
  // 3. Verify IPC get-properties was called
});

test('Keyboard shortcut executes correct command', async () => {
  // 1. Press Ctrl+Z
  // 2. Verify undo command was sent
  // 3. Verify status bar updated
});

test('Context menu action executes correctly', async () => {
  // 1. Right-click on tube
  // 2. Select 'Edit Specification'
  // 3. Verify edit command was executed
});
```

## Performance Considerations

### Property Updates
- Debounce property changes (300ms) to reduce backend calls
- Virtual scrolling for large property lists
- Lazy evaluation of expensive validations

### Status Bar
- Throttle cursor position updates to 60fps
- Batch status updates via events instead of individual IPC calls
- Spinner animation optimized with CSS

### Context Menu
- Portal rendering to avoid stacking context issues
- Position adjustments only when menu is visible
- Keyboard navigation with arrow keys (no immediate layout changes)

## Accessibility

All Phase 32 components support:
- Keyboard navigation (all interactions possible without mouse)
- Screen reader compatibility (proper ARIA labels)
- High contrast mode (CSS variables support)
- Reduced motion preferences

## Migration from Phase 31

No breaking changes. Phase 32 adds new features on top of Phase 31:
- RibbonBar remains fully functional
- FeatureTree integration is additive
- KeyboardShortcuts extended (backward compatible)
- All existing commands still work

## Future Extensions (Phase 33+)

Reserved for:
- Custom shortcut configuration UI
- Context menu customization
- Property validation rules engine
- Advanced filtering in properties
- Docking layout manager
- Undo/redo history visualization

## Documentation

- `KEYBOARD_SHORTCUTS.md` - Complete shortcut reference
- `PHASE_32_INTEGRATION.md` - This file
- Inline code comments in all components
- JSDoc for React components
- Doxygen for C++ backend

## Version Information

- **Phase:** 32 (Professional UI)
- **Version:** 1.0
- **Release Date:** 2026-10-23
- **Status:** Production Ready
- **Platforms:** Windows 10+, macOS 11+, Linux (Ubuntu 20.04+)
