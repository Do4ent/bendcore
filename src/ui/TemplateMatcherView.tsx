/**
 * Template Matcher Component
 * Display template matching results with confidence scores and recommendations
 * Task 2.3: React UI Components (Subtask 3)
 */

import React, { useState, useCallback, useEffect } from "react";
import { useTemplates, TemplateMatch } from "./hooks/useTemplates";
import { getColorHex } from "./ColorScheme";

interface TemplateMatcherViewProps {
  outerDiameter?: number;
  wallThickness?: number;
  bendAngle?: number;
  bendRadius?: number;
  bendSpacing?: number;
  onSelectTemplate?: (templateId: string) => void;
}

const getConfidenceColor = (confidence: number): string => {
  if (confidence >= 85) return "#22c55e"; // green
  if (confidence >= 75) return "#f59e0b"; // amber
  return "#ef4444"; // red
};

const getConfidenceLabel = (confidence: number): string => {
  if (confidence >= 95) return "Perfect Match";
  if (confidence >= 85) return "Good Match";
  if (confidence >= 75) return "Acceptable";
  return "Poor Match";
};

export const TemplateMatcherView: React.FC<TemplateMatcherViewProps> = ({
  outerDiameter = 25,
  wallThickness = 2,
  bendAngle = 90,
  bendRadius = 2,
  bendSpacing = 20,
  onSelectTemplate,
}) => {
  const { library } = useTemplates();
  const [matches, setMatches] = useState<TemplateMatch[]>([]);
  const [selectedTemplate, setSelectedTemplate] = useState<string | null>(null);

  // Calculate matches when parameters change
  useEffect(() => {
    if (library.templates.length === 0) {
      setMatches([]);
      return;
    }

    const calculatedMatches = library.templates
      .map((template) => {
        // Simple scoring (OD=30%, Wall=20%, Bend=25%, Radius=15%, Spacing=10%)
        const odScore = calculateParameterScore(outerDiameter, template.outer_diameter) * 0.3;
        const wallScore = calculateParameterScore(wallThickness, template.wall_thickness) * 0.2;
        const bendScore =
          (bendAngle <= 90 ? 1.0 : Math.max(0, 1.0 - (bendAngle - 90) / 90)) * 0.25;
        const radiusScore = (bendRadius >= 2 ? 1.0 : bendRadius / 2) * 0.15;
        const spacingScore = (bendSpacing >= 15 ? 1.0 : bendSpacing / 15) * 0.1;

        const totalScore = (odScore + wallScore + bendScore + radiusScore + spacingScore) * 100;

        return {
          template_id: template.id,
          template_name: template.name,
          confidence: Math.min(100, Math.max(0, totalScore)),
        };
      })
      .sort((a, b) => b.confidence - a.confidence)
      .slice(0, 5);

    setMatches(calculatedMatches);
  }, [library.templates, outerDiameter, wallThickness, bendAngle, bendRadius, bendSpacing]);

  const handleSelectTemplate = useCallback(
    (templateId: string) => {
      setSelectedTemplate(templateId);
      onSelectTemplate?.(templateId);
    },
    [onSelectTemplate]
  );

  if (matches.length === 0) {
    return (
      <div
        style={{
          padding: "16px",
          backgroundColor: getColorHex("surface"),
          borderRadius: "8px",
          border: `1px solid ${getColorHex("border")}`,
          textAlign: "center",
          color: getColorHex("text"),
        }}
      >
        <p>No templates available for matching</p>
      </div>
    );
  }

  return (
    <div
      style={{
        padding: "0",
        backgroundColor: getColorHex("surface"),
        borderRadius: "8px",
        border: `1px solid ${getColorHex("border")}`,
        overflow: "hidden",
      }}
    >
      {/* Table Header */}
      <div
        style={{
          display: "grid",
          gridTemplateColumns: "2fr 1fr 1fr 1fr",
          gap: "0",
          padding: "12px 16px",
          backgroundColor: getColorHex("background"),
          borderBottom: `1px solid ${getColorHex("border")}`,
          fontWeight: 600,
          fontSize: "12px",
          color: getColorHex("textSecondary"),
        }}
      >
        <div>Template</div>
        <div style={{ textAlign: "center" }}>Confidence</div>
        <div style={{ textAlign: "center" }}>Match Quality</div>
        <div style={{ textAlign: "right" }}>Action</div>
      </div>

      {/* Match Results */}
      {matches.map((match, idx) => (
        <div
          key={match.template_id}
          style={{
            borderBottom:
              idx < matches.length - 1 ? `1px solid ${getColorHex("border")}` : "none",
          }}
        >
          <div
            style={{
              display: "grid",
              gridTemplateColumns: "2fr 1fr 1fr 1fr",
              gap: "0",
              padding: "12px 16px",
              alignItems: "center",
              backgroundColor:
                selectedTemplate === match.template_id ? getColorHex("hover") : "transparent",
            }}
          >
            {/* Template Name */}
            <div>
              <div style={{ fontWeight: 500, color: getColorHex("text") }}>
                {match.template_name}
              </div>
              <div
                style={{
                  fontSize: "11px",
                  color: getColorHex("textSecondary"),
                  marginTop: "2px",
                }}
              >
                ID: {match.template_id}
              </div>
            </div>

            {/* Confidence Score */}
            <div style={{ textAlign: "center" }}>
              <div
                style={{
                  fontSize: "18px",
                  fontWeight: 700,
                  color: getConfidenceColor(match.confidence),
                }}
              >
                {match.confidence.toFixed(0)}%
              </div>
            </div>

            {/* Match Quality */}
            <div style={{ textAlign: "center" }}>
              <div
                style={{
                  display: "inline-block",
                  padding: "4px 8px",
                  backgroundColor: getConfidenceColor(match.confidence),
                  color: "#fff",
                  borderRadius: "4px",
                  fontSize: "11px",
                  fontWeight: 500,
                }}
              >
                {getConfidenceLabel(match.confidence)}
              </div>
            </div>

            {/* Select Button */}
            <div style={{ textAlign: "right" }}>
              <button
                onClick={() => handleSelectTemplate(match.template_id)}
                style={{
                  padding: "6px 12px",
                  backgroundColor:
                    selectedTemplate === match.template_id
                      ? getColorHex("primary")
                      : getColorHex("surface"),
                  color:
                    selectedTemplate === match.template_id ? "#fff" : getColorHex("text"),
                  border: `1px solid ${
                    selectedTemplate === match.template_id
                      ? getColorHex("primary")
                      : getColorHex("border")
                  }`,
                  borderRadius: "4px",
                  cursor: "pointer",
                  fontSize: "12px",
                  fontWeight: 500,
                  transition: "all 0.2s",
                }}
                onMouseOver={(e) => {
                  if (selectedTemplate !== match.template_id) {
                    (e.currentTarget as HTMLButtonElement).style.backgroundColor =
                      getColorHex("hover");
                  }
                }}
                onMouseOut={(e) => {
                  if (selectedTemplate !== match.template_id) {
                    (e.currentTarget as HTMLButtonElement).style.backgroundColor =
                      getColorHex("surface");
                  }
                }}
              >
                {selectedTemplate === match.template_id ? "Selected" : "Select"}
              </button>
            </div>
          </div>
        </div>
      ))}

      {/* Info Footer */}
      <div
        style={{
          padding: "12px 16px",
          backgroundColor: getColorHex("background"),
          borderTop: `1px solid ${getColorHex("border")}`,
          fontSize: "11px",
          color: getColorHex("textSecondary"),
          display: "flex",
          gap: "16px",
        }}
      >
        <div>
          <strong>OD:</strong> {outerDiameter} mm
        </div>
        <div>
          <strong>Wall:</strong> {wallThickness} mm
        </div>
        <div>
          <strong>Bend:</strong> {bendAngle}°
        </div>
        <div>
          <strong>Radius:</strong> {bendRadius} (R/D)
        </div>
        <div>
          <strong>Spacing:</strong> {bendSpacing} mm
        </div>
      </div>
    </div>
  );
};

// ============ HELPER FUNCTIONS ============

function calculateParameterScore(value: number, param: any): number {
  if (value >= param.min && value <= param.max) {
    return 1.0;
  }

  const distance = value < param.min ? param.min - value : value - param.max;
  const range = param.max - param.min;
  if (range === 0) return 0.0;

  return Math.max(0, 1.0 - distance / range);
}
