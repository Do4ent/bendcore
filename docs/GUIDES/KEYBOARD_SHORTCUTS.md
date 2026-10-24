# BendCore Keyboard Shortcuts Reference

Complete keyboard shortcut mapping for BendCore CAD system. All 28 shortcuts across all categories (Phase 32).

## File Operations (5 shortcuts)

| Shortcut | Command | Description |
|----------|---------|-------------|
| **Ctrl+N** | New Document | Create a new BendCore document |
| **Ctrl+O** | Open | Open an existing document |
| **Ctrl+S** | Save | Save the current document |
| **Ctrl+Shift+S** | Save As | Save with a new filename |
| **Ctrl+P** | Print | Print the document |

## Part Design - Sketches (2 shortcuts)

| Shortcut | Command | Description |
|----------|---------|-------------|
| **Ctrl+Shift+K** | New Sketch | Create a new 2D sketch |
| **Ctrl+K** | Edit Sketch | Edit an existing sketch |

## Part Design - Tubes (2 shortcuts)

| Shortcut | Command | Description |
|----------|---------|-------------|
| **Ctrl+T** | New Tube | Create a new tube element |
| **Ctrl+E** | Edit Tube | Edit an existing tube |

## Analysis (3 shortcuts)

| Shortcut | Command | Description |
|----------|---------|-------------|
| **Ctrl+G** | Check Gaps | Check gap requirements between bends |
| **Ctrl+M** | Calculate Mass | Calculate component mass and properties |
| **Ctrl+Alt+C** | Check Collisions | Check for geometry collisions |

## Drawing (1 shortcut)

| Shortcut | Command | Description |
|----------|---------|-------------|
| **Ctrl+D** | New Sheet | Create a new drawing sheet |

## Editing & Selection (6 shortcuts) - Phase 32

| Shortcut | Command | Description |
|----------|---------|-------------|
| **Ctrl+A** | Select All | Select all objects in the document |
| **Ctrl+Z** | Undo | Undo the last action |
| **Ctrl+Y** | Redo | Redo the last undone action |
| **Ctrl+X** | Cut | Cut selected objects to clipboard |
| **Ctrl+C** | Copy | Copy selected objects to clipboard |
| **Ctrl+V** | Paste | Paste objects from clipboard |

## Deletion & Renaming (2 shortcuts) - Phase 32

| Shortcut | Command | Description |
|----------|---------|-------------|
| **Delete** | Delete | Delete selected objects |
| **F2** | Rename | Rename selected object |

## Modes & Toggles (1 shortcut) - Phase 32

| Shortcut | Command | Description |
|----------|---------|-------------|
| **F3** | Toggle Snap | Toggle snap mode on/off |

## Navigation & Search (2 shortcuts) - Phase 32

| Shortcut | Command | Description |
|----------|---------|-------------|
| **Ctrl+F** | Find | Open find/search dialog |
| **Ctrl+H** | History | Show undo/redo history panel |

## File Operations - Extended (2 shortcuts) - Phase 32

| Shortcut | Command | Description |
|----------|---------|-------------|
| **Ctrl+,** | Settings | Open settings/preferences |
| **Alt+F4** | Exit | Exit application (Windows/Linux) |

## Help (2 shortcuts) - Phase 32

| Shortcut | Command | Description |
|----------|---------|-------------|
| **F1** | Help | Open help documentation |
| **Ctrl+?** | Keyboard Help | Show keyboard shortcuts help dialog |

**Note:** On macOS, `Ctrl` is automatically replaced with `Cmd` for all shortcuts.

## Platform Support

All shortcuts are designed to work on:
- Windows (7 and later)
- macOS (10.13 and later)
- Linux (GTK+ based systems)

Keyboard layout support:
- QWERTY
- QWERTY-Mac
- QWERTZ
- AZERTY
- Dvorak

## Conflict Resolution

If a shortcut conflicts with system shortcuts:
1. Use the Command Search (Ctrl+K) instead
2. Access the command through the Ribbon UI
3. Configure custom shortcuts in Settings → Keyboard

## Summary by Category

### Phase 31 (Original - 13 shortcuts)
- File Operations: 5
- Sketches: 2
- Tubes: 2
- Analysis: 3
- Drawing: 1

### Phase 32 (Additions - 15 shortcuts)
- Editing & Selection: 6
- Deletion & Renaming: 2
- Modes & Toggles: 1
- Navigation & Search: 2
- File Operations - Extended: 2
- Help: 2

**Total: 28 shortcuts**

## Future Shortcuts (Phase 33+)

The following shortcuts are reserved for future implementation:
- **Ctrl+Shift+P** - Print Preview
- **F5** - Start Simulation
- **F6** - Show Analysis Results
- **F7** - Show Constraints
- **Ctrl+Shift+M** - Toggle Measurement Mode

## Customization

In Phase 32, users will be able to:
1. View all shortcuts in Settings → Keyboard
2. Reassign shortcuts to different commands
3. Create custom shortcut profiles
4. Export/import shortcut configurations
5. Reset to default shortcuts

## Implementation Details

### C++ Backend (`src/bendcore/shortcuts.cpp`)
- `ShortcutManager::get_command()` - Maps shortcut string to command ID
- `ShortcutManager::register_shortcut()` - Register new shortcuts
- `ShortcutManager::get_shortcuts_for_command()` - Get shortcuts for a command

### React Frontend (`src/ui/KeyboardShortcuts.ts`)
- `initializeShortcutListeners()` - Initialize global keyboard listener
- `getShortcutForCommand()` - Get display shortcut for command
- `formatShortcutForDisplay()` - Format for UI (Cmd on macOS)

### Electron Main Process
- Shortcuts are handled at the window level
- Electron preload exposes IPC channel for command execution
- No OS-level global shortcuts (Phase 32)

## Testing

All shortcuts are tested with:
1. Unit tests in `__tests__/ui/KeyboardShortcuts.test.ts`
2. Integration tests with Electron main process
3. Platform-specific tests (Windows, macOS, Linux)
4. Keyboard layout tests (QWERTY, QWERTZ, AZERTY)

## Support

For issues or questions about keyboard shortcuts:
1. Check this documentation
2. Run keyboard layout diagnostics in Settings
3. Review logs: `~/.bendcore/logs/shortcuts.log`
4. Report issues at https://github.com/Do4ent/bendcore/issues
