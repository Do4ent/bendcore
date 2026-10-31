/**
 * Workspace state used by keyboard-driven commands: undo history, the open
 * project (with auto-save), toolbar layout and colour themes.
 *
 * Each class is self-contained and in-memory; KeyboardShortcuts composes them
 * so that shortcuts such as Ctrl+S / Ctrl+Z act on the same state the UI shows.
 */

// ---------------------------------------------------------------------------
// Undo / redo
// ---------------------------------------------------------------------------

export interface RecordedAction {
  type: string;
  data?: unknown;
}

export class ActionHistory {
  private done: RecordedAction[] = [];
  private undone: RecordedAction[] = [];

  constructor(private readonly limit: number = 200) {}

  record(action: RecordedAction): void {
    if (!action || typeof action.type !== 'string' || action.type.length === 0) {
      throw new Error('Action type is required');
    }
    this.done.push({ ...action });
    if (this.done.length > this.limit) {
      this.done.shift();
    }
    // A new action invalidates everything that was undone
    this.undone = [];
  }

  undo(): RecordedAction | undefined {
    const action = this.done.pop();
    if (action) this.undone.push(action);
    return action;
  }

  redo(): RecordedAction | undefined {
    const action = this.undone.pop();
    if (action) this.done.push(action);
    return action;
  }

  canUndo(): boolean {
    return this.done.length > 0;
  }

  canRedo(): boolean {
    return this.undone.length > 0;
  }

  /** Actions that can currently be undone, oldest first. */
  getHistory(): RecordedAction[] {
    return this.done.map((a) => ({ ...a }));
  }

  clear(): void {
    this.done = [];
    this.undone = [];
  }
}

// ---------------------------------------------------------------------------
// Project session with auto-save
// ---------------------------------------------------------------------------

export interface ProjectData {
  id: string;
  name: string;
  [key: string]: unknown;
}

export interface AutoSaveSnapshot {
  project: ProjectData;
  savedAt: number;
}

const cloneProject = (project: ProjectData): ProjectData =>
  JSON.parse(JSON.stringify(project)) as ProjectData;

export class ProjectSession {
  private saved = new Map<string, ProjectData>();
  private autoSaves = new Map<string, AutoSaveSnapshot>();
  private current: ProjectData | null = null;
  private dirty = false;
  private autoSaveTimer: ReturnType<typeof setInterval> | null = null;

  /** Save a project (or the open one). Returns false if there is nothing to save. */
  save(project?: ProjectData): boolean {
    const target = project ?? this.current;
    if (!target || typeof target.id !== 'string' || target.id.length === 0) {
      return false;
    }
    const copy = cloneProject(target);
    this.saved.set(copy.id, copy);
    this.current = cloneProject(copy);
    this.dirty = false;
    if (this.autoSaveTimer) {
      this.writeAutoSave();
    }
    return true;
  }

  /** Open a saved project by id, or open the given project data. */
  load(source: string | ProjectData): ProjectData {
    let project: ProjectData | undefined;
    if (typeof source === 'string') {
      project = this.saved.get(source);
      if (!project) {
        throw new Error(`Project not found: ${source}`);
      }
    } else {
      project = source;
    }
    this.current = cloneProject(project);
    this.dirty = false;
    return cloneProject(this.current);
  }

  getCurrent(): ProjectData | null {
    return this.current ? cloneProject(this.current) : null;
  }

  /** Apply changes to the open project; marks it as having unsaved changes. */
  modify(changes: Partial<ProjectData>): ProjectData {
    if (!this.current) {
      throw new Error('No project is open');
    }
    // The project id identifies the saved copy and cannot be changed here
    this.current = cloneProject({ ...this.current, ...changes, id: this.current.id } as ProjectData);
    this.dirty = true;
    return cloneProject(this.current);
  }

  hasUnsavedChanges(): boolean {
    return this.dirty;
  }

  /** Periodically snapshot the open project; also snapshots on every save. */
  enableAutoSave(intervalMs: number): void {
    if (!(intervalMs > 0)) {
      throw new Error(`Auto-save interval must be positive, got ${intervalMs}`);
    }
    this.disableAutoSave();
    this.autoSaveTimer = setInterval(() => this.writeAutoSave(), intervalMs);
    // Do not keep a Node process (tests, Electron main) alive just for auto-save
    const timer = this.autoSaveTimer as { unref?: () => void };
    timer.unref?.();
  }

  disableAutoSave(): void {
    if (this.autoSaveTimer) {
      clearInterval(this.autoSaveTimer);
      this.autoSaveTimer = null;
    }
  }

  isAutoSaveEnabled(): boolean {
    return this.autoSaveTimer !== null;
  }

  getAutoSave(projectId: string): AutoSaveSnapshot | undefined {
    const snapshot = this.autoSaves.get(projectId);
    return snapshot ? { savedAt: snapshot.savedAt, project: cloneProject(snapshot.project) } : undefined;
  }

  private writeAutoSave(): void {
    if (this.current) {
      this.autoSaves.set(this.current.id, { project: cloneProject(this.current), savedAt: Date.now() });
    }
  }
}

// ---------------------------------------------------------------------------
// Toolbar layout
// ---------------------------------------------------------------------------

export interface ToolbarLayoutPreference {
  buttons: string[];
  size: 'small' | 'medium' | 'large' | string;
}

export const DEFAULT_TOOLBAR_ORDER: readonly string[] = ['sketch', 'dimensions', 'constraints', 'solve'];

export class ToolbarState {
  private order: string[] = [...DEFAULT_TOOLBAR_ORDER];
  private hidden = new Set<string>();
  private layout: ToolbarLayoutPreference = { buttons: [...DEFAULT_TOOLBAR_ORDER], size: 'medium' };

  setOrder(order: string[]): void {
    this.order = Array.from(new Set(order));
  }

  getOrder(): string[] {
    return [...this.order];
  }

  setVisibility(buttonId: string, visible: boolean): void {
    if (visible) {
      this.hidden.delete(buttonId);
    } else {
      this.hidden.add(buttonId);
    }
  }

  isVisible(buttonId: string): boolean {
    return !this.hidden.has(buttonId);
  }

  getHidden(): string[] {
    return Array.from(this.hidden);
  }

  resetDefaults(): void {
    this.order = [...DEFAULT_TOOLBAR_ORDER];
    this.hidden.clear();
    this.layout = { buttons: [...DEFAULT_TOOLBAR_ORDER], size: 'medium' };
  }

  saveLayout(layout: ToolbarLayoutPreference): void {
    this.layout = { ...layout, buttons: [...layout.buttons] };
    this.setOrder(layout.buttons);
  }

  getLayout(): ToolbarLayoutPreference {
    return { ...this.layout, buttons: [...this.layout.buttons] };
  }
}

// ---------------------------------------------------------------------------
// Themes
// ---------------------------------------------------------------------------

export interface ThemeColors {
  background: string;
  grid: string;
  [role: string]: string;
}

export interface ThemeDefinition extends ThemeColors {
  name: string;
}

const BUILT_IN_THEMES: Record<string, ThemeColors> = {
  light: { background: '#F5F5F5', grid: '#D0D0D0', text: '#212121', selection: '#1F6FC5' },
  dark: { background: '#1E1E1E', grid: '#3A3A3A', text: '#E0E0E0', selection: '#4A9BE8' },
};

export class ThemeRegistry {
  private themes = new Map<string, ThemeColors>(
    Object.entries(BUILT_IN_THEMES).map(([name, colors]) => [name, { ...colors }])
  );
  private current = 'light';

  register(theme: ThemeDefinition): void {
    const { name, ...colors } = theme;
    if (!name) {
      throw new Error('Theme name is required');
    }
    if (typeof colors.background !== 'string' || typeof colors.grid !== 'string') {
      throw new Error(`Theme "${name}" must define background and grid colours`);
    }
    this.themes.set(name, { ...colors });
  }

  set(name: string): void {
    if (!this.themes.has(name)) {
      throw new Error(`Unknown theme: ${name}`);
    }
    this.current = name;
  }

  get(): string {
    return this.current;
  }

  getColors(): ThemeColors {
    return { ...(this.themes.get(this.current) as ThemeColors) };
  }

  list(): string[] {
    return Array.from(this.themes.keys());
  }
}
