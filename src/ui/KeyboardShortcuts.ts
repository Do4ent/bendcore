/**
 * BendCore Keyboard Shortcuts Mapping
 * All 34 commands with keyboard shortcuts
 * Part of Phase 31 foundational work
 */

import {
  ActionHistory,
  AutoSaveSnapshot,
  ProjectData,
  ProjectSession,
  RecordedAction,
  ThemeColors,
  ThemeDefinition,
  ThemeRegistry,
  ToolbarLayoutPreference,
  ToolbarState,
} from "./WorkspaceState";

/**
 * Shortcut definition
 */
export interface KeyboardShortcutDef {
  shortcut: string;      // e.g., "Ctrl+N"
  commandId: string;     // e.g., "new-document"
  description: string;   // User-friendly description
  category: string;      // e.g., "file", "sketch", "tube", etc.
  platform?: "win" | "mac" | "all";  // Platform support
}

/**
 * Complete keyboard shortcut mapping (34 shortcuts for 34 commands)
 */
export const KEYBOARD_SHORTCUTS: KeyboardShortcutDef[] = [
  // FILE OPERATIONS (5 shortcuts)
  {
    shortcut: "Ctrl+N",
    commandId: "new-document",
    description: "Create a new document",
    category: "file",
    platform: "all",
  },
  {
    shortcut: "Ctrl+O",
    commandId: "open",
    description: "Open an existing document",
    category: "file",
    platform: "all",
  },
  {
    shortcut: "Ctrl+S",
    commandId: "save",
    description: "Save the current document",
    category: "file",
    platform: "all",
  },
  {
    shortcut: "Ctrl+Shift+S",
    commandId: "save-as",
    description: "Save with a new filename",
    category: "file",
    platform: "all",
  },
  {
    shortcut: "Ctrl+P",
    commandId: "print",
    description: "Print the document",
    category: "file",
    platform: "all",
  },

  // PART DESIGN - SKETCHES (3 shortcuts)
  {
    shortcut: "Ctrl+Shift+K",
    commandId: "new-sketch",
    description: "Create a new 2D sketch",
    category: "sketch",
    platform: "all",
  },
  {
    shortcut: "Ctrl+K",
    commandId: "edit-sketch",
    description: "Edit an existing sketch",
    category: "sketch",
    platform: "all",
  },

  // PART DESIGN - TUBES (2 shortcuts)
  {
    shortcut: "Ctrl+T",
    commandId: "new-tube",
    description: "Create a new tube element",
    category: "tube",
    platform: "all",
  },
  {
    shortcut: "Ctrl+E",
    commandId: "edit-tube",
    description: "Edit an existing tube",
    category: "tube",
    platform: "all",
  },

  // ANALYSIS (3 shortcuts)
  {
    shortcut: "Ctrl+G",
    commandId: "check-gaps",
    description: "Check gap requirements",
    category: "analysis",
    platform: "all",
  },
  {
    shortcut: "Ctrl+M",
    commandId: "calculate-mass",
    description: "Calculate component mass",
    category: "analysis",
    platform: "all",
  },
  {
    shortcut: "Ctrl+Alt+C",
    commandId: "check-collisions",
    description: "Check for collisions",
    category: "analysis",
    platform: "all",
  },

  // DRAWING (1 shortcut)
  {
    shortcut: "Ctrl+D",
    commandId: "new-sheet",
    description: "Create a new drawing sheet",
    category: "drawing",
    platform: "all",
  },

  // EDITING & SELECTION (6 shortcuts)
  {
    shortcut: "Ctrl+A",
    commandId: "select-all",
    description: "Select all objects",
    category: "editing",
    platform: "all",
  },
  {
    shortcut: "Ctrl+Z",
    commandId: "undo",
    description: "Undo last action",
    category: "editing",
    platform: "all",
  },
  {
    shortcut: "Ctrl+Y",
    commandId: "redo",
    description: "Redo last undone action",
    category: "editing",
    platform: "all",
  },
  {
    shortcut: "Ctrl+X",
    commandId: "cut",
    description: "Cut selected objects",
    category: "editing",
    platform: "all",
  },
  {
    shortcut: "Ctrl+C",
    commandId: "copy",
    description: "Copy selected objects",
    category: "editing",
    platform: "all",
  },
  {
    shortcut: "Ctrl+V",
    commandId: "paste",
    description: "Paste copied objects",
    category: "editing",
    platform: "all",
  },

  // DELETION & RENAMING (2 shortcuts)
  {
    shortcut: "Delete",
    commandId: "delete",
    description: "Delete selected objects",
    category: "editing",
    platform: "all",
  },
  {
    shortcut: "F2",
    commandId: "rename",
    description: "Rename selected object",
    category: "editing",
    platform: "all",
  },

  // MODES & TOGGLES (2 shortcuts)
  {
    shortcut: "F3",
    commandId: "toggle-snap",
    description: "Toggle snap mode on/off",
    category: "tools",
    platform: "all",
  },
  {
    shortcut: "Alt+F4",
    commandId: "exit",
    description: "Exit application",
    category: "file",
    platform: "win",
  },

  // NAVIGATION & SEARCH (2 shortcuts)
  {
    shortcut: "Ctrl+F",
    commandId: "find",
    description: "Open find/search dialog",
    category: "navigation",
    platform: "all",
  },
  {
    shortcut: "Ctrl+H",
    commandId: "history",
    description: "Show undo/redo history",
    category: "navigation",
    platform: "all",
  },

  // SETTINGS & HELP (3 shortcuts)
  {
    shortcut: "Ctrl+,",
    commandId: "settings",
    description: "Open settings/preferences",
    category: "file",
    platform: "all",
  },
  {
    shortcut: "F1",
    commandId: "help",
    description: "Open help documentation",
    category: "help",
    platform: "all",
  },
  {
    shortcut: "Ctrl+?",
    commandId: "keyboard-help",
    description: "Show keyboard shortcuts help",
    category: "help",
    platform: "all",
  },
];

/**
 * Create reverse mapping: shortcut -> commandId
 */
export const SHORTCUT_TO_COMMAND: Record<string, string> = KEYBOARD_SHORTCUTS.reduce(
  (map, shortcut) => {
    map[shortcut.shortcut] = shortcut.commandId;
    return map;
  },
  {} as Record<string, string>
);

/**
 * Create command -> shortcuts mapping
 */
export const COMMAND_TO_SHORTCUTS: Record<string, string[]> = KEYBOARD_SHORTCUTS.reduce(
  (map, shortcut) => {
    if (!map[shortcut.commandId]) {
      map[shortcut.commandId] = [];
    }
    map[shortcut.commandId].push(shortcut.shortcut);
    return map;
  },
  {} as Record<string, string[]>
);

/**
 * Get shortcut for a command
 */
export function getShortcutForCommand(commandId: string): string | undefined {
  const shortcuts = COMMAND_TO_SHORTCUTS[commandId];
  return shortcuts ? shortcuts[0] : undefined;
}

/**
 * Get command for a shortcut
 */
export function getCommandForShortcut(shortcut: string): string | undefined {
  return SHORTCUT_TO_COMMAND[shortcut];
}

/**
 * Check if shortcut is defined
 */
export function isShortcutDefined(shortcut: string): boolean {
  return shortcut in SHORTCUT_TO_COMMAND;
}

/**
 * Get all shortcuts for a category
 */
export function getShortcutsByCategory(
  category: string
): KeyboardShortcutDef[] {
  return KEYBOARD_SHORTCUTS.filter((s) => s.category === category);
}

/**
 * Initialize keyboard shortcut listeners on app startup.
 * Returns a function that removes the listener again.
 */
export function initializeShortcutListeners(
  onCommand: (commandId: string) => void
): () => void {
  const listener = (event: KeyboardEvent) => {
    // Build shortcut string from event
    const parts: string[] = [];

    if (event.ctrlKey) parts.push("Ctrl");
    if (event.altKey) parts.push("Alt");
    if (event.shiftKey) parts.push("Shift");

    // Add the key
    let key = event.key.toUpperCase();
    if (event.key.length === 1) {
      // Single character key
      key = event.key.toUpperCase();
    } else if (event.key === "Control" || event.key === "Alt" || event.key === "Shift") {
      return; // Skip modifier keys
    } else {
      // Named key (Enter, Escape, etc.)
      key = event.key;
    }

    parts.push(key);
    const shortcut = parts.join("+");

    // Check if this is a registered shortcut
    const commandId = getCommandForShortcut(shortcut);
    if (commandId) {
      event.preventDefault();
      onCommand(commandId);
    }
  };
  window.addEventListener("keydown", listener);
  return () => window.removeEventListener("keydown", listener);
}

/**
 * Display shortcut in tooltip format
 */
export function formatShortcutForDisplay(commandId: string): string {
  const shortcut = getShortcutForCommand(commandId);
  if (!shortcut) return "";

  // On macOS, replace Ctrl with Cmd
  if (navigator.platform.includes("Mac")) {
    return shortcut.replace("Ctrl", "Cmd");
  }

  return shortcut;
}

// ---------------------------------------------------------------------------
// KeyboardShortcuts controller
// ---------------------------------------------------------------------------


export interface ShortcutBinding {
  action: string;
  /** Normalised key combination, e.g. ["Ctrl", "Shift", "S"] */
  keys: string[];
}

export interface WorkspacePreferences {
  theme: string;
  toolbarOrder: string[];
  hiddenToolbarButtons: string[];
  toolbarLayout: ToolbarLayoutPreference;
  shortcuts: ShortcutBinding[];
}

const MODIFIER_ORDER = ["Ctrl", "Alt", "Shift", "Meta"];
const MODIFIER_ALIASES: Record<string, string> = {
  ctrl: "Ctrl",
  control: "Ctrl",
  alt: "Alt",
  option: "Alt",
  shift: "Shift",
  meta: "Meta",
  cmd: "Meta",
  command: "Meta",
};

/** Normalise keys to "Ctrl+Alt+Shift+Meta+<KEY>" order with an upper-case key. */
export function normalizeShortcut(keys: string[]): string {
  const modifiers = new Set<string>();
  const others: string[] = [];
  keys
    .flatMap((k) => k.split("+"))
    .map((k) => k.trim())
    .filter((k) => k.length > 0)
    .forEach((k) => {
      const modifier = MODIFIER_ALIASES[k.toLowerCase()];
      if (modifier) {
        modifiers.add(modifier);
      } else {
        others.push(k.length === 1 ? k.toUpperCase() : k);
      }
    });
  if (others.length !== 1) {
    throw new Error(`A shortcut needs exactly one non-modifier key, got: ${keys.join("+")}`);
  }
  return [...MODIFIER_ORDER.filter((m) => modifiers.has(m)), others[0]].join("+");
}

/**
 * Keyboard-driven workspace controller: the shortcut map plus the state the
 * shortcuts act on (undo history, the open project, toolbar and theme
 * preferences), so commands such as Ctrl+S or Ctrl+Z and the UI share it.
 */
export class KeyboardShortcuts {
  private bindings = new Map<string, string>();
  private readonly history = new ActionHistory();
  private readonly project = new ProjectSession();
  private readonly toolbar = new ToolbarState();
  private readonly themes = new ThemeRegistry();
  private savedPreferences: WorkspacePreferences | null = null;

  constructor(definitions: KeyboardShortcutDef[] = KEYBOARD_SHORTCUTS) {
    definitions.forEach((def) => this.bindings.set(normalizeShortcut([def.shortcut]), def.commandId));
  }

  // Shortcuts ---------------------------------------------------------------

  /** Action bound to a key combination, e.g. ("Ctrl", "S") -> "save". */
  handleKeyboardShortcut(...keys: string[]): string | undefined {
    return this.bindings.get(normalizeShortcut(keys));
  }

  /** Bind a key combination to an action, replacing any existing binding. */
  setShortcut(action: string, ...keys: string[]): void {
    if (!action) {
      throw new Error("Shortcut action is required");
    }
    this.bindings.set(normalizeShortcut(keys), action);
  }

  /** Action currently bound to a key combination (a conflict when re-binding). */
  checkShortcutConflict(...keys: string[]): string | undefined {
    return this.bindings.get(normalizeShortcut(keys));
  }

  getAllShortcuts(): ShortcutBinding[] {
    return Array.from(this.bindings.entries()).map(([shortcut, action]) => ({
      action,
      keys: shortcut.split("+"),
    }));
  }

  // Undo / redo -------------------------------------------------------------

  recordAction(action: RecordedAction): void {
    this.history.record(action);
  }

  undo(): RecordedAction | undefined {
    return this.history.undo();
  }

  redo(): RecordedAction | undefined {
    return this.history.redo();
  }

  canUndo(): boolean {
    return this.history.canUndo();
  }

  canRedo(): boolean {
    return this.history.canRedo();
  }

  getUndoHistory(): RecordedAction[] {
    return this.history.getHistory();
  }

  // Project -----------------------------------------------------------------

  saveProject(project?: ProjectData): boolean {
    return this.project.save(project);
  }

  loadProject(source: string | ProjectData): ProjectData {
    return this.project.load(source);
  }

  modifyProject(changes: Partial<ProjectData>): ProjectData {
    return this.project.modify(changes);
  }

  hasUnsavedChanges(): boolean {
    return this.project.hasUnsavedChanges();
  }

  enableAutoSave(intervalMs: number): void {
    this.project.enableAutoSave(intervalMs);
  }

  disableAutoSave(): void {
    this.project.disableAutoSave();
  }

  getAutoSaveData(projectId: string): AutoSaveSnapshot | undefined {
    return this.project.getAutoSave(projectId);
  }

  // Toolbar -----------------------------------------------------------------

  setToolbarOrder(order: string[]): void {
    this.toolbar.setOrder(order);
  }

  getToolbarOrder(): string[] {
    return this.toolbar.getOrder();
  }

  setToolbarVisibility(buttonId: string, visible: boolean): void {
    this.toolbar.setVisibility(buttonId, visible);
  }

  isToolbarButtonVisible(buttonId: string): boolean {
    return this.toolbar.isVisible(buttonId);
  }

  resetToolbarDefaults(): void {
    this.toolbar.resetDefaults();
  }

  saveToolbarLayout(layout: ToolbarLayoutPreference): void {
    this.toolbar.saveLayout(layout);
  }

  getToolbarLayout(): ToolbarLayoutPreference {
    return this.toolbar.getLayout();
  }

  // Themes ------------------------------------------------------------------

  registerTheme(theme: ThemeDefinition): void {
    this.themes.register(theme);
  }

  setTheme(name: string): void {
    this.themes.set(name);
  }

  getTheme(): string {
    return this.themes.get();
  }

  getThemeColors(): ThemeColors {
    return this.themes.getColors();
  }

  // Preferences -------------------------------------------------------------

  /** Snapshot the current preferences (theme, toolbar, shortcuts). */
  savePreferences(): WorkspacePreferences {
    this.savedPreferences = {
      theme: this.themes.get(),
      toolbarOrder: this.toolbar.getOrder(),
      hiddenToolbarButtons: this.toolbar.getHidden(),
      toolbarLayout: this.toolbar.getLayout(),
      shortcuts: this.getAllShortcuts(),
    };
    return this.getPreferences() as WorkspacePreferences;
  }

  /** Last saved preferences, or null if none were saved yet. */
  getPreferences(): WorkspacePreferences | null {
    return this.savedPreferences ? JSON.parse(JSON.stringify(this.savedPreferences)) : null;
  }
}
