/**
 * BendCore Icon Theme System Tests
 */

import {
  IconThemeManager,
  initializeThemeManager,
  getThemeManager,
  adjustColorBrightness,
  areColorsSimilar,
  getAvailableThemes,
} from "../IconThemeSystem";

describe("IconThemeManager", () => {
  let manager: IconThemeManager;

  beforeEach(() => {
    // Create a new instance for each test
    manager = new IconThemeManager();
  });

  describe("Theme Presets", () => {
    it("should initialize with light theme", () => {
      expect(manager.getThemeName()).toBe("Light");
      expect(manager.isDarkTheme()).toBe(false);
    });

    it("should switch to dark theme", () => {
      manager.setPreset("dark");
      expect(manager.getThemeName()).toBe("Dark");
      expect(manager.isDarkTheme()).toBe(true);
    });

    it("should switch to high contrast theme", () => {
      manager.setPreset("highContrast");
      expect(manager.getThemeName()).toBe("High Contrast");
    });

    it("should switch to colorblind safe theme", () => {
      manager.setPreset("colorblind");
      expect(manager.getThemeName()).toBe("Colorblind Safe");
    });
  });

  describe("Color Management", () => {
    it("should get color for category", () => {
      const color = manager.getColor("create");
      expect(color).toBe("#2E9E5B");
    });

    it("should get all colors", () => {
      const colors = manager.getAllColors();
      expect(colors.tube).toBeDefined();
      expect(colors.create).toBeDefined();
      expect(colors.delete).toBeDefined();
    });

    it("should get CSS variable name", () => {
      const cssVar = manager.getCssVariable("create");
      expect(cssVar).toBe("--icon-create-green");
    });

    it("should get current theme config", () => {
      const config = manager.getCurrentTheme();
      expect(config.name).toBe("Light");
      expect(config.colors).toBeDefined();
    });

    it("should have all 13 color categories", () => {
      const colors = manager.getAllColors();
      const categories = [
        "tube",
        "create",
        "delete",
        "import",
        "links",
        "edit",
        "update",
        "freeze",
        "auxiliary",
        "axes_x",
        "axes_y",
        "axes_z",
        "neutral",
      ];

      for (const cat of categories) {
        expect(colors[cat as keyof typeof colors]).toBeDefined();
      }
    });
  });

  describe("Custom Themes", () => {
    it("should set custom theme", () => {
      const customTheme = {
        name: "Custom Test",
        isDark: false,
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
        cssVariables: {},
      };

      manager.setCustomTheme({
        name: "Custom Test",
        preset: "custom" as const,
        colors: customTheme.colors,
      });

      expect(manager.getThemeName()).toBe("Custom Test");
      expect(manager.getColor("tube")).toBe("#FF0000");
    });
  });

  describe("Color Utilities", () => {
    it("should check if colors are similar", () => {
      const similar = areColorsSimilar("#FF0000", "#FE0000");
      expect(similar).toBe(true);
    });

    it("should detect different colors", () => {
      const different = areColorsSimilar("#FF0000", "#0000FF", 10);
      expect(different).toBe(false);
    });

    it("should adjust color brightness", () => {
      const brightened = adjustColorBrightness("#808080", 20);
      expect(brightened).toBeTruthy();
      expect(brightened).not.toBe("#808080");
    });

    it("should darken colors", () => {
      const darkened = adjustColorBrightness("#FFFFFF", -50);
      expect(darkened).toBeTruthy();
      expect(darkened).not.toBe("#FFFFFF");
    });
  });

  describe("Theme Events", () => {
    it("should dispatch theme change event", (done) => {
      const listener = (event: CustomEvent) => {
        expect(event.detail.theme).toBe("Dark");
        window.removeEventListener("themeChanged", listener);
        done();
      };

      window.addEventListener("themeChanged", listener);
      manager.setPreset("dark");
    });
  });

  describe("Global Theme Manager", () => {
    it("should initialize global theme manager", () => {
      const mgr = initializeThemeManager();
      expect(mgr).toBeDefined();
      expect(mgr.getThemeName()).toBe("Light");
    });

    it("should return same instance on subsequent calls", () => {
      const mgr1 = getThemeManager();
      const mgr2 = getThemeManager();
      expect(mgr1).toBe(mgr2);
    });

    it("should get available themes", () => {
      const themes = getAvailableThemes();
      expect(themes.length).toBeGreaterThanOrEqual(4);

      const themeNames = themes.map((t) => t.name);
      expect(themeNames).toContain("Light");
      expect(themeNames).toContain("Dark");
    });
  });

  describe("CSS Variable Application", () => {
    it("should apply CSS variables to root", () => {
      const root = document.documentElement;
      const style = root.style;

      // Check that CSS variables are set
      const tubeColor = style.getPropertyValue("--icon-tube-blue");
      expect(tubeColor).toBeTruthy();
    });

    it("should update CSS variables on theme change", () => {
      const root = document.documentElement;

      manager.setPreset("dark");

      const tubeColorDark = root.style.getPropertyValue("--icon-tube-blue");
      expect(tubeColorDark).toBeTruthy();
    });

    it("should add dark-theme class for dark mode", () => {
      const root = document.documentElement;

      manager.setPreset("dark");
      expect(root.classList.contains("dark-theme")).toBe(true);
      expect(root.classList.contains("light-theme")).toBe(false);

      manager.setPreset("light");
      expect(root.classList.contains("light-theme")).toBe(true);
      expect(root.classList.contains("dark-theme")).toBe(false);
    });
  });

  describe("Theme Presets Colors", () => {
    it("light theme should have expected colors", () => {
      manager.setPreset("light");
      const colors = manager.getAllColors();

      expect(colors.tube).toBe("#1F6FC5");
      expect(colors.create).toBe("#2E9E5B");
      expect(colors.delete).toBe("#D93A3A");
    });

    it("dark theme should have different colors than light", () => {
      manager.setPreset("light");
      const lightColors = manager.getAllColors();

      manager.setPreset("dark");
      const darkColors = manager.getAllColors();

      expect(lightColors.neutral).not.toBe(darkColors.neutral);
    });

    it("high contrast theme should have bright colors", () => {
      manager.setPreset("highContrast");
      const colors = manager.getAllColors();

      // High contrast should have distinct, bright colors
      expect(colors.create).toBe("#00CC00");
      expect(colors.delete).toBe("#FF0000");
    });

    it("colorblind theme should have accessible colors", () => {
      manager.setPreset("colorblind");
      const colors = manager.getAllColors();

      // Colorblind palette should have specific colors
      expect(colors.tube).toBe("#0173B2");
      expect(colors.create).toBe("#DE8F05");
    });
  });
});
