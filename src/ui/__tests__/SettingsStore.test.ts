/**
 * BendCore Settings Store Tests
 */

import { SettingsStore, settingsStore } from "../SettingsStore";

// Mock localStorage
const localStorageMock = (() => {
  let store: Record<string, string> = {};

  return {
    getItem: (key: string) => store[key] || null,
    setItem: (key: string, value: string) => {
      store[key] = value.toString();
    },
    removeItem: (key: string) => {
      delete store[key];
    },
    clear: () => {
      store = {};
    },
  };
})();

Object.defineProperty(window, "localStorage", {
  value: localStorageMock,
});

describe("SettingsStore", () => {
  let store: SettingsStore;

  beforeEach(() => {
    localStorage.clear();
    store = new SettingsStore();
  });

  afterEach(() => {
    store.destroy();
  });

  describe("Initialization", () => {
    it("should create with default settings", () => {
      const settings = store.getSettings();
      expect(settings.version).toBe("1.0.0");
      expect(settings.currentProfile).toBe("Default");
    });

    it("should have default toolbar layout", () => {
      const toolbar = store.getToolbarLayout();
      expect(toolbar.buttons.length).toBeGreaterThan(0);
      expect(toolbar.buttons).toContain("new-document");
    });

    it("should have light theme by default", () => {
      const theme = store.getTheme();
      expect(theme.preset).toBe("light");
      expect(theme.colors.create).toBe("#2E9E5B");
    });
  });

  describe("Toolbar Layout", () => {
    it("should update toolbar layout", () => {
      const newLayout = {
        buttons: ["new-document", "save-document"],
        homeTabButtons: ["new-document"],
        assemblyTabButtons: [],
        drawingTabButtons: [],
      };

      store.setToolbarLayout(newLayout);
      const layout = store.getToolbarLayout();
      expect(layout.buttons).toEqual(newLayout.buttons);
    });

    it("should persist toolbar changes", () => {
      const newLayout = {
        buttons: ["custom-button"],
        homeTabButtons: [],
        assemblyTabButtons: [],
        drawingTabButtons: [],
      };

      store.setToolbarLayout(newLayout);
      store.save();

      const newStore = new SettingsStore();
      const layout = newStore.getToolbarLayout();
      expect(layout.buttons).toContain("custom-button");
      newStore.destroy();
    });
  });

  describe("Themes", () => {
    it("should get light theme preset", () => {
      const theme = store.getThemeByPreset("light");
      expect(theme.preset).toBe("light");
      expect(theme.colors.tube).toBe("#1F6FC5");
    });

    it("should get dark theme preset", () => {
      const theme = store.getThemeByPreset("dark");
      expect(theme.preset).toBe("dark");
      expect(theme.colors.neutral).toBe("#A0A0A0");
    });

    it("should get high contrast theme preset", () => {
      const theme = store.getThemeByPreset("highContrast");
      expect(theme.preset).toBe("highContrast");
    });

    it("should get colorblind safe theme preset", () => {
      const theme = store.getThemeByPreset("colorblind");
      expect(theme.preset).toBe("colorblind");
    });

    it("should set custom theme", () => {
      const customTheme = {
        name: "Custom",
        preset: "custom" as const,
        colors: {
          tube: "#FF0000",
          create: "#00FF00",
          delete: "#0000FF",
          import: "#FFFF00",
          links: "#FF00FF",
          edit: "#00FFFF",
          update: "#FF8800",
          freeze: "#8800FF",
          auxiliary: "#008800",
          axes_x: "#FF0000",
          axes_y: "#00FF00",
          axes_z: "#0000FF",
          neutral: "#808080",
        },
      };

      store.setTheme(customTheme);
      const theme = store.getTheme();
      expect(theme.colors.tube).toBe("#FF0000");
    });

    it("should get color for category", () => {
      const color = store.getColorForCategory("create");
      expect(color).toBe("#2E9E5B");
    });
  });

  describe("Profiles", () => {
    it("should get default profile", () => {
      const profile = store.getProfile("Default");
      expect(profile).toBeDefined();
      expect(profile?.name).toBe("Default");
      expect(profile?.isDefault).toBe(true);
    });

    it("should create new profile", () => {
      const currentProfile = store.getCurrentProfile();
      const newProfile = {
        ...currentProfile,
        name: "Test Profile",
        isDefault: false,
      };

      store.saveProfile(newProfile);
      const retrieved = store.getProfile("Test Profile");
      expect(retrieved).toBeDefined();
      expect(retrieved?.name).toBe("Test Profile");
    });

    it("should switch current profile", () => {
      const newProfile = {
        ...store.getCurrentProfile(),
        name: "Profile 2",
        isDefault: false,
      };

      store.saveProfile(newProfile);
      store.setCurrentProfile("Profile 2");

      const current = store.getCurrentProfile();
      expect(current.name).toBe("Profile 2");
    });

    it("should delete profile", () => {
      const newProfile = {
        ...store.getCurrentProfile(),
        name: "To Delete",
        isDefault: false,
      };

      store.saveProfile(newProfile);
      store.deleteProfile("To Delete");

      const deleted = store.getProfile("To Delete");
      expect(deleted).toBeUndefined();
    });

    it("should not delete default profile", () => {
      expect(() => {
        store.deleteProfile("Default");
      }).toThrow();
    });

    it("should get all profile names", () => {
      const newProfile = {
        ...store.getCurrentProfile(),
        name: "Profile 2",
        isDefault: false,
      };

      store.saveProfile(newProfile);
      const names = store.getProfileNames();
      expect(names).toContain("Default");
      expect(names).toContain("Profile 2");
    });
  });

  describe("Export/Import", () => {
    it("should export settings as JSON", () => {
      const exported = store.exportSettings();
      expect(exported).toBeTruthy();

      const parsed = JSON.parse(exported);
      expect(parsed.version).toBe("1.0.0");
      expect(parsed.currentProfile).toBe("Default");
    });

    it("should import settings from JSON", () => {
      const customSettings = {
        version: "1.0.0",
        currentProfile: "Imported",
        toolbar: {
          buttons: ["custom"],
          homeTabButtons: [],
          assemblyTabButtons: [],
          drawingTabButtons: [],
        },
        theme: {
          name: "Dark",
          preset: "dark",
          colors: store.getTheme().colors,
        },
        profiles: {},
        workspaceProfiles: {},
      };

      const json = JSON.stringify(customSettings);
      store.importSettings(json);

      const settings = store.getSettings();
      expect(settings.currentProfile).toBe("Imported");
    });

    it("should throw on invalid JSON import", () => {
      expect(() => {
        store.importSettings("invalid json");
      }).toThrow();
    });
  });

  describe("Reset", () => {
    it("should reset to defaults", () => {
      const newProfile = {
        ...store.getCurrentProfile(),
        name: "Test",
        isDefault: false,
      };

      store.saveProfile(newProfile);
      store.resetToDefaults();

      const profile = store.getProfile("Test");
      expect(profile).toBeUndefined();

      const current = store.getCurrentProfile();
      expect(current.name).toBe("Default");
    });
  });

  describe("Workspace Profiles", () => {
    it("should save workspace profile", () => {
      const wsProfile = {
        name: "Project A",
        projectPath: "/path/to/project",
        lastModified: Date.now(),
        settings: {},
      };

      store.saveWorkspaceProfile(wsProfile);
      const retrieved = store.getWorkspaceProfile("Project A");
      expect(retrieved).toBeDefined();
      expect(retrieved?.projectPath).toBe("/path/to/project");
    });

    it("should delete workspace profile", () => {
      const wsProfile = {
        name: "Project B",
        projectPath: "/path/to/project",
        lastModified: Date.now(),
        settings: {},
      };

      store.saveWorkspaceProfile(wsProfile);
      store.deleteWorkspaceProfile("Project B");

      const retrieved = store.getWorkspaceProfile("Project B");
      expect(retrieved).toBeUndefined();
    });
  });

  describe("Persistence", () => {
    it("should save and load settings", (done) => {
      const toolbar = {
        buttons: ["test1", "test2"],
        homeTabButtons: [],
        assemblyTabButtons: [],
        drawingTabButtons: [],
      };

      store.setToolbarLayout(toolbar);
      store.save();

      // Simulate reload
      setTimeout(() => {
        const newStore = new SettingsStore();
        const loaded = newStore.getToolbarLayout();
        expect(loaded.buttons).toEqual(["test1", "test2"]);
        newStore.destroy();
        done();
      }, 100);
    });
  });
});
