/**
 * Tube Element Configuration Dialog
 * Complete 5-tab dialog for tube design with real-time validation and preview
 * Phase 29 Day 6: Complete implementation
 */

import React, { useState, useCallback, useMemo, useEffect } from "react";
import { getColorHex } from "./ColorScheme";
import { useTemplates, TemplateMatch } from "./hooks/useTemplates";
import { TemplateSuggestionsTab } from "./TemplateSuggestionsTab";
import OffsetCalculatorTab from "./OffsetCalculatorTab";

// ============ INTERFACES ============

interface GeometryParams {
  outerDiameter: number;
  wallThickness: number;
  length: number;
  material: string;  // Material ID from database (e.g., "steel-1020")
  materialName?: string;  // Display name (e.g., "Carbon Steel 1020")
}

interface BendParams {
  id: string;
  position: number;
  radius: number;
  angle: number;
}

interface PortParams {
  id: string;
  name: string;
  position: number;
  type: "Connection" | "Drain" | "Vent";
  diameter: number;
}

interface AdvancedParams {
  springbackFactor: number;
  bendCompensation: boolean;
  lengthTolerance: number;
  angleTolerance: number;
  surfaceFinish: "Raw" | "Polished" | "Powder-coated";
}

export interface TubeConfig {
  geometry: GeometryParams;
  bends: BendParams[];
  ports: PortParams[];
  advanced: AdvancedParams;
}

interface TubeDialogProps {
  isOpen: boolean;
  onClose: () => void;
  onSave: (config: TubeConfig) => void;
  initialConfig?: TubeConfig;
}

interface ValidationError {
  field: string;
  message: string;
}

interface MaterialDensity {
  name: string;
  density: number; // g/cm³
}

// ============ CONSTANTS ============

const MATERIAL_DENSITIES: Record<string, number> = {
  Steel: 7.85,
  Aluminum: 2.70,
  Copper: 8.96,
  Titanium: 4.54,
  Custom: 5.0,
};

// Extended material density lookup for new materials database
const EXTENDED_MATERIAL_DENSITIES: Record<string, number> = {
  // Legacy material names
  "Steel": 7.85,
  "Aluminum": 2.70,
  "Copper": 8.96,
  "Titanium": 4.54,
  "Custom": 5.0,
  // New material IDs - Carbon Steel
  "steel-1020": 7.85,
  "steel-1045": 7.85,
  "steel-1060": 7.85,
  "steel-ck45": 7.85,
  "steel-1018": 7.85,
  "steel-4140": 7.85,
  "steel-4340": 7.85,
  "steel-9310": 7.85,
  // Stainless Steel
  "ss-304": 8.0,
  "ss-304l": 8.0,
  "ss-316": 8.0,
  "ss-316l": 8.0,
  "ss-430": 7.73,
  "ss-174ph": 7.8,
  "ss-630": 7.8,
  "ss-15-5ph": 7.8,
  // Aluminum
  "aluminum-6061": 2.70,
  "aluminum-6061-t6": 2.70,
  "aluminum-7075": 2.81,
  "aluminum-7075-t6": 2.81,
  "aluminum-5083": 2.66,
  "aluminum-5754": 2.66,
  // Copper
  "copper-pure": 8.96,
  "brass-7030": 8.47,
  "bronze-cu-sn": 8.75,
  "cupro-nickel-9010": 8.94,
  // Titanium
  "titanium-grade2": 4.51,
  "titanium-grade5": 4.54,
  "titanium-grade23": 4.56,
  // Nickel Alloy
  "inconel-625": 8.44,
  "hastelloy-c276": 8.89,
  "monel-400": 8.80,
  // Custom
  "custom": 5.0,
};

// Helper function to get material density
const getMaterialDensity = (materialId: string): number => {
  return EXTENDED_MATERIAL_DENSITIES[materialId] || 5.0;
};

// Helper function to get material display name
const getMaterialDisplayName = (materialId: string): string => {
  const names: Record<string, string> = {
    "steel-1020": "Carbon Steel 1020",
    "steel-1045": "Carbon Steel 1045",
    "steel-1060": "Carbon Steel 1060",
    "steel-ck45": "Carbon Steel CK45",
    "steel-1018": "Carbon Steel 1018",
    "steel-4140": "Alloy Steel 4140",
    "steel-4340": "Alloy Steel 4340",
    "steel-9310": "Alloy Steel 9310",
    "ss-304": "Stainless Steel 304",
    "ss-304l": "Stainless Steel 304L",
    "ss-316": "Stainless Steel 316",
    "ss-316l": "Stainless Steel 316L",
    "ss-430": "Stainless Steel 430",
    "ss-174ph": "Stainless Steel 17-4 PH",
    "ss-630": "Stainless Steel 17-7 PH",
    "ss-15-5ph": "Stainless Steel 15-5 PH",
    "aluminum-6061": "Aluminum 6061",
    "aluminum-6061-t6": "Aluminum 6061-T6",
    "aluminum-7075": "Aluminum 7075",
    "aluminum-7075-t6": "Aluminum 7075-T6",
    "aluminum-5083": "Aluminum 5083",
    "aluminum-5754": "Aluminum 5754",
    "copper-pure": "Pure Copper",
    "brass-7030": "Brass 70/30",
    "bronze-cu-sn": "Bronze Cu-Sn",
    "cupro-nickel-9010": "Cupro-Nickel 90/10",
    "titanium-grade2": "Titanium Grade 2",
    "titanium-grade5": "Titanium Grade 5",
    "titanium-grade23": "Titanium Grade 23",
    "inconel-625": "Inconel 625",
    "hastelloy-c276": "Hastelloy C-276",
    "monel-400": "Monel 400",
    "custom": "Custom Material",
    "Steel": "Steel",
    "Aluminum": "Aluminum",
    "Copper": "Copper",
    "Titanium": "Titanium",
    "Custom": "Custom",
  };
  return names[materialId] || materialId;
};

const DEFAULT_CONFIG: TubeConfig = {
  geometry: {
    outerDiameter: 0,
    wallThickness: 1.5,
    length: 1000,
    material: "steel-1020",
    materialName: "Carbon Steel 1020",
  },
  bends: [],
  ports: [],
  advanced: {
    springbackFactor: 0.3,
    bendCompensation: false,
    lengthTolerance: 2.0,
    angleTolerance: 0.5,
    surfaceFinish: "Raw",
  },
};

// ============ GEOMETRY TAB COMPONENT ============

interface GeometryTabProps {
  config: TubeConfig;
  onChange: (config: TubeConfig) => void;
  errors: ValidationError[];
  calculatedMass: number | null;
  onTemplateSelected?: (templateId: string) => void;
}

const GeometryTab: React.FC<GeometryTabProps> = ({
  config,
  onChange,
  errors,
  calculatedMass,
  onTemplateSelected,
}) => {
  const geometry = config.geometry;
  const { selectTemplates, getTemplate } = useTemplates();
  const [suggestedTemplates, setSuggestedTemplates] = useState<TemplateMatch[]>([]);

  // Update suggested templates when geometry changes
  useEffect(() => {
    const suggestions = selectTemplates(
      geometry.outerDiameter,
      geometry.wallThickness,
      geometry.length
    );
    setSuggestedTemplates(suggestions);
  }, [geometry.outerDiameter, geometry.wallThickness, geometry.length, selectTemplates]);

  const handleChange = useCallback(
    (field: keyof GeometryParams, value: any) => {
      onChange({
        ...config,
        geometry: { ...geometry, [field]: value },
      });
    },
    [config, onChange, geometry]
  );

  const handleApplyTemplate = useCallback(
    (templateId: string) => {
      const template = getTemplate(templateId);
      if (template) {
        // Apply template defaults to geometry
        onChange({
          ...config,
          geometry: {
            ...geometry,
            outerDiameter: template.outer_diameter.default,
            wallThickness: template.wall_thickness.default,
            length: template.length.default,
          },
        });
        if (onTemplateSelected) {
          onTemplateSelected(templateId);
        }
      }
    },
    [config, geometry, onChange, getTemplate, onTemplateSelected]
  );

  const getFieldError = (field: string) =>
    errors.find((e) => e.field === field)?.message || "";

  const hasFieldError = (field: string) => getFieldError(field) !== "";

  // Calculate cross-section area
  const crossSectionArea = useMemo(() => {
    const od = geometry.outerDiameter;
    const wt = geometry.wallThickness;
    const id = od - 2 * wt;
    return Math.PI * (od * od - id * id) / 4;
  }, [geometry.outerDiameter, geometry.wallThickness]);

  const density = getMaterialDensity(geometry.material);

  return (
    <div className="space-y-4">
      {/* Quick Select Template */}
      {suggestedTemplates.length > 0 && (
        <div className="bg-blue-50 border border-blue-300 rounded p-4">
          <h4 className="text-sm font-semibold text-gray-800 mb-3">
            Quick Select Template
          </h4>
          <div className="space-y-2">
            {suggestedTemplates.map((match) => (
              <button
                key={match.template_id}
                onClick={() => handleApplyTemplate(match.template_id)}
                className="w-full text-left px-3 py-2 border border-blue-300 rounded hover:bg-blue-100 transition flex justify-between items-center"
              >
                <span className="text-sm font-medium text-gray-800">
                  {match.template_name}
                </span>
                <span className="text-xs font-semibold text-blue-600">
                  {Math.round(match.confidence)}% match
                </span>
              </button>
            ))}
          </div>
        </div>
      )}

      {/* Outer Diameter */}
      <div className="border-l-4 border-ochre-600 pl-3">
        <label htmlFor="outer-diameter" className="block text-sm font-medium text-gray-700 mb-1">
          Outer Diameter (mm)
        </label>
        <div className="flex items-center gap-2">
          <input
            id="outer-diameter"
            type="number"
            min="1"
            max="100"
            step="0.1"
            value={geometry.outerDiameter}
            onChange={(e) => handleChange("outerDiameter", parseFloat(e.target.value))}
            className={`flex-1 px-2 py-1 border rounded text-sm focus:outline-none focus:ring-2 focus:ring-blue-500 ${
              hasFieldError("outerDiameter")
                ? "border-red-500 bg-red-50"
                : "border-gray-300"
            }`}
          />
          <span className="text-gray-500 text-sm">mm</span>
        </div>
        {hasFieldError("outerDiameter") && (
          <p className="text-red-600 text-xs mt-1">{getFieldError("outerDiameter")}</p>
        )}
      </div>

      {/* Wall Thickness */}
      <div className="border-l-4 border-ochre-600 pl-3">
        <label htmlFor="wall-thickness" className="block text-sm font-medium text-gray-700 mb-1">
          Wall Thickness (mm)
        </label>
        <div className="flex items-center gap-2">
          <input
            id="wall-thickness"
            type="number"
            min="0.1"
            max="10"
            step="0.1"
            value={geometry.wallThickness}
            onChange={(e) => handleChange("wallThickness", parseFloat(e.target.value))}
            className={`flex-1 px-2 py-1 border rounded text-sm focus:outline-none focus:ring-2 focus:ring-blue-500 ${
              hasFieldError("wallThickness")
                ? "border-red-500 bg-red-50"
                : "border-gray-300"
            }`}
          />
          <span className="text-gray-500 text-sm">mm</span>
        </div>
        {hasFieldError("wallThickness") && (
          <p className="text-red-600 text-xs mt-1">{getFieldError("wallThickness")}</p>
        )}
      </div>

      {/* Length */}
      <div className="border-l-4 border-ochre-600 pl-3">
        <label htmlFor="tube-length" className="block text-sm font-medium text-gray-700 mb-1">
          Length (mm)
        </label>
        <div className="flex items-center gap-2">
          <input
            id="tube-length"
            type="number"
            min="10"
            max="5000"
            step="1"
            value={geometry.length}
            onChange={(e) => handleChange("length", parseInt(e.target.value))}
            className={`flex-1 px-2 py-1 border rounded text-sm focus:outline-none focus:ring-2 focus:ring-blue-500 ${
              hasFieldError("length") ? "border-red-500 bg-red-50" : "border-gray-300"
            }`}
          />
          <span className="text-gray-500 text-sm">mm</span>
        </div>
        {hasFieldError("length") && (
          <p className="text-red-600 text-xs mt-1">{getFieldError("length")}</p>
        )}
      </div>

      {/* Material */}
      <div className="border-l-4 border-ochre-600 pl-3">
        <label htmlFor="tube-material" className="block text-sm font-medium text-gray-700 mb-1">
          Material
        </label>
        <select
          id="tube-material"
          value={geometry.material}
          onChange={(e) =>
            handleChange(
              "material",
              e.target.value
            )
          }
          className="w-full px-2 py-1 border border-gray-300 rounded text-sm focus:outline-none focus:ring-2 focus:ring-blue-500"
        >
          {/* Common Materials */}
          <optgroup label="Carbon Steel">
            <option value="steel-1020">Carbon Steel 1020</option>
            <option value="steel-1045">Carbon Steel 1045</option>
            <option value="steel-4140">Alloy Steel 4140</option>
          </optgroup>
          <optgroup label="Stainless Steel">
            <option value="ss-304">Stainless Steel 304</option>
            <option value="ss-316">Stainless Steel 316</option>
            <option value="ss-174ph">Stainless Steel 17-4 PH</option>
          </optgroup>
          <optgroup label="Aluminum">
            <option value="aluminum-6061">Aluminum 6061</option>
            <option value="aluminum-6061-t6">Aluminum 6061-T6</option>
            <option value="aluminum-7075">Aluminum 7075</option>
          </optgroup>
          <optgroup label="Other Metals">
            <option value="copper-pure">Pure Copper</option>
            <option value="titanium-grade5">Titanium Grade 5</option>
            <option value="brass-7030">Brass 70/30</option>
          </optgroup>
          {/* Legacy options for backward compatibility */}
          <optgroup label="Legacy">
            <option value="Steel">Steel (Legacy)</option>
            <option value="Aluminum">Aluminum (Legacy)</option>
            <option value="Copper">Copper (Legacy)</option>
            <option value="Titanium">Titanium (Legacy)</option>
            <option value="Custom">Custom</option>
          </optgroup>
        </select>
        <p className="text-xs text-gray-500 mt-1">
          Selected: {geometry.materialName || getMaterialDisplayName(geometry.material)}
        </p>
      </div>

      {/* Calculated Properties */}
      <div className="bg-blue-50 border border-blue-200 rounded p-3 mt-6">
        <h4 className="text-sm font-semibold text-gray-800 mb-3">
          Calculated Properties
        </h4>
        <div className="space-y-2 text-xs">
          <div className="flex justify-between py-1">
            <span className="text-gray-700">Cross-section Area:</span>
            <span className="text-gray-900 font-medium">
              {crossSectionArea.toFixed(2)} mm²
            </span>
          </div>
          <div className="flex justify-between py-1">
            <span className="text-gray-700">Material Density:</span>
            <span className="text-gray-900 font-medium">
              {density.toFixed(2)} g/cm³
            </span>
          </div>
          <div className="flex justify-between py-1">
            <span className="text-gray-700">Calculated Mass:</span>
            <span className="text-gray-900 font-medium">
              {calculatedMass !== null ? calculatedMass.toFixed(3) : "—"} kg
            </span>
          </div>
        </div>
      </div>
    </div>
  );
};

// ============ BENDS TAB COMPONENT ============

interface BendsTabProps {
  bends: BendParams[];
  tubeLength: number;
  onAdd: () => void;
  onEdit: (index: number, bend: BendParams) => void;
  onDelete: (index: number) => void;
  errors: ValidationError[];
  maxBendRadius?: number;
}

const BendsTab: React.FC<BendsTabProps> = ({
  bends,
  tubeLength,
  onAdd,
  onEdit,
  onDelete,
  errors,
  maxBendRadius,
}) => {
  const [editingIndex, setEditingIndex] = useState<number | null>(null);
  const [editingBend, setEditingBend] = useState<BendParams | null>(null);

  const startEdit = (index: number) => {
    setEditingIndex(index);
    setEditingBend({ ...bends[index] });
  };

  const saveEdit = () => {
    if (editingIndex !== null && editingBend) {
      onEdit(editingIndex, editingBend);
      setEditingIndex(null);
      setEditingBend(null);
    }
  };

  const cancelEdit = () => {
    setEditingIndex(null);
    setEditingBend(null);
  };

  const totalBendAngle = bends.reduce((sum, bend) => sum + bend.angle, 0);

  return (
    <div className="space-y-4">
      {/* Bends List */}
      {bends.length === 0 ? (
        <div className="text-center py-6 text-gray-500">
          <p className="text-sm">No bends defined yet</p>
        </div>
      ) : (
        <div className="border border-gray-300 rounded overflow-hidden">
          <table className="w-full text-sm">
            <thead className="bg-gray-100 border-b border-gray-300">
              <tr>
                <th className="px-3 py-2 text-left text-xs font-semibold text-gray-700">
                  Position
                </th>
                <th className="px-3 py-2 text-left text-xs font-semibold text-gray-700">
                  Radius
                </th>
                <th className="px-3 py-2 text-left text-xs font-semibold text-gray-700">
                  Angle
                </th>
                <th className="px-3 py-2 text-center text-xs font-semibold text-gray-700">
                  Actions
                </th>
              </tr>
            </thead>
            <tbody>
              {bends.map((bend, index) => (
                <tr key={bend.id} className="border-t border-gray-200 hover:bg-gray-50">
                  <td className="px-3 py-2 text-gray-900">{bend.position.toFixed(1)} mm</td>
                  <td className="px-3 py-2 text-gray-900">{bend.radius.toFixed(1)} mm</td>
                  <td className="px-3 py-2 text-gray-900">{bend.angle.toFixed(1)}°</td>
                  <td className="px-3 py-2 text-center">
                    <button
                      onClick={() => startEdit(index)}
                      className="px-2 py-1 text-xs text-blue-600 hover:bg-blue-50 rounded mr-2"
                    >
                      Edit
                    </button>
                    <button
                      onClick={() => onDelete(index)}
                      className="px-2 py-1 text-xs text-red-600 hover:bg-red-50 rounded"
                    >
                      Delete
                    </button>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}

      {/* Edit Dialog */}
      {editingIndex !== null && editingBend && (
        <div className="bg-blue-50 border border-blue-300 rounded p-3">
          <h4 className="text-sm font-semibold text-gray-800 mb-3">Edit Bend</h4>
          <div className="space-y-3">
            <div>
              <label className="block text-xs font-medium text-gray-700 mb-1">
                Position (mm)
              </label>
              <input
                type="number"
                min="0"
                max={tubeLength}
                value={editingBend.position}
                onChange={(e) =>
                  setEditingBend({
                    ...editingBend,
                    position: parseFloat(e.target.value),
                  })
                }
                className="w-full px-2 py-1 border border-gray-300 rounded text-sm focus:outline-none focus:ring-2 focus:ring-blue-500"
              />
            </div>
            <div>
              <label className="block text-xs font-medium text-gray-700 mb-1">
                Radius (mm)
              </label>
              <input
                type="number"
                min="10"
                max="500"
                value={editingBend.radius}
                onChange={(e) =>
                  setEditingBend({
                    ...editingBend,
                    radius: parseFloat(e.target.value),
                  })
                }
                className="w-full px-2 py-1 border border-gray-300 rounded text-sm focus:outline-none focus:ring-2 focus:ring-blue-500"
              />
            </div>
            <div>
              <label className="block text-xs font-medium text-gray-700 mb-1">
                Angle (°)
              </label>
              <input
                type="number"
                min="1"
                max="180"
                value={editingBend.angle}
                onChange={(e) =>
                  setEditingBend({
                    ...editingBend,
                    angle: parseFloat(e.target.value),
                  })
                }
                className="w-full px-2 py-1 border border-gray-300 rounded text-sm focus:outline-none focus:ring-2 focus:ring-blue-500"
              />
            </div>
            <div className="flex gap-2 justify-end">
              <button
                onClick={cancelEdit}
                className="px-3 py-1 text-xs border border-gray-300 rounded hover:bg-gray-100"
              >
                Cancel
              </button>
              <button
                onClick={saveEdit}
                className="px-3 py-1 text-xs bg-blue-500 text-white rounded hover:bg-blue-600"
              >
                Save
              </button>
            </div>
          </div>
        </div>
      )}

      {/* Summary */}
      <div className="bg-yellow-50 border border-yellow-200 rounded p-3">
        <div className="flex justify-between text-sm">
          <span className="text-gray-700">Total Bends:</span>
          <span className="text-gray-900 font-medium">{bends.length}</span>
        </div>
        <div className="flex justify-between text-sm mt-1">
          <span className="text-gray-700">Total Angle:</span>
          <span className="text-gray-900 font-medium">{totalBendAngle.toFixed(1)}°</span>
        </div>
      </div>

      {/* Add Button */}
      <button
        onClick={onAdd}
        className="w-full px-3 py-2 bg-green-600 text-white text-sm font-medium rounded hover:bg-green-700 transition"
      >
        + Add New Bend
      </button>
    </div>
  );
};

// ============ PORTS TAB COMPONENT ============

interface PortsTabProps {
  ports: PortParams[];
  tubeLength: number;
  tubeOD: number;
  onAdd: () => void;
  onEdit: (index: number, port: PortParams) => void;
  onDelete: (index: number) => void;
  errors: ValidationError[];
}

const PortsTab: React.FC<PortsTabProps> = ({
  ports,
  tubeLength,
  tubeOD,
  onAdd,
  onEdit,
  onDelete,
  errors,
}) => {
  const [editingIndex, setEditingIndex] = useState<number | null>(null);
  const [editingPort, setEditingPort] = useState<PortParams | null>(null);

  const startEdit = (index: number) => {
    setEditingIndex(index);
    setEditingPort({ ...ports[index] });
  };

  const saveEdit = () => {
    if (editingIndex !== null && editingPort) {
      onEdit(editingIndex, editingPort);
      setEditingIndex(null);
      setEditingPort(null);
    }
  };

  const cancelEdit = () => {
    setEditingIndex(null);
    setEditingPort(null);
  };

  return (
    <div className="space-y-4">
      {/* Ports List */}
      {ports.length === 0 ? (
        <div className="text-center py-6 text-gray-500">
          <p className="text-sm">No ports defined yet</p>
        </div>
      ) : (
        <div className="border border-gray-300 rounded overflow-hidden">
          <table className="w-full text-sm">
            <thead className="bg-gray-100 border-b border-gray-300">
              <tr>
                <th className="px-3 py-2 text-left text-xs font-semibold text-gray-700">
                  Name
                </th>
                <th className="px-3 py-2 text-left text-xs font-semibold text-gray-700">
                  Position
                </th>
                <th className="px-3 py-2 text-left text-xs font-semibold text-gray-700">
                  Type
                </th>
                <th className="px-3 py-2 text-left text-xs font-semibold text-gray-700">
                  Diameter
                </th>
                <th className="px-3 py-2 text-center text-xs font-semibold text-gray-700">
                  Actions
                </th>
              </tr>
            </thead>
            <tbody>
              {ports.map((port, index) => (
                <tr key={port.id} className="border-t border-gray-200 hover:bg-gray-50">
                  <td className="px-3 py-2 text-gray-900">{port.name}</td>
                  <td className="px-3 py-2 text-gray-900">{port.position.toFixed(1)} mm</td>
                  <td className="px-3 py-2 text-gray-900">{port.type}</td>
                  <td className="px-3 py-2 text-gray-900">{port.diameter.toFixed(1)} mm</td>
                  <td className="px-3 py-2 text-center">
                    <button
                      onClick={() => startEdit(index)}
                      className="px-2 py-1 text-xs text-blue-600 hover:bg-blue-50 rounded mr-2"
                    >
                      Edit
                    </button>
                    <button
                      onClick={() => onDelete(index)}
                      className="px-2 py-1 text-xs text-red-600 hover:bg-red-50 rounded"
                    >
                      Delete
                    </button>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}

      {/* Edit Dialog */}
      {editingIndex !== null && editingPort && (
        <div className="bg-blue-50 border border-blue-300 rounded p-3">
          <h4 className="text-sm font-semibold text-gray-800 mb-3">Edit Port</h4>
          <div className="space-y-3">
            <div>
              <label className="block text-xs font-medium text-gray-700 mb-1">
                Port Name
              </label>
              <input
                type="text"
                value={editingPort.name}
                onChange={(e) =>
                  setEditingPort({
                    ...editingPort,
                    name: e.target.value,
                  })
                }
                className="w-full px-2 py-1 border border-gray-300 rounded text-sm focus:outline-none focus:ring-2 focus:ring-blue-500"
              />
            </div>
            <div>
              <label className="block text-xs font-medium text-gray-700 mb-1">
                Position (mm)
              </label>
              <input
                type="number"
                min="0"
                max={tubeLength}
                value={editingPort.position}
                onChange={(e) =>
                  setEditingPort({
                    ...editingPort,
                    position: parseFloat(e.target.value),
                  })
                }
                className="w-full px-2 py-1 border border-gray-300 rounded text-sm focus:outline-none focus:ring-2 focus:ring-blue-500"
              />
            </div>
            <div>
              <label className="block text-xs font-medium text-gray-700 mb-1">
                Type
              </label>
              <select
                value={editingPort.type}
                onChange={(e) =>
                  setEditingPort({
                    ...editingPort,
                    type: e.target.value as PortParams["type"],
                  })
                }
                className="w-full px-2 py-1 border border-gray-300 rounded text-sm focus:outline-none focus:ring-2 focus:ring-blue-500"
              >
                <option value="Connection">Connection</option>
                <option value="Drain">Drain</option>
                <option value="Vent">Vent</option>
              </select>
            </div>
            <div>
              <label className="block text-xs font-medium text-gray-700 mb-1">
                Diameter (mm)
              </label>
              <input
                type="number"
                min="1"
                max={Math.max(tubeOD - 1, 20)}
                value={editingPort.diameter}
                onChange={(e) =>
                  setEditingPort({
                    ...editingPort,
                    diameter: parseFloat(e.target.value),
                  })
                }
                className="w-full px-2 py-1 border border-gray-300 rounded text-sm focus:outline-none focus:ring-2 focus:ring-blue-500"
              />
            </div>
            <div className="flex gap-2 justify-end">
              <button
                onClick={cancelEdit}
                className="px-3 py-1 text-xs border border-gray-300 rounded hover:bg-gray-100"
              >
                Cancel
              </button>
              <button
                onClick={saveEdit}
                className="px-3 py-1 text-xs bg-blue-500 text-white rounded hover:bg-blue-600"
              >
                Save
              </button>
            </div>
          </div>
        </div>
      )}

      {/* Summary */}
      <div className="bg-purple-50 border border-purple-200 rounded p-3">
        <div className="text-sm text-gray-700">
          <span className="font-medium">{ports.length}</span> port
          {ports.length !== 1 ? "s" : ""} defined
        </div>
      </div>

      {/* Add Button */}
      <button
        onClick={onAdd}
        className="w-full px-3 py-2 bg-green-600 text-white text-sm font-medium rounded hover:bg-green-700 transition"
      >
        + Add New Port
      </button>
    </div>
  );
};

// ============ ADVANCED TAB COMPONENT ============

interface AdvancedTabProps {
  config: TubeConfig;
  onChange: (config: TubeConfig) => void;
}

const AdvancedTab: React.FC<AdvancedTabProps> = ({ config, onChange }) => {
  const advanced = config.advanced;

  const handleChange = useCallback(
    (field: keyof AdvancedParams, value: any) => {
      onChange({
        ...config,
        advanced: { ...advanced, [field]: value },
      });
    },
    [config, onChange, advanced]
  );

  const resetDefaults = () => {
    onChange({
      ...config,
      advanced: DEFAULT_CONFIG.advanced,
    });
  };

  return (
    <div className="space-y-4">
      {/* Springback Factor */}
      <div className="border-l-4 border-ochre-600 pl-3">
        <label className="block text-sm font-medium text-gray-700 mb-2">
          Springback Factor
        </label>
        <div className="flex items-center gap-3">
          <input
            type="range"
            min="0"
            max="1"
            step="0.05"
            value={advanced.springbackFactor}
            onChange={(e) =>
              handleChange("springbackFactor", parseFloat(e.target.value))
            }
            className="flex-1 h-2 bg-gray-300 rounded-lg appearance-none cursor-pointer"
          />
          <span className="text-gray-900 font-medium min-w-12">
            {advanced.springbackFactor.toFixed(2)}
          </span>
        </div>
        <p className="text-xs text-gray-600 mt-1">
          0 = No springback, 1 = Maximum springback
        </p>
      </div>

      {/* Bend Compensation */}
      <div className="border-l-4 border-ochre-600 pl-3">
        <label className="flex items-center gap-3 cursor-pointer">
          <input
            type="checkbox"
            checked={advanced.bendCompensation}
            onChange={(e) => handleChange("bendCompensation", e.target.checked)}
            className="w-4 h-4 text-blue-600 rounded focus:ring-2 focus:ring-blue-500"
          />
          <span className="text-sm font-medium text-gray-700">
            Enable Bend Compensation
          </span>
        </label>
        <p className="text-xs text-gray-600 mt-1 ml-7">
          Automatically adjust bend radius to compensate for springback
        </p>
      </div>

      {/* Length Tolerance */}
      <div className="border-l-4 border-ochre-600 pl-3">
        <label className="block text-sm font-medium text-gray-700 mb-1">
          Length Tolerance (±mm)
        </label>
        <div className="flex items-center gap-2">
          <input
            type="number"
            min="0"
            max="10"
            step="0.1"
            value={advanced.lengthTolerance}
            onChange={(e) =>
              handleChange("lengthTolerance", parseFloat(e.target.value))
            }
            className="flex-1 px-2 py-1 border border-gray-300 rounded text-sm focus:outline-none focus:ring-2 focus:ring-blue-500"
          />
          <span className="text-gray-500 text-sm">mm</span>
        </div>
      </div>

      {/* Angle Tolerance */}
      <div className="border-l-4 border-ochre-600 pl-3">
        <label className="block text-sm font-medium text-gray-700 mb-1">
          Angle Tolerance (±°)
        </label>
        <div className="flex items-center gap-2">
          <input
            type="number"
            min="0"
            max="5"
            step="0.1"
            value={advanced.angleTolerance}
            onChange={(e) =>
              handleChange("angleTolerance", parseFloat(e.target.value))
            }
            className="flex-1 px-2 py-1 border border-gray-300 rounded text-sm focus:outline-none focus:ring-2 focus:ring-blue-500"
          />
          <span className="text-gray-500 text-sm">°</span>
        </div>
      </div>

      {/* Surface Finish */}
      <div className="border-l-4 border-ochre-600 pl-3">
        <label className="block text-sm font-medium text-gray-700 mb-1">
          Surface Finish
        </label>
        <select
          value={advanced.surfaceFinish}
          onChange={(e) =>
            handleChange(
              "surfaceFinish",
              e.target.value as typeof advanced.surfaceFinish
            )
          }
          className="w-full px-2 py-1 border border-gray-300 rounded text-sm focus:outline-none focus:ring-2 focus:ring-blue-500"
        >
          <option value="Raw">Raw</option>
          <option value="Polished">Polished</option>
          <option value="Powder-coated">Powder-coated</option>
        </select>
      </div>

      {/* Reset Button */}
      <button
        onClick={resetDefaults}
        className="w-full px-3 py-2 border border-gray-300 text-gray-700 text-sm font-medium rounded hover:bg-gray-100 transition"
      >
        Reset to Defaults
      </button>
    </div>
  );
};

// ============ PREVIEW TAB COMPONENT ============

interface PreviewTabProps {
  config: TubeConfig;
  calculatedMass: number | null;
  isValid: boolean;
  errors: ValidationError[];
}

const PreviewTab: React.FC<PreviewTabProps> = ({
  config,
  calculatedMass,
  isValid,
  errors,
}) => {
  const geometry = config.geometry;
  const density = getMaterialDensity(geometry.material);

  const totalBendAngle = config.bends.reduce((sum, bend) => sum + bend.angle, 0);

  // Determine status
  const hasErrors = errors.length > 0;
  const isReady = !hasErrors && config.bends.length > 0;

  return (
    <div className="space-y-4">
      {/* Tube Configuration */}
      <div className="bg-white border border-gray-300 rounded p-4">
        <h4 className="text-sm font-semibold text-gray-800 mb-3">Tube Configuration</h4>
        <div className="space-y-2 text-sm">
          <div className="flex justify-between">
            <span className="text-gray-700">Material:</span>
            <span className="text-gray-900 font-medium">
              {geometry.material} ({density.toFixed(2)} g/cm³)
            </span>
          </div>
          <div className="flex justify-between">
            <span className="text-gray-700">Outer Diameter:</span>
            <span className="text-gray-900 font-medium">{geometry.outerDiameter.toFixed(1)} mm</span>
          </div>
          <div className="flex justify-between">
            <span className="text-gray-700">Wall Thickness:</span>
            <span className="text-gray-900 font-medium">{geometry.wallThickness.toFixed(1)} mm</span>
          </div>
          <div className="flex justify-between">
            <span className="text-gray-700">Length:</span>
            <span className="text-gray-900 font-medium">{geometry.length} mm</span>
          </div>
          <div className="flex justify-between pt-2 border-t border-gray-200">
            <span className="text-gray-700">Calculated Mass:</span>
            <span className="text-gray-900 font-medium">
              {calculatedMass !== null ? `${calculatedMass.toFixed(3)} kg` : "—"}
            </span>
          </div>
        </div>
      </div>

      {/* Geometry Summary */}
      <div className="bg-white border border-gray-300 rounded p-4">
        <h4 className="text-sm font-semibold text-gray-800 mb-3">Geometry</h4>
        <div className="space-y-2 text-sm">
          <div className="flex justify-between">
            <span className="text-gray-700">Bends:</span>
            <span className="text-gray-900 font-medium">
              {config.bends.length}
              {config.bends.length > 0 && ` (total angle: ${totalBendAngle.toFixed(0)}°)`}
            </span>
          </div>
          <div className="flex justify-between">
            <span className="text-gray-700">Ports:</span>
            <span className="text-gray-900 font-medium">
              {config.ports.length}
              {config.ports.length > 0 && ` (${config.ports.map((p) => p.type.toLowerCase()).join(", ")})`}
            </span>
          </div>
          <div className="flex justify-between">
            <span className="text-gray-700">Springback Factor:</span>
            <span className="text-gray-900 font-medium">{config.advanced.springbackFactor.toFixed(2)}</span>
          </div>
          <div className="flex justify-between">
            <span className="text-gray-700">Surface Finish:</span>
            <span className="text-gray-900 font-medium">{config.advanced.surfaceFinish}</span>
          </div>
        </div>
      </div>

      {/* Validation Status */}
      <div
        className={`border-l-4 p-3 rounded ${
          isValid
            ? "bg-green-50 border-green-500"
            : "bg-red-50 border-red-500"
        }`}
      >
        <div className="flex items-start gap-2">
          <span
            className={`text-lg font-bold ${isValid ? "text-green-600" : "text-red-600"}`}
          >
            {isValid ? "✓" : "✗"}
          </span>
          <div>
            <p className="text-sm font-semibold text-gray-800">
              {isValid && config.bends.length > 0
                ? "Ready to manufacture"
                : hasErrors
                ? "Validation errors found"
                : "Configuration incomplete"}
            </p>
            {hasErrors && (
              <div className="mt-2 text-xs text-red-700">
                {errors.map((err, idx) => (
                  <div key={idx} className="mt-1">
                    • {err.message}
                  </div>
                ))}
              </div>
            )}
          </div>
        </div>
      </div>

      {/* Warnings */}
      {config.advanced.springbackFactor > 0.7 && (
        <div className="bg-yellow-50 border-l-4 border-yellow-500 p-3 rounded">
          <p className="text-xs font-medium text-yellow-800">
            Warning: High springback factor ({config.advanced.springbackFactor.toFixed(2)}) may affect accuracy
          </p>
        </div>
      )}
    </div>
  );
};

// ============ MAIN TUBE DIALOG COMPONENT ============

const TubeDialog: React.FC<TubeDialogProps> = ({
  isOpen,
  onClose,
  onSave,
  initialConfig,
}) => {
  const [activeTab, setActiveTab] = useState<string>("geometry");
  const [config, setConfig] = useState<TubeConfig>(
    initialConfig || DEFAULT_CONFIG
  );
  const [errors, setErrors] = useState<ValidationError[]>([]);
  const [calculatedMass, setCalculatedMass] = useState<number | null>(null);

  // Validation logic
  const validateConfig = useCallback((cfg: TubeConfig) => {
    const newErrors: ValidationError[] = [];
    const { geometry, bends, ports } = cfg;

    // Geometry validation
    if (geometry.outerDiameter <= 0 || geometry.outerDiameter > 100) {
      newErrors.push({
        field: "outerDiameter",
        message: "OD must be between 1 and 100 mm",
      });
    }

    if (geometry.wallThickness <= 0.1 || geometry.wallThickness >= geometry.outerDiameter / 2) {
      newErrors.push({
        field: "wallThickness",
        message: `WT must be between 0.1 and ${(geometry.outerDiameter / 2).toFixed(2)} mm`,
      });
    }

    if (geometry.length <= 0 || geometry.length > 5000) {
      newErrors.push({
        field: "length",
        message: "Length must be between 10 and 5000 mm",
      });
    }

    const odWtRatio = geometry.outerDiameter / geometry.wallThickness;
    if (odWtRatio < 2 || odWtRatio > 100) {
      newErrors.push({
        field: "wallThickness",
        message: `OD/WT ratio must be between 2 and 100 (current: ${odWtRatio.toFixed(1)})`,
      });
    }

    // Bends validation
    bends.forEach((bend, idx) => {
      if (bend.position < 0 || bend.position >= geometry.length) {
        newErrors.push({
          field: `bend_${idx}_position`,
          message: `Bend ${idx + 1}: position must be between 0 and ${geometry.length} mm`,
        });
      }
      if (bend.radius < geometry.outerDiameter / 2) {
        newErrors.push({
          field: `bend_${idx}_radius`,
          message: `Bend ${idx + 1}: radius must be at least ${(geometry.outerDiameter / 2).toFixed(1)} mm`,
        });
      }
      if (bend.angle <= 0 || bend.angle > 180) {
        newErrors.push({
          field: `bend_${idx}_angle`,
          message: `Bend ${idx + 1}: angle must be between 1 and 180 degrees`,
        });
      }
    });

    // Ports validation
    ports.forEach((port, idx) => {
      if (port.position < 0 || port.position >= geometry.length) {
        newErrors.push({
          field: `port_${idx}_position`,
          message: `Port ${idx + 1}: position must be between 0 and ${geometry.length} mm`,
        });
      }
      if (port.diameter >= geometry.outerDiameter) {
        newErrors.push({
          field: `port_${idx}_diameter`,
          message: `Port ${idx + 1}: diameter must be less than OD (${geometry.outerDiameter} mm)`,
        });
      }
    });

    setErrors(newErrors);
    return newErrors.length === 0;
  }, []);

  // Calculate mass when geometry changes
  const calculateMass = useCallback(() => {
    const { outerDiameter, wallThickness, length, material } = config.geometry;
    const density = MATERIAL_DENSITIES[material] || 5.0;

    // Calculate volume
    const innerDiameter = outerDiameter - 2 * wallThickness;
    const area = Math.PI * (outerDiameter * outerDiameter - innerDiameter * innerDiameter) / 4;
    const volume = length * area / 1000; // Convert to cm³
    const mass = (volume * density) / 1000; // Convert to kg

    setCalculatedMass(mass);
  }, [config.geometry]);

  // Handle bends
  const handleAddBend = () => {
    const newBend: BendParams = {
      id: `bend_${Date.now()}`,
      position: config.geometry.length / 2,
      radius: Math.max(config.geometry.outerDiameter, 50),
      angle: 90,
    };
    setConfig({
      ...config,
      bends: [...config.bends, newBend],
    });
  };

  const handleEditBend = (index: number, bend: BendParams) => {
    const newBends = [...config.bends];
    newBends[index] = bend;
    setConfig({
      ...config,
      bends: newBends,
    });
  };

  const handleDeleteBend = (index: number) => {
    setConfig({
      ...config,
      bends: config.bends.filter((_, i) => i !== index),
    });
  };

  // Handle ports
  const handleAddPort = () => {
    const newPort: PortParams = {
      id: `port_${Date.now()}`,
      name: `Port${config.ports.length + 1}`,
      position: config.geometry.length / 2,
      type: "Connection",
      diameter: 6,
    };
    setConfig({
      ...config,
      ports: [...config.ports, newPort],
    });
  };

  const handleEditPort = (index: number, port: PortParams) => {
    const newPorts = [...config.ports];
    newPorts[index] = port;
    setConfig({
      ...config,
      ports: newPorts,
    });
  };

  const handleDeletePort = (index: number) => {
    setConfig({
      ...config,
      ports: config.ports.filter((_, i) => i !== index),
    });
  };

  // Handle save
  const handleSave = () => {
    validateConfig(config);
    if (errors.length === 0) {
      onSave(config);
      onClose();
    }
  };

  // Validate and calculate mass whenever config changes
  React.useEffect(() => {
    validateConfig(config);
    calculateMass();
  }, [config, validateConfig, calculateMass]);

  if (!isOpen) return null;

  const isValid = errors.length === 0;

  return (
    <div className="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50">
      <div role="dialog" aria-modal="true" aria-label="Tube Configuration" className="bg-white rounded-lg shadow-xl w-full max-w-2xl max-h-[90vh] flex flex-col">
        {/* Header */}
        <div className="flex justify-between items-center p-6 border-b border-gray-200">
          <h2 className="text-lg font-bold text-gray-900">Tube Configuration</h2>
          <button
            onClick={onClose}
            className="text-gray-500 hover:text-gray-700 text-2xl leading-none"
          >
            ×
          </button>
        </div>

        {/* Tabs */}
        <div className="flex gap-1 px-6 pt-4 border-b border-gray-200 bg-gray-50">
          {[
            { id: "geometry", label: "Geometry" },
            { id: "bends", label: "Bends" },
            { id: "ports", label: "Ports" },
            { id: "offset-calculator", label: "Offset Calculator" },
            { id: "templates", label: "Templates" },
            { id: "advanced", label: "Advanced" },
            { id: "preview", label: "Preview" },
          ].map((tab) => (
            <button
              key={tab.id}
              role="tab"
              aria-selected={activeTab === tab.id}
              onClick={() => setActiveTab(tab.id)}
              className={`px-4 py-2 text-sm font-medium rounded-t transition ${
                activeTab === tab.id
                  ? "bg-white text-blue-600 border-b-2 border-blue-500"
                  : "text-gray-700 hover:text-gray-900"
              }`}
            >
              {tab.label}
            </button>
          ))}
        </div>

        {/* Content */}
        <div className="flex-1 overflow-y-auto p-6">
          {activeTab === "geometry" && (
            <GeometryTab
              config={config}
              onChange={setConfig}
              errors={errors}
              calculatedMass={calculatedMass}
              onTemplateSelected={(id) => {
                // Optional: handle template selection in parent
              }}
            />
          )}
          {activeTab === "bends" && (
            <BendsTab
              bends={config.bends}
              tubeLength={config.geometry.length}
              onAdd={handleAddBend}
              onEdit={handleEditBend}
              onDelete={handleDeleteBend}
              errors={errors}
            />
          )}
          {activeTab === "ports" && (
            <PortsTab
              ports={config.ports}
              tubeLength={config.geometry.length}
              tubeOD={config.geometry.outerDiameter}
              onAdd={handleAddPort}
              onEdit={handleEditPort}
              onDelete={handleDeletePort}
              errors={errors}
            />
          )}
          {activeTab === "offset-calculator" && (
            <OffsetCalculatorTab
              tubeParams={{
                outerDiameter: config.geometry.outerDiameter,
                wallThickness: config.geometry.wallThickness,
                length: config.geometry.length,
                material: config.geometry.material,
              }}
              onResultSync={(offsetData) => {
                // Optional: Handle offset results synchronization
                console.log("Offset results:", offsetData);
              }}
            />
          )}
          {activeTab === "templates" && (
            <TemplateSuggestionsTab
              outerDiameter={config.geometry.outerDiameter}
              wallThickness={config.geometry.wallThickness}
              length={config.geometry.length}
              onTemplateSelected={(templateId) => {
                // Optional: handle template selection
                console.log("Template selected:", templateId);
              }}
            />
          )}
          {activeTab === "advanced" && (
            <AdvancedTab config={config} onChange={setConfig} />
          )}
          {activeTab === "preview" && (
            <PreviewTab
              config={config}
              calculatedMass={calculatedMass}
              isValid={isValid}
              errors={errors}
            />
          )}
        </div>

        {/* Footer */}
        <div className="flex justify-between items-center p-6 border-t border-gray-200 bg-gray-50">
          <button
            onClick={onClose}
            className="px-4 py-2 border border-gray-300 text-gray-700 rounded font-medium hover:bg-gray-100 transition"
          >
            Cancel
          </button>
          <div className="flex items-center gap-2">
            {isValid && (
              <span className="text-green-600 font-medium flex items-center gap-1">
                ✓ Valid
              </span>
            )}
            <button
              onClick={handleSave}
              disabled={!isValid}
              className={`px-4 py-2 rounded font-medium transition ${
                isValid
                  ? "bg-blue-600 text-white hover:bg-blue-700"
                  : "bg-gray-300 text-gray-500 cursor-not-allowed"
              }`}
            >
              Save
            </button>
          </div>
        </div>
      </div>
    </div>
  );
};

export default TubeDialog;
