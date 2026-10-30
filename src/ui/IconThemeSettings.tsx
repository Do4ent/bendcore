/**
 * BendCore Icon Theme Settings Component
 * Color picker for 13 icon categories with presets and custom themes
 */

import React, { useState, useCallback } from "react";
import { IconTheme } from "./SettingsStore";
import { getThemeManager } from "./IconThemeSystem";
import "./IconThemeSettings.css";

interface IconThemeSettingsProps {
  currentTheme: IconTheme;
  onThemeChange: (theme: IconTheme) => void;
}

interface ColorPickerState {
  isOpen: boolean;
  category: string;
  color: string;
}

const COLOR_CATEGORIES = [
  {
    key: "tube",
    name: "Tube",
    usage: "Tube bodies, main elements",
    examples: ["New Tube", "Tube Body"],
  },
  {
    key: "create",
    name: "Create",
    usage: "Creation commands",
    examples: ["New Sketch", "New Component"],
  },
  {
    key: "delete",
    name: "Delete",
    usage: "Delete & prohibition",
    examples: ["Delete Element", "Remove"],
  },
  {
    key: "import",
    name: "Import",
    usage: "Imported data, geometry",
    examples: ["Import STEP", "Import DWG"],
  },
  {
    key: "links",
    name: "Links",
    usage: "References, constraints",
    examples: ["Port", "Trace Tube"],
  },
  {
    key: "edit",
    name: "Edit",
    usage: "Edit & transformation",
    examples: ["Edit Tube", "Move"],
  },
  {
    key: "update",
    name: "Update",
    usage: "Recalculate, refresh",
    examples: ["Recalculate", "Sync"],
  },
  {
    key: "freeze",
    name: "Freeze",
    usage: "Lock, suppress changes",
    examples: ["Lock", "Freeze Link"],
  },
  {
    key: "auxiliary",
    name: "Auxiliary",
    usage: "Help geometry, aids",
    examples: ["Reference Plane", "Axis"],
  },
  {
    key: "axes_x",
    name: "X Axis",
    usage: "X-axis of coordinate system",
    examples: ["X Direction"],
  },
  {
    key: "axes_y",
    name: "Y Axis",
    usage: "Y-axis of coordinate system",
    examples: ["Y Direction"],
  },
  {
    key: "axes_z",
    name: "Z Axis",
    usage: "Z-axis of coordinate system",
    examples: ["Z Direction"],
  },
  {
    key: "neutral",
    name: "Neutral",
    usage: "Outlines, shadows, defaults",
    examples: ["Border", "Background"],
  },
];

const THEME_PRESETS = [
  {
    id: "light",
    name: "Light",
    description: "Bright, soft colors",
    icon: "☀️",
  },
  {
    id: "dark",
    name: "Dark",
    description: "Dark theme optimized",
    icon: "🌙",
  },
  {
    id: "highContrast",
    name: "High Contrast",
    description: "Maximum visibility",
    icon: "◆",
  },
  {
    id: "colorblind",
    name: "Colorblind Safe",
    description: "Deuteranopia palette",
    icon: "🎯",
  },
];

const IconThemeSettings: React.FC<IconThemeSettingsProps> = ({
  currentTheme,
  onThemeChange,
}) => {
  const [colors, setColors] = useState<Record<string, string>>(
    currentTheme.colors
  );
  const [colorPicker, setColorPicker] = useState<ColorPickerState>({
    isOpen: false,
    category: "",
    color: "",
  });
  const [saveState, setSaveState] = useState<{
    type: "success" | "error" | "info" | null;
    message: string;
  }>({ type: null, message: "" });
  const [fileInputLabel, setFileInputLabel] = useState("Choose file...");

  // Open color picker
  const openColorPicker = (category: string) => {
    setColorPicker({
      isOpen: true,
      category,
      color: colors[category],
    });
  };

  // Close color picker
  const closeColorPicker = () => {
    setColorPicker({
      isOpen: false,
      category: "",
      color: "",
    });
  };

  // Update color
  const updateColor = (category: string, color: string) => {
    const newColors = { ...colors, [category]: color };
    setColors(newColors);
  };

  // Apply preset
  const applyPreset = (
    presetId: "light" | "dark" | "highContrast" | "colorblind"
  ) => {
    const themeManager = getThemeManager();
    const presetTheme = themeManager.getCurrentTheme();

    // Get preset colors from theme manager
    const themeConfig = themeManager.getAllColors
      ? themeManager.getAllColors()
      : colors;

    // Create new theme with preset colors
    const newTheme: IconTheme = {
      name: THEME_PRESETS.find((p) => p.id === presetId)?.name || "Custom",
      preset: presetId,
      colors: { ...colors }, // Will be overwritten below
    };

    // Apply preset color values
    themeManager.setPreset(presetId);
    const newColors = themeManager.getAllColors();
    setColors(newColors as Record<string, string>);

    newTheme.colors = newColors;
    onThemeChange(newTheme);
    setSaveState({ type: "success", message: `Preset "${newTheme.name}" applied` });
    setTimeout(() => setSaveState({ type: null, message: "" }), 3000);
  };

  // Save custom theme
  const saveTheme = useCallback(() => {
    const newTheme: IconTheme = {
      name: currentTheme.name || "Custom Theme",
      preset: "custom",
      colors,
    };
    onThemeChange(newTheme);
    setSaveState({ type: "success", message: "Theme saved successfully" });
    setTimeout(() => setSaveState({ type: null, message: "" }), 3000);
  }, [colors, currentTheme.name, onThemeChange]);

  // Export theme
  const exportTheme = () => {
    const themeData = {
      name: currentTheme.name,
      preset: currentTheme.preset,
      colors,
    };
    const json = JSON.stringify(themeData, null, 2);
    const blob = new Blob([json], { type: "application/json" });
    const url = URL.createObjectURL(blob);
    const link = document.createElement("a");
    link.href = url;
    link.download = `theme-${currentTheme.name || "custom"}.json`;
    link.click();
    URL.revokeObjectURL(url);
    setSaveState({ type: "success", message: "Theme exported" });
    setTimeout(() => setSaveState({ type: null, message: "" }), 3000);
  };

  // Import theme
  const importTheme = (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (!file) return;

    const reader = new FileReader();
    reader.onload = (event) => {
      try {
        const imported = JSON.parse(event.target?.result as string);
        if (imported.colors) {
          setColors(imported.colors);
          setSaveState({
            type: "success",
            message: `Theme "${imported.name}" imported`,
          });
          setTimeout(() => setSaveState({ type: null, message: "" }), 3000);
        }
      } catch (error) {
        setSaveState({
          type: "error",
          message: "Failed to import theme file",
        });
      }
    };
    reader.readAsText(file);

    // Reset file input
    if (e.target) {
      e.target.value = "";
      setFileInputLabel("Choose file...");
    }
  };

  // Reset to defaults
  const resetToDefaults = () => {
    setColors({ ...currentTheme.colors });
    setSaveState({ type: "info", message: "Reset to current theme" });
    setTimeout(() => setSaveState({ type: null, message: "" }), 3000);
  };

  return (
    <div className="icon-theme-settings">
      <h2 className="icon-theme-settings-title">Icon Theme Settings</h2>

      {/* Presets */}
      <div className="theme-presets">
        {THEME_PRESETS.map((preset) => (
          <button
            key={preset.id}
            className={`preset-button ${
              currentTheme.preset === preset.id ? "active" : ""
            }`}
            onClick={() =>
              applyPreset(preset.id as "light" | "dark" | "highContrast" | "colorblind")
            }
          >
            <div className="preset-icon" style={{ backgroundColor: colors.tube }}>
              {preset.icon}
            </div>
            <div className="preset-info">
              <span className="preset-name">{preset.name}</span>
              <span className="preset-description">{preset.description}</span>
            </div>
          </button>
        ))}
      </div>

      {/* Save State */}
      {saveState.type && (
        <div className={`theme-save-state ${saveState.type}`}>
          <span className="save-state-icon">
            {saveState.type === "success" ? "✓" : saveState.type === "error" ? "✕" : "ℹ"}
          </span>
          {saveState.message}
        </div>
      )}

      {/* Color Picker Grid */}
      <div className="color-picker-grid">
        {COLOR_CATEGORIES.map((cat) => (
          <div key={cat.key} className="color-category">
            <div className="color-category-header">
              <div
                className="color-category-swatch"
                style={{ backgroundColor: colors[cat.key] }}
                onClick={() => openColorPicker(cat.key)}
                title="Click to change color"
              />
              <div className="color-category-label">
                <div className="color-category-name">{cat.name}</div>
                <div className="color-category-usage">{cat.usage}</div>
              </div>
            </div>
            <input
              type="text"
              className="color-category-input"
              value={colors[cat.key]}
              onChange={(e) => updateColor(cat.key, e.target.value)}
              placeholder="#000000"
            />
            <div className="color-category-example">
              {cat.examples.map((example) => (
                <div
                  key={example}
                  className="example-icon"
                  style={{ backgroundColor: colors[cat.key] }}
                  title={example}
                >
                  {example.substring(0, 2)}
                </div>
              ))}
            </div>
          </div>
        ))}
      </div>

      {/* Preview Section */}
      <div className="theme-preview">
        <div className="preview-title">Live Preview</div>
        <div className="preview-row">
          {COLOR_CATEGORIES.slice(0, 6).map((cat) => (
            <div key={cat.key} className="preview-item">
              <div
                className="preview-icon"
                style={{ backgroundColor: colors[cat.key] }}
              >
                ◆
              </div>
              <span>{cat.name}</span>
            </div>
          ))}
        </div>
        <div className="preview-row">
          {COLOR_CATEGORIES.slice(6).map((cat) => (
            <div key={cat.key} className="preview-item">
              <div
                className="preview-icon"
                style={{ backgroundColor: colors[cat.key] }}
              >
                ◆
              </div>
              <span>{cat.name}</span>
            </div>
          ))}
        </div>
      </div>

      {/* Import/Export Section */}
      <div className="import-export-section">
        <div className="section-title">Import & Export</div>
        <div className="file-input-group">
          <input
            type="file"
            id="theme-import"
            className="file-input"
            accept=".json"
            onChange={importTheme}
          />
          <label htmlFor="theme-import" className="file-input-label">
            {fileInputLabel}
          </label>
          <button
            className="file-input-button"
            onClick={() => document.getElementById("theme-import")?.click()}
          >
            Import Theme
          </button>
          <button className="file-input-button" onClick={exportTheme}>
            Export Theme
          </button>
        </div>
      </div>

      {/* Action Buttons */}
      <div className="theme-actions">
        <button
          className="theme-action-button primary"
          onClick={saveTheme}
        >
          ✓ Save Theme
        </button>
        <button className="theme-action-button" onClick={resetToDefaults}>
          ↺ Reset to Defaults
        </button>
      </div>
    </div>
  );
};

export default IconThemeSettings;
