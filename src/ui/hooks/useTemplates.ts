/**
 * useTemplates React Hook
 * Manages template state and operations for UI components
 * Task 2.5: Template Library Management
 */

import { useState, useCallback, useEffect } from "react";

// ============ INTERFACES ============

export interface TemplateParameter {
  min: number;
  max: number;
  default: number;
  unit: string;
}

export interface Template {
  id: string;
  name: string;
  description: string;
  category: string;
  outer_diameter: TemplateParameter;
  wall_thickness: TemplateParameter;
  length: TemplateParameter;
  use_case: string;
  compatible_materials: string[];
}

export interface TemplateMatch {
  template_id: string;
  template_name: string;
  confidence: number;  // 0-100
}

export interface TemplateLibrary {
  templates: Template[];
  active_template_id: string;
}

// ============ CUSTOM HOOK ============

const TEMPLATES_STORAGE_KEY = "bendcore_templates";
const TEMPLATES_JSON_PATH = "/api/templates";

/**
 * Custom hook for template management
 */
export const useTemplates = () => {
  const [library, setLibrary] = useState<TemplateLibrary>({
    templates: [],
    active_template_id: "",
  });
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  // Load templates from JSON file or localStorage
  useEffect(() => {
    const loadTemplates = async () => {
      try {
        // Try to load from localStorage first
        const stored = localStorage.getItem(TEMPLATES_STORAGE_KEY);
        if (stored) {
          setLibrary(JSON.parse(stored));
          setLoading(false);
          return;
        }

        // Fall back to loading from JSON file
        const response = await fetch("/data/templates.json");
        if (!response.ok) {
          throw new Error("Failed to load templates");
        }
        const data = await response.json();
        setLibrary(data);
        localStorage.setItem(TEMPLATES_STORAGE_KEY, JSON.stringify(data));
        setError(null);
      } catch (err) {
        setError(err instanceof Error ? err.message : "Unknown error");
        // Initialize with empty library on error
        setLibrary({
          templates: [],
          active_template_id: "",
        });
      } finally {
        setLoading(false);
      }
    };

    loadTemplates();
  }, []);

  // Persist library to localStorage
  const persistLibrary = useCallback((newLibrary: TemplateLibrary) => {
    localStorage.setItem(TEMPLATES_STORAGE_KEY, JSON.stringify(newLibrary));
  }, []);

  // Add template
  const addTemplate = useCallback(
    (template: Template) => {
      const newLibrary: TemplateLibrary = {
        ...library,
        templates: [...library.templates, template],
      };
      setLibrary(newLibrary);
      persistLibrary(newLibrary);
    },
    [library, persistLibrary]
  );

  // Update template
  const updateTemplate = useCallback(
    (id: string, updates: Partial<Template>) => {
      const newLibrary: TemplateLibrary = {
        ...library,
        templates: library.templates.map((t) =>
          t.id === id ? { ...t, ...updates } : t
        ),
      };
      setLibrary(newLibrary);
      persistLibrary(newLibrary);
    },
    [library, persistLibrary]
  );

  // Delete template
  const deleteTemplate = useCallback(
    (id: string) => {
      const newLibrary: TemplateLibrary = {
        ...library,
        templates: library.templates.filter((t) => t.id !== id),
        active_template_id:
          library.active_template_id === id ? "" : library.active_template_id,
      };
      setLibrary(newLibrary);
      persistLibrary(newLibrary);
    },
    [library, persistLibrary]
  );

  // Get template by ID
  const getTemplate = useCallback(
    (id: string) => {
      return library.templates.find((t) => t.id === id);
    },
    [library.templates]
  );

  // Get active template
  const getActiveTemplate = useCallback(() => {
    return library.templates.find((t) => t.id === library.active_template_id);
  }, [library]);

  // Set active template
  const setActiveTemplate = useCallback(
    (id: string) => {
      const newLibrary: TemplateLibrary = {
        ...library,
        active_template_id: id,
      };
      setLibrary(newLibrary);
      persistLibrary(newLibrary);
    },
    [library, persistLibrary]
  );

  // Auto-select templates for given parameters
  const selectTemplates = useCallback(
    (od: number, wall: number, length: number): TemplateMatch[] => {
      const matches = library.templates
        .map((template) => {
          // Calculate parameter scores (0-1)
          const odScore = calculateParameterScore(
            od,
            template.outer_diameter
          );
          const wallScore = calculateParameterScore(
            wall,
            template.wall_thickness
          );
          const lengthScore = calculateParameterScore(
            length,
            template.length
          );

          // Weighted sum: OD=50%, Wall=30%, Length=20%
          const totalScore =
            odScore * 0.5 + wallScore * 0.3 + lengthScore * 0.2;

          return {
            template_id: template.id,
            template_name: template.name,
            confidence: totalScore * 100,
          };
        })
        .sort((a, b) => b.confidence - a.confidence)
        .slice(0, 3);

      return matches;
    },
    [library.templates]
  );

  // Export templates as JSON
  const exportTemplates = useCallback(() => {
    const dataStr = JSON.stringify(library, null, 2);
    const dataBlob = new Blob([dataStr], { type: "application/json" });
    const url = URL.createObjectURL(dataBlob);
    const link = document.createElement("a");
    link.href = url;
    link.download = `bendcore-templates-${Date.now()}.json`;
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
    URL.revokeObjectURL(url);
  }, [library]);

  // Import templates from JSON
  const importTemplates = useCallback(
    (file: File): Promise<void> => {
      return new Promise((resolve, reject) => {
        const reader = new FileReader();
        reader.onload = (e) => {
          try {
            const content = e.target?.result as string;
            const imported = JSON.parse(content) as TemplateLibrary;

            // Validate structure
            if (!Array.isArray(imported.templates)) {
              throw new Error("Invalid template file format");
            }

            setLibrary(imported);
            persistLibrary(imported);
            resolve();
          } catch (err) {
            reject(err instanceof Error ? err : new Error("Import failed"));
          }
        };
        reader.onerror = () => {
          reject(new Error("Failed to read file"));
        };
        reader.readAsText(file);
      });
    },
    [persistLibrary]
  );

  return {
    library,
    loading,
    error,
    addTemplate,
    updateTemplate,
    deleteTemplate,
    getTemplate,
    getActiveTemplate,
    setActiveTemplate,
    selectTemplates,
    exportTemplates,
    importTemplates,
  };
};

// ============ HELPER FUNCTIONS ============

/**
 * Calculate parameter match score (0.0 to 1.0)
 * Returns 1.0 if value is within range, decreases with distance
 */
function calculateParameterScore(
  value: number,
  param: TemplateParameter
): number {
  // If value is within range, return perfect score
  if (value >= param.min && value <= param.max) {
    return 1.0;
  }

  // If value is outside range, calculate penalty
  const distance =
    value < param.min ? param.min - value : value - param.max;

  // Maximum penalty: distance as percentage of range
  const range = param.max - param.min;
  if (range === 0) return 0.0;

  // Score decreases linearly with distance
  const score = Math.max(0.0, 1.0 - distance / range);
  return score;
}
