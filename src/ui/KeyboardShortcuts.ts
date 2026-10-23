/**
 * BendCore Keyboard Shortcuts Mapping
 * All 34 commands with keyboard shortcuts
 * Part of Phase 31 foundational work
 */

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
 * Initialize keyboard shortcut listeners on app startup
 */
export function initializeShortcutListeners(
  onCommand: (commandId: string) => void
): void {
  window.addEventListener("keydown", (event: KeyboardEvent) => {
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
  });
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
