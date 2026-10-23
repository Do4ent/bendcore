/**
 * BendCore Pattern Templates Component
 * Parametric tube forms for common bending patterns
 *
 * 6 Standard patterns:
 * 1. 90° Angle
 * 2. Offset (Duck/Dogleg)
 * 3. Saddle (Obstacle Avoidance)
 * 4. U-Shape (Loop/Hairpin)
 * 5. Z-Shape
 * 6. Wall Penetration
 */

import React, { useState, useCallback } from "react";
import "./PatternTemplates.css";

/**
 * Pattern parameter definitions
 */
interface PatternParams {
  [key: string]: {
    value: number;
    min: number;
    max: number;
    step: number;
    unit: string;
    label: string;
  };
}

/**
 * Pattern template definition
 */
interface PatternTemplate {
  id: string;
  name: string;
  description: string;
  segments: number;
  params: PatternParams;
  icon: string;
  generateVertices: (params: PatternParams) => [number, number, number][];
  preview: string;
}

/**
 * All 6 pattern templates
 */
const PATTERN_TEMPLATES: Record<string, PatternTemplate> = {
  angle_90: {
    id: "angle_90",
    name: "90° Angle",
    description: "Simple 90-degree bend with straight sections",
    segments: 2,
    params: {},
    icon: "⊢",
    generateVertices: () => {
      // 0° -> 90° -> 0°
      // Returns 3 points: start, apex, end
      return [
        [0, 0, 0],         // Start point
        [50, 0, 0],        // Horizontal straight
        [50, 50, 0],       // 90° bend point
        [50, 100, 0],      // Vertical straight
      ];
    },
    preview: "Simple 90° corner bend",
  },

  offset: {
    id: "offset",
    name: "Offset (Duck/Dogleg)",
    description: "Parallel offset path - dodge around obstacles",
    segments: 3,
    params: {
      offset_distance: {
        value: 25,
        min: 5,
        max: 200,
        step: 5,
        unit: "mm",
        label: "Offset Distance",
      },
      offset_height: {
        value: 50,
        min: 10,
        max: 300,
        step: 5,
        unit: "mm",
        label: "Height",
      },
    },
    icon: "⇄",
    generateVertices: (params) => {
      const offsetDist = params.offset_distance?.value || 25;
      const offsetHeight = params.offset_height?.value || 50;
      // Returns path that offsets and returns to original line
      // Two 45° bends + intermediate straight section
      return [
        [0, 0, 0],                          // Start
        [offsetHeight / 2, offsetHeight / 2, 0],  // 45° bend up
        [offsetHeight / 2 + offsetDist, offsetHeight / 2, 0],  // Offset horizontal
        [offsetHeight + offsetDist, 0, 0],  // 45° bend down
        [offsetHeight + offsetDist, 100, 0],  // Continue straight
      ];
    },
    preview: "Offset path with 45° bends",
  },

  saddle: {
    id: "saddle",
    name: "Saddle (Obstacle Avoidance)",
    description: "Bypass around central obstacle with symmetric bends",
    segments: 4,
    params: {
      base_length: {
        value: 80,
        min: 20,
        max: 300,
        step: 5,
        unit: "mm",
        label: "Base Length",
      },
      obstacle_height: {
        value: 40,
        min: 10,
        max: 200,
        step: 5,
        unit: "mm",
        label: "Obstacle Height",
      },
    },
    icon: "⌢",
    generateVertices: (params) => {
      const baseLen = params.base_length?.value || 80;
      const obstacleHt = params.obstacle_height?.value || 40;
      // Two 45° bends on same plane, forming arc around obstacle
      const halfBase = baseLen / 2;
      return [
        [0, 0, 0],                    // Start
        [halfBase / 2, obstacleHt / 2, 0],   // 45° up-right
        [halfBase, obstacleHt, 0],          // Peak
        [halfBase + halfBase / 2, obstacleHt / 2, 0],  // 45° down-right
        [baseLen, 0, 0],              // End on original line
      ];
    },
    preview: "Symmetric arc over obstacle",
  },

  u_shape: {
    id: "u_shape",
    name: "U-Shape (Loop/Hairpin)",
    description: "Return path - both ends parallel after 180° bend",
    segments: 3,
    params: {
      length: {
        value: 100,
        min: 30,
        max: 400,
        step: 5,
        unit: "mm",
        label: "Length",
      },
      distance_between_ends: {
        value: 60,
        min: 20,
        max: 300,
        step: 5,
        unit: "mm",
        label: "Distance Between Ends",
      },
    },
    icon: "U",
    generateVertices: (params) => {
      const len = params.length?.value || 100;
      const dist = params.distance_between_ends?.value || 60;
      // Straight up, 180° bend, straight down (parallel to start)
      return [
        [0, 0, 0],              // Start point
        [0, len / 2, 0],        // Straight up
        [dist / 2, len / 2 + dist / 2, 0],  // 180° bend (top)
        [dist, len / 2, 0],     // Straight down (parallel, offset)
        [dist, 0, 0],           // End point
      ];
    },
    preview: "180° return bend (hairpin loop)",
  },

  z_shape: {
    id: "z_shape",
    name: "Z-Shape",
    description: "3D displacement in X, Y, and Z directions",
    segments: 4,
    params: {
      offset_xy: {
        value: 50,
        min: 10,
        max: 300,
        step: 5,
        unit: "mm",
        label: "XY Offset",
      },
      offset_z: {
        value: 30,
        min: 10,
        max: 200,
        step: 5,
        unit: "mm",
        label: "Z Offset",
      },
    },
    icon: "Z",
    generateVertices: (params) => {
      const offsetXY = params.offset_xy?.value || 50;
      const offsetZ = params.offset_z?.value || 30;
      // Four perpendicular offsets in different planes
      return [
        [0, 0, 0],                            // Start
        [offsetXY / 2, 0, 0],                // Horizontal X
        [offsetXY, 0, offsetZ / 2],          // Diagonal (XZ plane)
        [offsetXY, offsetXY / 2, offsetZ],   // Diagonal (YZ plane)
        [offsetXY, offsetXY, offsetZ],       // Final position
        [offsetXY, offsetXY, offsetZ + 100], // Continue vertical
      ];
    },
    preview: "3D S-curve displacement",
  },

  wall_penetration: {
    id: "wall_penetration",
    name: "Wall Penetration",
    description: "Exit perpendicular to wall surface",
    segments: 2,
    params: {
      wall_thickness: {
        value: 10,
        min: 2,
        max: 100,
        step: 1,
        unit: "mm",
        label: "Wall Thickness",
      },
      exit_angle: {
        value: 90,
        min: 45,
        max: 180,
        step: 5,
        unit: "degrees",
        label: "Exit Angle",
      },
    },
    icon: "↗",
    generateVertices: (params) => {
      const wallThickness = params.wall_thickness?.value || 10;
      const exitAngle = (params.exit_angle?.value || 90) * (Math.PI / 180);
      // Perpendicular approach to wall, exit at angle
      const exitX = Math.cos(exitAngle) * wallThickness;
      const exitZ = Math.sin(exitAngle) * wallThickness;
      return [
        [0, 0, 0],              // Approach from front
        [0, 0, wallThickness],  // Wall entrance
        [exitX, 0, wallThickness + exitZ],  // Exit at angle
        [exitX * 2, 0, wallThickness + exitZ * 2],  // Continue
      ];
    },
    preview: "Perpendicular wall penetration",
  },
};

/**
 * Pattern preview component
 */
interface PatternPreviewProps {
  vertices: [number, number, number][];
  template: PatternTemplate;
}

const PatternPreview: React.FC<PatternPreviewProps> = ({ vertices, template }) => {
  if (!vertices || vertices.length < 2) {
    return <div className="pattern-preview-empty">No preview available</div>;
  }

  // Simple 2D projection of 3D path
  const minX = Math.min(...vertices.map((v) => v[0]));
  const maxX = Math.max(...vertices.map((v) => v[0]));
  const minY = Math.min(...vertices.map((v) => v[1]));
  const maxY = Math.max(...vertices.map((v) => v[1]));
  const minZ = Math.min(...vertices.map((v) => v[2]));
  const maxZ = Math.max(...vertices.map((v) => v[2]));

  const rangeX = maxX - minX || 1;
  const rangeY = maxY - minY || 1;
  const rangeZ = maxZ - minZ || 1;
  const maxRange = Math.max(rangeX, rangeY, rangeZ) || 100;

  // Scale to fit in preview (200x150)
  const scale = 200 / maxRange;
  const offsetX = 20 - minX * scale;
  const offsetY = 15 - minY * scale;
  const offsetZ = (minZ * scale) / 2; // 3D perspective

  // Project 3D to 2D isometric
  const projectPoint = (v: [number, number, number]): [number, number] => {
    const x = (v[0] - minX) * scale + offsetX;
    const y = (v[1] - minY) * scale + offsetY;
    const z = (v[2] - minZ) * scale;
    // Isometric projection
    return [x + z * 0.3, y + z * 0.3];
  };

  const points = vertices.map(projectPoint);
  const pathData = points.map((p, i) => `${i === 0 ? "M" : "L"} ${p[0]} ${p[1]}`).join(" ");

  return (
    <svg className="pattern-preview-svg" viewBox="0 0 240 180">
      <rect width="240" height="180" fill="transparent" />
      <path d={pathData} stroke="rgb(100, 150, 255)" strokeWidth="2" fill="none" />
      {points.map((p, i) => (
        <circle key={`pt-${i}`} cx={p[0]} cy={p[1]} r="3" fill="rgb(50, 100, 200)" />
      ))}
      <text x="120" y="170" textAnchor="middle" fontSize="12" fill="#666">
        {template.preview}
      </text>
    </svg>
  );
};

/**
 * Pattern selector and parameter editor
 */
interface PatternTemplatesProps {
  onPatternSelect: (pattern: PatternTemplate, params: PatternParams) => void;
  onCancel?: () => void;
}

export const PatternTemplates: React.FC<PatternTemplatesProps> = ({
  onPatternSelect,
  onCancel,
}) => {
  const [selectedPattern, setSelectedPattern] = useState<string>("angle_90");
  const [params, setParams] = useState<PatternParams>(
    PATTERN_TEMPLATES.angle_90.params
  );
  const [previewVertices, setPreviewVertices] = useState<[number, number, number][]>(
    PATTERN_TEMPLATES.angle_90.generateVertices({})
  );

  const template = PATTERN_TEMPLATES[selectedPattern];

  const handlePatternChange = useCallback((patternId: string) => {
    const newTemplate = PATTERN_TEMPLATES[patternId];
    setSelectedPattern(patternId);
    setParams(JSON.parse(JSON.stringify(newTemplate.params))); // Deep copy
    setPreviewVertices(newTemplate.generateVertices(newTemplate.params));
  }, []);

  const handleParamChange = useCallback(
    (paramKey: string, value: number) => {
      const newParams = { ...params };
      newParams[paramKey] = { ...newParams[paramKey], value };
      setParams(newParams);
      setPreviewVertices(template.generateVertices(newParams));
    },
    [params, template]
  );

  const handleCreate = useCallback(() => {
    onPatternSelect(template, params);
  }, [template, params, onPatternSelect]);

  return (
    <div className="pattern-templates">
      <div className="pattern-templates-header">
        <h2>Parametric Tube Patterns</h2>
        <p>Select a standard pattern and customize parameters</p>
      </div>

      <div className="pattern-templates-content">
        {/* Pattern selector */}
        <div className="pattern-selector">
          <div className="pattern-grid">
            {Object.values(PATTERN_TEMPLATES).map((p) => (
              <div
                key={p.id}
                className={`pattern-item ${selectedPattern === p.id ? "selected" : ""}`}
                onClick={() => handlePatternChange(p.id)}
              >
                <div className="pattern-icon">{p.icon}</div>
                <div className="pattern-name">{p.name}</div>
                <div className="pattern-desc">{p.description}</div>
              </div>
            ))}
          </div>
        </div>

        {/* Parameters and preview */}
        <div className="pattern-editor">
          <div className="pattern-preview-section">
            <h3>Preview</h3>
            <PatternPreview vertices={previewVertices} template={template} />
          </div>

          <div className="pattern-params-section">
            <h3>Parameters</h3>
            {Object.keys(params).length === 0 ? (
              <p className="no-params">No parameters needed</p>
            ) : (
              <div className="params-list">
                {Object.entries(params).map(([key, param]) => (
                  <div key={key} className="param-control">
                    <label>{param.label}</label>
                    <div className="param-input-group">
                      <input
                        type="range"
                        min={param.min}
                        max={param.max}
                        step={param.step}
                        value={param.value}
                        onChange={(e) =>
                          handleParamChange(key, parseFloat(e.target.value))
                        }
                        className="param-slider"
                      />
                      <input
                        type="number"
                        min={param.min}
                        max={param.max}
                        step={param.step}
                        value={param.value}
                        onChange={(e) =>
                          handleParamChange(key, parseFloat(e.target.value))
                        }
                        className="param-number"
                      />
                      <span className="param-unit">{param.unit}</span>
                    </div>
                  </div>
                ))}
              </div>
            )}
          </div>

          <div className="pattern-info-section">
            <h3>Pattern Info</h3>
            <div className="info-grid">
              <div className="info-item">
                <span className="label">Segments:</span>
                <span className="value">{template.segments}</span>
              </div>
              <div className="info-item">
                <span className="label">Bend Points:</span>
                <span className="value">{previewVertices.length - 1}</span>
              </div>
            </div>
          </div>
        </div>
      </div>

      {/* Action buttons */}
      <div className="pattern-templates-footer">
        <button className="btn-create" onClick={handleCreate}>
          Create Tube from Pattern
        </button>
        {onCancel && (
          <button className="btn-cancel" onClick={onCancel}>
            Cancel
          </button>
        )}
      </div>
    </div>
  );
};

/**
 * Export pattern template definitions for use in C++ layer
 */
export function getPatternTemplate(
  patternId: string
): PatternTemplate | undefined {
  return PATTERN_TEMPLATES[patternId];
}

export function listPatternTemplates(): PatternTemplate[] {
  return Object.values(PATTERN_TEMPLATES);
}

export function generatePatternVertices(
  patternId: string,
  params: PatternParams
): [number, number, number][] | null {
  const template = PATTERN_TEMPLATES[patternId];
  if (!template) return null;
  return template.generateVertices(params);
}

export default PatternTemplates;
