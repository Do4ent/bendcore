/**
 * Template Editor Component
 * UI for viewing and editing tube bend templates
 * Task 2.3: React UI Components (Subtask 3)
 */

import React, { useState, useCallback } from "react";
import { Template, TemplateParameter } from "./hooks/useTemplates";
import { getColorHex } from "./ColorScheme";

interface TemplateEditorProps {
  template: Partial<Template>;
  onSave: (template: Partial<Template>) => void;
  onCancel: () => void;
  isNew?: boolean;
}

export const TemplateEditor: React.FC<TemplateEditorProps> = ({
  template,
  onSave,
  onCancel,
  isNew = false,
}) => {
  const [formData, setFormData] = useState<Partial<Template>>(template);
  const [errors, setErrors] = useState<Record<string, string>>({});

  const handleInputChange = useCallback(
    (
      field: keyof Template,
      value: any,
      nestedField?: keyof TemplateParameter
    ) => {
      if (nestedField && field in template && typeof template[field] === "object") {
        const param = formData[field] as TemplateParameter | undefined;
        setFormData({
          ...formData,
          [field]: {
            ...param,
            [nestedField]: parseFloat(value),
          },
        });
      } else {
        setFormData({
          ...formData,
          [field]: value,
        });
      }
      // Clear error for this field
      setErrors({ ...errors, [field]: "" });
    },
    [formData, errors]
  );

  const validateForm = useCallback((): boolean => {
    const newErrors: Record<string, string> = {};

    if (!formData.id?.trim()) {
      newErrors.id = "Template ID is required";
    }
    if (!formData.name?.trim()) {
      newErrors.name = "Template name is required";
    }

    // Validate parameter ranges
    if (formData.outer_diameter) {
      const od = formData.outer_diameter;
      if (od.min >= od.max) {
        newErrors.outer_diameter = "Min must be less than Max";
      }
    }
    if (formData.wall_thickness) {
      const wall = formData.wall_thickness;
      if (wall.min >= wall.max) {
        newErrors.wall_thickness = "Min must be less than Max";
      }
    }

    setErrors(newErrors);
    return Object.keys(newErrors).length === 0;
  }, [formData]);

  const handleSave = useCallback(() => {
    if (validateForm()) {
      onSave(formData);
    }
  }, [formData, validateForm, onSave]);

  const renderParameterEditor = (
    label: string,
    field: keyof Template,
    param: TemplateParameter | undefined
  ) => {
    if (!param) return null;

    const error = errors[field];
    const textColor = error ? getColorHex("error") : getColorHex("text");

    return (
      <div style={{ marginBottom: "16px" }}>
        <label style={{ display: "block", marginBottom: "4px", fontWeight: 500 }}>
          {label}
        </label>
        <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr 1fr", gap: "8px" }}>
          <div>
            <label style={{ display: "block", fontSize: "12px", marginBottom: "2px" }}>
              Min
            </label>
            <input
              type="number"
              value={param.min}
              onChange={(e) => handleInputChange(field, e.target.value, "min")}
              style={{
                width: "100%",
                padding: "8px",
                border: `1px solid ${error ? getColorHex("error") : getColorHex("border")}`,
                borderRadius: "4px",
              }}
            />
          </div>
          <div>
            <label style={{ display: "block", fontSize: "12px", marginBottom: "2px" }}>
              Max
            </label>
            <input
              type="number"
              value={param.max}
              onChange={(e) => handleInputChange(field, e.target.value, "max")}
              style={{
                width: "100%",
                padding: "8px",
                border: `1px solid ${error ? getColorHex("error") : getColorHex("border")}`,
                borderRadius: "4px",
              }}
            />
          </div>
          <div>
            <label style={{ display: "block", fontSize: "12px", marginBottom: "2px" }}>
              Default
            </label>
            <input
              type="number"
              value={param.default}
              onChange={(e) => handleInputChange(field, e.target.value, "default")}
              style={{
                width: "100%",
                padding: "8px",
                border: `1px solid ${getColorHex("border")}`,
                borderRadius: "4px",
              }}
            />
          </div>
        </div>
        {error && (
          <span style={{ color: textColor, fontSize: "12px", marginTop: "4px" }}>
            {error}
          </span>
        )}
      </div>
    );
  };

  return (
    <div
      style={{
        padding: "24px",
        maxHeight: "80vh",
        overflowY: "auto",
        backgroundColor: getColorHex("background"),
      }}
    >
      <h2 style={{ marginTop: 0, marginBottom: "24px", color: getColorHex("text") }}>
        {isNew ? "Create New Template" : "Edit Template"}
      </h2>

      {/* Basic Info Section */}
      <div
        style={{
          marginBottom: "24px",
          padding: "16px",
          backgroundColor: getColorHex("surface"),
          borderRadius: "8px",
          border: `1px solid ${getColorHex("border")}`,
        }}
      >
        <h3 style={{ marginTop: 0, color: getColorHex("text") }}>Basic Information</h3>

        <div style={{ marginBottom: "12px" }}>
          <label style={{ display: "block", marginBottom: "4px", fontWeight: 500 }}>
            Template ID
          </label>
          <input
            type="text"
            value={formData.id || ""}
            onChange={(e) => handleInputChange("id", e.target.value)}
            disabled={!isNew}
            style={{
              width: "100%",
              padding: "8px",
              border: `1px solid ${errors.id ? getColorHex("error") : getColorHex("border")}`,
              borderRadius: "4px",
              opacity: !isNew ? 0.6 : 1,
            }}
          />
          {errors.id && (
            <span style={{ color: getColorHex("error"), fontSize: "12px" }}>
              {errors.id}
            </span>
          )}
        </div>

        <div style={{ marginBottom: "12px" }}>
          <label style={{ display: "block", marginBottom: "4px", fontWeight: 500 }}>
            Template Name
          </label>
          <input
            type="text"
            value={formData.name || ""}
            onChange={(e) => handleInputChange("name", e.target.value)}
            style={{
              width: "100%",
              padding: "8px",
              border: `1px solid ${errors.name ? getColorHex("error") : getColorHex("border")}`,
              borderRadius: "4px",
            }}
            placeholder="e.g., Standard 90° Bend"
          />
          {errors.name && (
            <span style={{ color: getColorHex("error"), fontSize: "12px" }}>
              {errors.name}
            </span>
          )}
        </div>

        <div style={{ marginBottom: "12px" }}>
          <label style={{ display: "block", marginBottom: "4px", fontWeight: 500 }}>
            Description
          </label>
          <textarea
            value={formData.description || ""}
            onChange={(e) => handleInputChange("description", e.target.value)}
            style={{
              width: "100%",
              padding: "8px",
              border: `1px solid ${getColorHex("border")}`,
              borderRadius: "4px",
              minHeight: "80px",
              fontFamily: "inherit",
            }}
            placeholder="Detailed description of this template..."
          />
        </div>

        <div style={{ marginBottom: "12px" }}>
          <label style={{ display: "block", marginBottom: "4px", fontWeight: 500 }}>
            Category
          </label>
          <select
            value={formData.category || "Custom"}
            onChange={(e) => handleInputChange("category", e.target.value)}
            style={{
              width: "100%",
              padding: "8px",
              border: `1px solid ${getColorHex("border")}`,
              borderRadius: "4px",
            }}
          >
            <option>Standard</option>
            <option>Reinforced</option>
            <option>Large-Assembly</option>
            <option>Custom</option>
          </select>
        </div>
      </div>

      {/* Geometry Constraints Section */}
      <div
        style={{
          marginBottom: "24px",
          padding: "16px",
          backgroundColor: getColorHex("surface"),
          borderRadius: "8px",
          border: `1px solid ${getColorHex("border")}`,
        }}
      >
        <h3 style={{ marginTop: 0, color: getColorHex("text") }}>Geometry Constraints</h3>
        {renderParameterEditor("Outer Diameter (mm)", "outer_diameter", formData.outer_diameter)}
        {renderParameterEditor("Wall Thickness (mm)", "wall_thickness", formData.wall_thickness)}
        {renderParameterEditor("Length (mm)", "length", formData.length)}
      </div>

      {/* Manufacturing Info Section */}
      <div
        style={{
          marginBottom: "24px",
          padding: "16px",
          backgroundColor: getColorHex("surface"),
          borderRadius: "8px",
          border: `1px solid ${getColorHex("border")}`,
        }}
      >
        <h3 style={{ marginTop: 0, color: getColorHex("text") }}>Manufacturing Information</h3>

        <div style={{ marginBottom: "12px" }}>
          <label style={{ display: "block", marginBottom: "4px", fontWeight: 500 }}>
            Manufacturing Method
          </label>
          <input
            type="text"
            value={formData.use_case || ""}
            onChange={(e) => handleInputChange("use_case", e.target.value)}
            placeholder="e.g., 3D CNC bending"
            style={{
              width: "100%",
              padding: "8px",
              border: `1px solid ${getColorHex("border")}`,
              borderRadius: "4px",
            }}
          />
        </div>

        <div style={{ marginBottom: "12px" }}>
          <label style={{ display: "block", marginBottom: "4px", fontWeight: 500 }}>
            Compatible Materials
          </label>
          <textarea
            value={(formData.compatible_materials || []).join(", ")}
            onChange={(e) =>
              handleInputChange(
                "compatible_materials",
                e.target.value.split(",").map((s) => s.trim())
              )
            }
            style={{
              width: "100%",
              padding: "8px",
              border: `1px solid ${getColorHex("border")}`,
              borderRadius: "4px",
              minHeight: "60px",
              fontFamily: "monospace",
              fontSize: "12px",
            }}
            placeholder="Comma-separated list: steel-1020, aluminum-6061, ..."
          />
        </div>
      </div>

      {/* Action Buttons */}
      <div style={{ display: "flex", gap: "8px", justifyContent: "flex-end" }}>
        <button
          onClick={onCancel}
          style={{
            padding: "8px 16px",
            backgroundColor: getColorHex("surface"),
            color: getColorHex("text"),
            border: `1px solid ${getColorHex("border")}`,
            borderRadius: "4px",
            cursor: "pointer",
            fontWeight: 500,
          }}
          onMouseOver={(e) => {
            (e.currentTarget as HTMLButtonElement).style.backgroundColor = getColorHex("hover");
          }}
          onMouseOut={(e) => {
            (e.currentTarget as HTMLButtonElement).style.backgroundColor = getColorHex("surface");
          }}
        >
          Cancel
        </button>
        <button
          onClick={handleSave}
          style={{
            padding: "8px 16px",
            backgroundColor: getColorHex("primary"),
            color: "#fff",
            border: "none",
            borderRadius: "4px",
            cursor: "pointer",
            fontWeight: 500,
          }}
          onMouseOver={(e) => {
            (e.currentTarget as HTMLButtonElement).style.opacity = "0.9";
          }}
          onMouseOut={(e) => {
            (e.currentTarget as HTMLButtonElement).style.opacity = "1";
          }}
        >
          Save Template
        </button>
      </div>
    </div>
  );
};
