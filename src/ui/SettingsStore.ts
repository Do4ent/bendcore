/**
 * BendCore Settings Store
 * Persistent storage for UI settings, toolbar layout, themes, and profiles
 * Stores in ~/.bendcore/settings.json
 */

export interface ToolbarButton {
  id: string;
  label: string;
  category: string;
  visible: boolean;
}

export interface ToolbarLayout {
  buttons: string[]; // Array of button IDs in order
  homeTabButtons: string[];
  assemblyTabButtons: string[];
  drawingTabButtons: string[];
}

export interface IconTheme {
  name: string;
  preset: "light" | "dark" | "highContrast" | "colorblind" | "custom";
  colors: Record<string, string>; // category -> hex color
}

export interface UIProfile {
  name: string;
  isDefault: boolean;
  toolbar: ToolbarLayout;
  theme: IconTheme;
  windowState: {
    width: number;
    height: number;
    x: number;
    y: number;
  };
  shortcuts: Record<string, string>; // commandId -> keybinding
  panelLayout: {
    featureTreeWidth: number;
    propertiesPanelWidth: number;
    statusBarHeight: number;
  };
}

export interface WorkspaceProfile {
  name: string;
  projectPath: string;
  lastModified: number; // timestamp
  settings: Partial<UIProfile>;
}

export interface SettingsData {
  version: string;
  currentProfile: string;
  currentWorkspaceProfile: string;
  profiles: Record<string, UIProfile>;
  workspaceProfiles: Record<string, WorkspaceProfile>;
  theme: IconTheme;
  toolbar: ToolbarLayout;
}

const DEFAULT_COLORS: Record<string, string> = {
  tube: "#1F6FC5",
  create: "#2E9E5B",
  delete: "#D93A3A",
  import: "#0E97A3",
  links: "#8A4FD8",
  edit: "#E8820C",
  update: "#F5B517",
  freeze: "#2D9CDB",
  auxiliary: "#C9920A",
  axes_x: "#FF0000",
  axes_y: "#00FF00",
  axes_z: "#0000FF",
  neutral: "#757575",
};

const DEFAULT_LIGHT_THEME: IconTheme = {
  name: "Light",
  preset: "light",
  colors: { ...DEFAULT_COLORS },
};

const DEFAULT_DARK_THEME: IconTheme = {
  name: "Dark",
  preset: "dark",
  colors: {
    ...DEFAULT_COLORS,
    neutral: "#A0A0A0", // Lighter neutral for dark theme
  },
};

const DEFAULT_HIGH_CONTRAST_THEME: IconTheme = {
  name: "High Contrast",
  preset: "highContrast",
  colors: {
    tube: "#0033FF",
    create: "#00CC00",
    delete: "#FF0000",
    import: "#00CCCC",
    links: "#FF00FF",
    edit: "#FF6600",
    update: "#FFFF00",
    freeze: "#0088FF",
    auxiliary: "#FF8800",
    axes_x: "#FF0000",
    axes_y: "#00FF00",
    axes_z: "#0000FF",
    neutral: "#000000",
  },
};

// Colorblind-safe palette (Deuteranopia)
const DEFAULT_COLORBLIND_THEME: IconTheme = {
  name: "Colorblind Safe",
  preset: "colorblind",
  colors: {
    tube: "#0173B2",
    create: "#DE8F05",
    delete: "#CC78BC",
    import: "#CA9161",
    links: "#029E73",
    edit: "#ECE133",
    update: "#56B4E9",
    freeze: "#009E73",
    auxiliary: "#F0E442",
    axes_x: "#E69F00",
    axes_y: "#56B4E9",
    axes_z: "#0173B2",
    neutral: "#999999",
  },
};

const DEFAULT_TOOLBAR_LAYOUT: ToolbarLayout = {
  buttons: [
    "new-document",
    "open-document",
    "save-document",
    "separator",
    "undo",
    "redo",
    "separator",
    "new-tube",
    "new-sketch",
    "new-component",
    "separator",
    "check-collisions",
    "check-gaps",
  ],
  homeTabButtons: [
    "new-document",
    "open-document",
    "save-document",
    "print-document",
  ],
  assemblyTabButtons: [
    "insert-component",
    "new-port",
    "trace-tube",
    "add-support",
  ],
  drawingTabButtons: ["new-sheet", "create-view", "insert-bend-table"],
};

const DEFAULT_UI_PROFILE: UIProfile = {
  name: "Default",
  isDefault: true,
  toolbar: DEFAULT_TOOLBAR_LAYOUT,
  theme: DEFAULT_LIGHT_THEME,
  windowState: {
    width: 1400,
    height: 900,
    x: 0,
    y: 0,
  },
  shortcuts: {},
  panelLayout: {
    featureTreeWidth: 250,
    propertiesPanelWidth: 300,
    statusBarHeight: 24,
  },
};

const DEFAULT_SETTINGS: SettingsData = {
  version: "1.0.0",
  currentProfile: "Default",
  currentWorkspaceProfile: "",
  profiles: {
    Default: DEFAULT_UI_PROFILE,
  },
  workspaceProfiles: {},
  theme: DEFAULT_LIGHT_THEME,
  toolbar: DEFAULT_TOOLBAR_LAYOUT,
};

/**
 * SettingsStore: Manages persistent application settings
 * Uses localStorage for web, electron file system for desktop
 */
export class SettingsStore {
  private settings: SettingsData;
  private isDirty: boolean = false;
  private saveTimer: NodeJS.Timeout | null = null;
  private readonly SAVE_DEBOUNCE_MS = 1000;
  private readonly STORAGE_KEY = "bendcore-settings";

  constructor() {
    this.settings = this.loadSettings();
  }

  /**
   * Load settings from storage or create defaults
   */
  private loadSettings(): SettingsData {
    try {
      const stored = localStorage.getItem(this.STORAGE_KEY);
      if (stored) {
        const parsed = JSON.parse(stored);
        return this.migrateSettings(parsed);
      }
    } catch (error) {
      console.error("Error loading settings:", error);
    }
    return JSON.parse(JSON.stringify(DEFAULT_SETTINGS));
  }

  /**
   * Migrate settings from older versions
   */
  private migrateSettings(settings: any): SettingsData {
    // Handle version upgrades
    if (!settings.version) {
      settings.version = "1.0.0";
    }

    if (settings.version === "1.0.0") {
      // Current version, no migration needed
    }

    // Ensure required fields exist
    settings.currentProfile = settings.currentProfile || "Default";
    settings.profiles = settings.profiles || { Default: DEFAULT_UI_PROFILE };
    settings.theme = settings.theme || DEFAULT_LIGHT_THEME;
    settings.toolbar = settings.toolbar || DEFAULT_TOOLBAR_LAYOUT;

    return settings as SettingsData;
  }

  /**
   * Save settings to storage (debounced)
   */
  private saveSettingsImmediate(): void {
    try {
      localStorage.setItem(this.STORAGE_KEY, JSON.stringify(this.settings));
      this.isDirty = false;
    } catch (error) {
      console.error("Error saving settings:", error);
    }
  }

  /**
   * Queue settings save with debounce
   */
  private queueSave(): void {
    this.isDirty = true;
    if (this.saveTimer) {
      clearTimeout(this.saveTimer);
    }
    this.saveTimer = setTimeout(
      () => this.saveSettingsImmediate(),
      this.SAVE_DEBOUNCE_MS
    );
  }

  /**
   * Force immediate save
   */
  public save(): void {
    if (this.saveTimer) {
      clearTimeout(this.saveTimer);
    }
    this.saveSettingsImmediate();
  }

  /**
   * Get current settings
   */
  public getSettings(): SettingsData {
    return JSON.parse(JSON.stringify(this.settings));
  }

  /**
   * Get current toolbar layout
   */
  public getToolbarLayout(): ToolbarLayout {
    return JSON.parse(JSON.stringify(this.settings.toolbar));
  }

  /**
   * Set toolbar layout
   */
  public setToolbarLayout(layout: ToolbarLayout): void {
    this.settings.toolbar = JSON.parse(JSON.stringify(layout));
    this.queueSave();
  }

  /**
   * Get current icon theme
   */
  public getTheme(): IconTheme {
    return JSON.parse(JSON.stringify(this.settings.theme));
  }

  /**
   * Set icon theme
   */
  public setTheme(theme: IconTheme): void {
    this.settings.theme = JSON.parse(JSON.stringify(theme));
    this.queueSave();
  }

  /**
   * Get theme by name or preset
   */
  public getThemeByPreset(
    preset: "light" | "dark" | "highContrast" | "colorblind"
  ): IconTheme {
    switch (preset) {
      case "light":
        return JSON.parse(JSON.stringify(DEFAULT_LIGHT_THEME));
      case "dark":
        return JSON.parse(JSON.stringify(DEFAULT_DARK_THEME));
      case "highContrast":
        return JSON.parse(JSON.stringify(DEFAULT_HIGH_CONTRAST_THEME));
      case "colorblind":
        return JSON.parse(JSON.stringify(DEFAULT_COLORBLIND_THEME));
      default:
        return JSON.parse(JSON.stringify(DEFAULT_LIGHT_THEME));
    }
  }

  /**
   * Get a specific UI profile
   */
  public getProfile(name: string): UIProfile | undefined {
    return this.settings.profiles[name]
      ? JSON.parse(JSON.stringify(this.settings.profiles[name]))
      : undefined;
  }

  /**
   * Get current UI profile
   */
  public getCurrentProfile(): UIProfile {
    const profile = this.settings.profiles[this.settings.currentProfile];
    return profile
      ? JSON.parse(JSON.stringify(profile))
      : JSON.parse(JSON.stringify(DEFAULT_UI_PROFILE));
  }

  /**
   * Save a UI profile
   */
  public saveProfile(profile: UIProfile): void {
    this.settings.profiles[profile.name] = JSON.parse(JSON.stringify(profile));
    this.queueSave();
  }

  /**
   * Delete a UI profile
   */
  public deleteProfile(name: string): void {
    if (name === "Default") {
      throw new Error("Cannot delete default profile");
    }
    delete this.settings.profiles[name];
    if (this.settings.currentProfile === name) {
      this.settings.currentProfile = "Default";
    }
    this.queueSave();
  }

  /**
   * Set current profile
   */
  public setCurrentProfile(name: string): void {
    if (this.settings.profiles[name]) {
      this.settings.currentProfile = name;
      this.queueSave();
    }
  }

  /**
   * Get all profile names
   */
  public getProfileNames(): string[] {
    return Object.keys(this.settings.profiles);
  }

  /**
   * Get a workspace profile
   */
  public getWorkspaceProfile(name: string): WorkspaceProfile | undefined {
    return this.settings.workspaceProfiles[name]
      ? JSON.parse(JSON.stringify(this.settings.workspaceProfiles[name]))
      : undefined;
  }

  /**
   * Save a workspace profile
   */
  public saveWorkspaceProfile(profile: WorkspaceProfile): void {
    this.settings.workspaceProfiles[profile.name] = JSON.parse(
      JSON.stringify(profile)
    );
    this.queueSave();
  }

  /**
   * Delete a workspace profile
   */
  public deleteWorkspaceProfile(name: string): void {
    delete this.settings.workspaceProfiles[name];
    this.queueSave();
  }

  /**
   * Export settings as JSON
   */
  public exportSettings(): string {
    return JSON.stringify(this.settings, null, 2);
  }

  /**
   * Import settings from JSON
   */
  public importSettings(json: string): void {
    try {
      const imported = JSON.parse(json);
      this.settings = this.migrateSettings(imported);
      this.saveSettingsImmediate();
    } catch (error) {
      throw new Error(`Failed to import settings: ${error}`);
    }
  }

  /**
   * Reset to default settings
   */
  public resetToDefaults(): void {
    this.settings = JSON.parse(JSON.stringify(DEFAULT_SETTINGS));
    this.saveSettingsImmediate();
  }

  /**
   * Get all themes
   */
  public getAllThemes(): IconTheme[] {
    return [
      DEFAULT_LIGHT_THEME,
      DEFAULT_DARK_THEME,
      DEFAULT_HIGH_CONTRAST_THEME,
      DEFAULT_COLORBLIND_THEME,
      this.settings.theme.preset === "custom" ? this.settings.theme : null,
    ].filter((t): t is IconTheme => t !== null);
  }

  /**
   * Get color for category
   */
  public getColorForCategory(category: string): string {
    return this.settings.theme.colors[category] || DEFAULT_COLORS.neutral;
  }

  /**
   * Cleanup
   */
  public destroy(): void {
    if (this.saveTimer) {
      clearTimeout(this.saveTimer);
      this.saveSettingsImmediate();
    }
  }
}

// Global singleton instance
export const settingsStore = new SettingsStore();
