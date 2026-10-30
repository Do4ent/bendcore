/**
 * BendCore Icon Theme System
 * Manages color themes for icons and UI elements
 * Supports Light, Dark, High Contrast, and Colorblind-safe themes
 * Dynamic theme switching without page reload
 */

import { IconTheme } from "./SettingsStore";

export interface ThemeColors {
  tube: string;
  create: string;
  delete: string;
  import: string;
  links: string;
  edit: string;
  update: string;
  freeze: string;
  auxiliary: string;
  axes_x: string;
  axes_y: string;
  axes_z: string;
  neutral: string;
}

export interface ThemeConfig {
  name: string;
  isDark: boolean;
  colors: ThemeColors;
  cssVariables: Record<string, string>;
}

/**
 * CSS Variable names for theme colors
 */
const THEME_CSS_VARIABLES = {
  tube: "--icon-tube-blue",
  create: "--icon-create-green",
  delete: "--icon-delete-red",
  import: "--icon-import-cyan",
  links: "--icon-links-purple",
  edit: "--icon-edit-orange",
  update: "--icon-update-yellow",
  freeze: "--icon-freeze-blue",
  auxiliary: "--icon-auxiliary-brown",
  axes_x: "--icon-axes-x-red",
  axes_y: "--icon-axes-y-green",
  axes_z: "--icon-axes-z-blue",
  neutral: "--icon-neutral-gray",
} as const;

/**
 * Light theme configuration
 */
const LIGHT_THEME: ThemeConfig = {
  name: "Light",
  isDark: false,
  colors: {
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
  },
  cssVariables: {},
};

/**
 * Dark theme configuration
 */
const DARK_THEME: ThemeConfig = {
  name: "Dark",
  isDark: true,
  colors: {
    tube: "#5BA3E3",
    create: "#5BC55B",
    delete: "#FF6B6B",
    import: "#4ECAD4",
    links: "#B792FF",
    edit: "#FFB84D",
    update: "#FFD93D",
    freeze: "#6DBFFF",
    auxiliary: "#FFD700",
    axes_x: "#FF6666",
    axes_y: "#66FF66",
    axes_z: "#6666FF",
    neutral: "#A0A0A0",
  },
  cssVariables: {},
};

/**
 * High Contrast theme configuration
 */
const HIGH_CONTRAST_THEME: ThemeConfig = {
  name: "High Contrast",
  isDark: false,
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
  cssVariables: {},
};

/**
 * Colorblind-safe theme (Deuteranopia)
 */
const COLORBLIND_THEME: ThemeConfig = {
  name: "Colorblind Safe",
  isDark: false,
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
  cssVariables: {},
};

/**
 * Initialize CSS variables for a theme
 */
function initializeCssVariables(theme: ThemeConfig): void {
  const cssVariables: Record<string, string> = {};
  for (const [colorKey, cssVar] of Object.entries(THEME_CSS_VARIABLES)) {
    cssVariables[cssVar] = theme.colors[colorKey as keyof ThemeColors];
  }
  theme.cssVariables = cssVariables;
}

// Initialize all predefined themes
initializeCssVariables(LIGHT_THEME);
initializeCssVariables(DARK_THEME);
initializeCssVariables(HIGH_CONTRAST_THEME);
initializeCssVariables(COLORBLIND_THEME);

/**
 * IconThemeManager: Manages theme switching and color lookup
 */
export class IconThemeManager {
  private currentTheme: ThemeConfig;
  private customTheme: ThemeConfig | null = null;
  private root: HTMLElement;

  constructor(initialTheme?: IconTheme) {
    this.root = document.documentElement;
    this.currentTheme = initialTheme
      ? this.createThemeConfig(initialTheme)
      : { ...LIGHT_THEME };
    this.applyTheme();
  }

  /**
   * Create a theme config from an IconTheme object
   */
  private createThemeConfig(theme: IconTheme): ThemeConfig {
    const config: ThemeConfig = {
      name: theme.name,
      isDark: theme.preset === "dark",
      colors: theme.colors as unknown as ThemeColors,
      cssVariables: {},
    };
    initializeCssVariables(config);
    return config;
  }

  /**
   * Apply current theme to DOM
   */
  private applyTheme(): void {
    // Set CSS variables on root element
    for (const [key, value] of Object.entries(this.currentTheme.cssVariables)) {
      this.root.style.setProperty(key, value);
    }

    // Set dark mode class if needed
    if (this.currentTheme.isDark) {
      this.root.classList.add("dark-theme");
      this.root.classList.remove("light-theme");
    } else {
      this.root.classList.add("light-theme");
      this.root.classList.remove("dark-theme");
    }

    // Dispatch custom event for theme change
    window.dispatchEvent(
      new CustomEvent("themeChanged", {
        detail: { theme: this.currentTheme.name },
      })
    );
  }

  /**
   * Set theme to a predefined preset
   */
  public setPreset(
    preset: "light" | "dark" | "highContrast" | "colorblind"
  ): void {
    switch (preset) {
      case "light":
        this.currentTheme = { ...LIGHT_THEME };
        break;
      case "dark":
        this.currentTheme = { ...DARK_THEME };
        break;
      case "highContrast":
        this.currentTheme = { ...HIGH_CONTRAST_THEME };
        break;
      case "colorblind":
        this.currentTheme = { ...COLORBLIND_THEME };
        break;
    }
    this.customTheme = null;
    this.applyTheme();
  }

  /**
   * Set a custom theme
   */
  public setCustomTheme(theme: IconTheme): void {
    this.currentTheme = this.createThemeConfig(theme);
    this.customTheme = this.currentTheme;
    this.applyTheme();
  }

  /**
   * Get color for a specific category
   */
  public getColor(category: keyof ThemeColors): string {
    return this.currentTheme.colors[category];
  }

  /**
   * Get CSS variable name for a category
   */
  public getCssVariable(category: keyof ThemeColors): string {
    return THEME_CSS_VARIABLES[category];
  }

  /**
   * Get all colors in current theme
   */
  public getAllColors(): ThemeColors {
    return { ...this.currentTheme.colors };
  }

  /**
   * Get current theme name
   */
  public getThemeName(): string {
    return this.currentTheme.name;
  }

  /**
   * Check if current theme is dark
   */
  public isDarkTheme(): boolean {
    return this.currentTheme.isDark;
  }

  /**
   * Get current theme config
   */
  public getCurrentTheme(): ThemeConfig {
    return JSON.parse(JSON.stringify(this.currentTheme));
  }
}

// Global singleton instance
let globalThemeManager: IconThemeManager | null = null;

/**
 * Initialize the global theme manager
 */
export function initializeThemeManager(theme?: IconTheme): IconThemeManager {
  if (!globalThemeManager) {
    globalThemeManager = new IconThemeManager(theme);
  }
  return globalThemeManager;
}

/**
 * Get the global theme manager
 */
export function getThemeManager(): IconThemeManager {
  if (!globalThemeManager) {
    globalThemeManager = new IconThemeManager();
  }
  return globalThemeManager;
}

/**
 * SVG Icon wrapper component that applies theme colors
 * Usage: <ThemedSVGIcon category="create" src="path/to/icon.svg" />
 */
export interface ThemedIconProps {
  category: keyof ThemeColors;
  src: string;
  size?: number; // 16, 24, or 32
  className?: string;
  title?: string;
}

/**
 * Create inline SVG with theme color
 * This is useful for dynamic color application
 */
export function createThemedSVG(
  svg: string,
  color: string,
  size: number = 24
): string {
  // Add fill attribute if not present
  const withColor = svg.includes('fill="')
    ? svg.replace(/fill="[^"]*"/g, `fill="${color}"`)
    : svg.replace("<svg", `<svg fill="${color}"`);

  // Add size attributes
  return withColor.replace(/<svg/, `<svg width="${size}" height="${size}"`);
}

/**
 * Get all available theme presets
 */
export function getAvailableThemes(): ThemeConfig[] {
  return [LIGHT_THEME, DARK_THEME, HIGH_CONTRAST_THEME, COLORBLIND_THEME];
}

/**
 * Export theme to JSON
 */
export function exportTheme(theme: ThemeConfig): string {
  return JSON.stringify(
    {
      name: theme.name,
      isDark: theme.isDark,
      colors: theme.colors,
    },
    null,
    2
  );
}

/**
 * Check if two colors are similar enough to be visually equivalent
 */
export function areColorsSimilar(
  color1: string,
  color2: string,
  threshold: number = 30
): boolean {
  const rgb1 = hexToRgb(color1);
  const rgb2 = hexToRgb(color2);

  if (!rgb1 || !rgb2) return false;

  const diff = Math.sqrt(
    Math.pow(rgb1.r - rgb2.r, 2) +
      Math.pow(rgb1.g - rgb2.g, 2) +
      Math.pow(rgb1.b - rgb2.b, 2)
  );

  return diff < threshold;
}

/**
 * Convert hex color to RGB
 */
function hexToRgb(hex: string): { r: number; g: number; b: number } | null {
  const result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
  return result
    ? {
        r: parseInt(result[1], 16),
        g: parseInt(result[2], 16),
        b: parseInt(result[3], 16),
      }
    : null;
}

/**
 * Adjust brightness of a color
 */
export function adjustColorBrightness(color: string, percent: number): string {
  const rgb = hexToRgb(color);
  if (!rgb) return color;

  let r = Math.min(255, Math.round(rgb.r * (1 + percent / 100)));
  let g = Math.min(255, Math.round(rgb.g * (1 + percent / 100)));
  let b = Math.min(255, Math.round(rgb.b * (1 + percent / 100)));

  return `#${((1 << 24) + (r << 16) + (g << 8) + b).toString(16).slice(1)}`;
}
