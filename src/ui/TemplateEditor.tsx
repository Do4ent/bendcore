/**
 * Template Editor Component
 * UI for creating, editing, and managing tube templates
 * Task 2.4: Custom Template Builder UI (400-500 LOC)
 */

import React, { useState, useCallback } from "react";
import { useTemplates, Template, TemplateParameter } from "./hooks/useTemplates";

// ============ INTERFACES ============

interface TemplateEditorProps {
  isOpen: boolean;
  onClose: () => void;
}

interface EditingTemplate {
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

// ============ MATERIAL OPTIONS ============

const MATERIAL_OPTIONS = [
  "steel-1020",
  "steel-1045",
  "steel-4140",
  "ss-304",
  "ss-316",
  "aluminum-6061",
  "aluminum-7075",
  "copper-pure",
  "titanium-grade5",
  "brass-7030",
];

// ============ HELPER COMPONENT: PARAMETER EDITOR ============

interface ParameterEditorProps {
  label: string;
  param: TemplateParameter;
  onChange: (param: TemplateParameter) => void;
}

const ParameterEditor: React.FC<ParameterEditorProps> = ({
  label,
  param,
  onChange,
}) => {
  const handleChange = useCallback(
    (field: keyof TemplateParameter, value: any) => {
      onChange({
        ...param,
        [field]: field === "unit" ? value : parseFloat(value) || 0,
      });
    },
    [param, onChange]
  );

  return (
    <div className="bg-gray-50 border border-gray-200 rounded p-3">
      <h4 className="text-sm font-semibold text-gray-800 mb-2">{label}</h4>
      <div className="space-y-2">
        <div className="flex gap-2">
          <div className="flex-1">
            <label className="block text-xs font-medium text-gray-700 mb-1">
              Min
            </label>
            <input
              type="number"
              step="0.1"
              value={param.min}
              onChange={(e) => handleChange("min", e.target.value)}
              className="w-full px-2 py-1 border border-gray-300 rounded text-sm focus:outline-none focus:ring-2 focus:ring-blue-500"
            />
          </div>
          <div className="flex-1">
            <label className="block text-xs font-medium text-gray-700 mb-1">
              Max
            </label>
            <input
              type="number"
              step="0.1"
              value={param.max}
              onChange={(e) => handleChange("max", e.target.value)}
              className="w-full px-2 py-1 border border-gray-300 rounded text-sm focus:outline-none focus:ring-2 focus:ring-blue-500"
            />
          </div>
          <div className="flex-1">
            <label className="block text-xs font-medium text-gray-700 mb-1">
              Default
            </label>
            <input
              type="number"
              step="0.1"
              value={param.default}
              onChange={(e) => handleChange("default", e.target.value)}
              className="w-full px-2 py-1 border border-gray-300 rounded text-sm focus:outline-none focus:ring-2 focus:ring-blue-500"
            />
          </div>
          <div className="flex-1">
            <label className="block text-xs font-medium text-gray-700 mb-1">
              Unit
            </label>
            <input
              type="text"
              value={param.unit}
              onChange={(e) => handleChange("unit", e.target.value)}
              className="w-full px-2 py-1 border border-gray-300 rounded text-sm focus:outline-none focus:ring-2 focus:ring-blue-500"
            />
          </div>
        </div>
        <div className="text-xs text-gray-600">
          Range: {param.min} - {param.max} {param.unit}
        </div>
      </div>
    </div>
  );
};

// ============ MAIN COMPONENT ============

const TemplateEditor: React.FC<TemplateEditorProps> = ({ isOpen, onClose }) => {
  const {
    library,
    loading,
    addTemplate,
    updateTemplate,
    deleteTemplate,
    exportTemplates,
    importTemplates,
  } = useTemplates();

  const [activeTab, setActiveTab] = useState<"list" | "create" | "edit">(
    "list"
  );
  const [selectedTemplateId, setSelectedTemplateId] = useState<string | null>(
    null
  );
  const [editingTemplate, setEditingTemplate] = useState<EditingTemplate | null>(
    null
  );
  const [showDeleteConfirm, setShowDeleteConfirm] = useState(false);
  const [deleteTargetId, setDeleteTargetId] = useState<string | null>(null);

  // Create new template
  const handleCreateNew = useCallback(() => {
    const newTemplate: EditingTemplate = {
      id: `custom-${Date.now()}`,
      name: "New Template",
      description: "Custom template",
      category: "Custom",
      outer_diameter: { min: 16, max: 50, default: 25, unit: "mm" },
      wall_thickness: { min: 1.6, max: 2.5, default: 2.0, unit: "mm" },
      length: { min: 50, max: 800, default: 400, unit: "mm" },
      use_case: "",
      compatible_materials: [],
    };
    setEditingTemplate(newTemplate);
    setActiveTab("create");
  }, []);

  // Edit existing template
  const handleEditTemplate = useCallback((id: string) => {
    const template = library.templates.find((t) => t.id === id);
    if (template) {
      setEditingTemplate(template);
      setSelectedTemplateId(id);
      setActiveTab("edit");
    }
  }, [library.templates]);

  // Save template
  const handleSaveTemplate = useCallback(() => {
    if (!editingTemplate) return;

    if (activeTab === "create") {
      addTemplate(editingTemplate);
    } else {
      updateTemplate(editingTemplate.id, editingTemplate);
    }

    setEditingTemplate(null);
    setSelectedTemplateId(null);
    setActiveTab("list");
  }, [editingTemplate, activeTab, addTemplate, updateTemplate]);

  // Delete template
  const handleDeleteTemplate = useCallback(() => {
    if (deleteTargetId) {
      deleteTemplate(deleteTargetId);
      setShowDeleteConfirm(false);
      setDeleteTargetId(null);
    }
  }, [deleteTargetId, deleteTemplate]);

  // Import templates
  const handleImport = useCallback(() => {
    const input = document.createElement("input");
    input.type = "file";
    input.accept = ".json";
    input.onchange = async (e) => {
      const file = (e.target as HTMLInputElement).files?.[0];
      if (file) {
        try {
          await importTemplates(file);
          alert("Templates imported successfully");
        } catch (err) {
          alert(
            "Failed to import templates: " +
              (err instanceof Error ? err.message : "Unknown error")
          );
        }
      }
    };
    input.click();
  }, [importTemplates]);

  if (!isOpen) return null;

  return (
    <div className="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50">
      <div className="bg-white rounded-lg shadow-xl w-full max-w-3xl max-h-[90vh] flex flex-col">
        {/* Header */}
        <div className="flex justify-between items-center p-6 border-b border-gray-200">
          <h2 className="text-lg font-bold text-gray-900">Template Editor</h2>
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
            { id: "list", label: "Templates" },
            { id: "create", label: "Create" },
          ].map((tab) => (
            <button
              key={tab.id}
              onClick={() => {
                if (tab.id === "create") {
                  handleCreateNew();
                } else {
                  setActiveTab(tab.id as "list" | "create" | "edit");
                  setEditingTemplate(null);
                }
              }}
              className={`px-4 py-2 text-sm font-medium rounded-t transition ${
                activeTab === tab.id && tab.id !== "create"
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
          {loading ? (
            <div className="text-center py-8 text-gray-500">
              <p>Loading templates...</p>
            </div>
          ) : activeTab === "list" ? (
            <TemplateListView
              templates={library.templates}
              onEdit={handleEditTemplate}
              onDelete={(id) => {
                setDeleteTargetId(id);
                setShowDeleteConfirm(true);
              }}
              onExport={exportTemplates}
              onImport={handleImport}
            />
          ) : editingTemplate ? (
            <TemplateFormView
              template={editingTemplate}
              onChange={setEditingTemplate}
              isCreate={activeTab === "create"}
            />
          ) : null}
        </div>

        {/* Footer */}
        <div className="flex justify-between items-center p-6 border-t border-gray-200 bg-gray-50">
          {(activeTab === "create" || activeTab === "edit") && editingTemplate ? (
            <>
              <button
                onClick={() => {
                  setEditingTemplate(null);
                  setActiveTab("list");
                }}
                className="px-4 py-2 border border-gray-300 text-gray-700 rounded font-medium hover:bg-gray-100 transition"
              >
                Cancel
              </button>
              <button
                onClick={handleSaveTemplate}
                className="px-4 py-2 bg-blue-600 text-white rounded font-medium hover:bg-blue-700 transition"
              >
                Save Template
              </button>
            </>
          ) : (
            <button
              onClick={onClose}
              className="px-4 py-2 border border-gray-300 text-gray-700 rounded font-medium hover:bg-gray-100 transition ml-auto"
            >
              Close
            </button>
          )}
        </div>
      </div>

      {/* Delete Confirmation Modal */}
      {showDeleteConfirm && (
        <div className="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-60">
          <div className="bg-white rounded-lg shadow-xl p-6 max-w-sm">
            <h3 className="text-lg font-bold text-gray-900 mb-4">
              Delete Template?
            </h3>
            <p className="text-gray-700 mb-6">
              This action cannot be undone.
            </p>
            <div className="flex gap-3 justify-end">
              <button
                onClick={() => setShowDeleteConfirm(false)}
                className="px-4 py-2 border border-gray-300 text-gray-700 rounded font-medium hover:bg-gray-100"
              >
                Cancel
              </button>
              <button
                onClick={handleDeleteTemplate}
                className="px-4 py-2 bg-red-600 text-white rounded font-medium hover:bg-red-700"
              >
                Delete
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

// ============ HELPER COMPONENTS ============

interface TemplateListViewProps {
  templates: Template[];
  onEdit: (id: string) => void;
  onDelete: (id: string) => void;
  onExport: () => void;
  onImport: () => void;
}

const TemplateListView: React.FC<TemplateListViewProps> = ({
  templates,
  onEdit,
  onDelete,
  onExport,
  onImport,
}) => {
  return (
    <div className="space-y-4">
      {/* Action Buttons */}
      <div className="flex gap-2">
        <button
          onClick={onExport}
          className="px-3 py-2 bg-green-600 text-white text-sm font-medium rounded hover:bg-green-700 transition"
        >
          Export Templates
        </button>
        <button
          onClick={onImport}
          className="px-3 py-2 bg-blue-600 text-white text-sm font-medium rounded hover:bg-blue-700 transition"
        >
          Import Templates
        </button>
      </div>

      {/* Templates List */}
      {templates.length === 0 ? (
        <div className="text-center py-8 text-gray-500">
          <p>No templates yet</p>
        </div>
      ) : (
        <div className="space-y-3">
          {templates.map((template) => (
            <div
              key={template.id}
              className="border border-gray-300 rounded p-4 hover:bg-gray-50"
            >
              <div className="flex justify-between items-start mb-2">
                <div>
                  <h3 className="font-semibold text-gray-900">
                    {template.name}
                  </h3>
                  <p className="text-sm text-gray-600">{template.description}</p>
                  <span className="inline-block mt-1 px-2 py-1 bg-blue-100 text-blue-800 text-xs rounded">
                    {template.category}
                  </span>
                </div>
                <div className="flex gap-2">
                  <button
                    onClick={() => onEdit(template.id)}
                    className="px-3 py-1 text-sm text-blue-600 hover:bg-blue-50 rounded"
                  >
                    Edit
                  </button>
                  <button
                    onClick={() => onDelete(template.id)}
                    className="px-3 py-1 text-sm text-red-600 hover:bg-red-50 rounded"
                  >
                    Delete
                  </button>
                </div>
              </div>

              {/* Parameter Preview */}
              <div className="bg-gray-100 rounded p-2 mt-2 text-xs text-gray-700 space-y-1">
                <div>
                  OD: {template.outer_diameter.min} -{" "}
                  {template.outer_diameter.max} {template.outer_diameter.unit}
                </div>
                <div>
                  Wall: {template.wall_thickness.min} -{" "}
                  {template.wall_thickness.max} {template.wall_thickness.unit}
                </div>
                <div>
                  Length: {template.length.min} - {template.length.max}{" "}
                  {template.length.unit}
                </div>
              </div>
            </div>
          ))}
        </div>
      )}
    </div>
  );
};

interface TemplateFormViewProps {
  template: EditingTemplate;
  onChange: (template: EditingTemplate) => void;
  isCreate: boolean;
}

const TemplateFormView: React.FC<TemplateFormViewProps> = ({
  template,
  onChange,
  isCreate,
}) => {
  const handleChange = useCallback(
    (field: keyof EditingTemplate, value: any) => {
      onChange({
        ...template,
        [field]: value,
      });
    },
    [template, onChange]
  );

  const handleParameterChange = useCallback(
    (field: keyof Omit<EditingTemplate, keyof Omit<EditingTemplate, "outer_diameter" | "wall_thickness" | "length">>, param: TemplateParameter) => {
      onChange({
        ...template,
        [field]: param,
      });
    },
    [template, onChange]
  );

  return (
    <div className="space-y-4">
      {/* Basic Info */}
      <div className="space-y-3">
        <div>
          <label className="block text-sm font-medium text-gray-700 mb-1">
            Template Name
          </label>
          <input
            type="text"
            value={template.name}
            onChange={(e) => handleChange("name", e.target.value)}
            className="w-full px-3 py-2 border border-gray-300 rounded text-sm focus:outline-none focus:ring-2 focus:ring-blue-500"
          />
        </div>

        <div>
          <label className="block text-sm font-medium text-gray-700 mb-1">
            Description
          </label>
          <input
            type="text"
            value={template.description}
            onChange={(e) => handleChange("description", e.target.value)}
            className="w-full px-3 py-2 border border-gray-300 rounded text-sm focus:outline-none focus:ring-2 focus:ring-blue-500"
          />
        </div>

        <div className="grid grid-cols-2 gap-3">
          <div>
            <label className="block text-sm font-medium text-gray-700 mb-1">
              Category
            </label>
            <select
              value={template.category}
              onChange={(e) => handleChange("category", e.target.value)}
              className="w-full px-3 py-2 border border-gray-300 rounded text-sm focus:outline-none focus:ring-2 focus:ring-blue-500"
            >
              <option value="Custom">Custom</option>
              <option value="Reinforced">Reinforced</option>
              <option value="Standard">Standard</option>
              <option value="Small-OD">Small-OD</option>
            </select>
          </div>
        </div>

        <div>
          <label className="block text-sm font-medium text-gray-700 mb-1">
            Use Case
          </label>
          <textarea
            value={template.use_case}
            onChange={(e) => handleChange("use_case", e.target.value)}
            rows={2}
            className="w-full px-3 py-2 border border-gray-300 rounded text-sm focus:outline-none focus:ring-2 focus:ring-blue-500"
          />
        </div>
      </div>

      {/* Parameter Editors */}
      <div className="space-y-3">
        <ParameterEditor
          label="Outer Diameter (mm)"
          param={template.outer_diameter}
          onChange={(param) => handleParameterChange("outer_diameter", param)}
        />
        <ParameterEditor
          label="Wall Thickness (mm)"
          param={template.wall_thickness}
          onChange={(param) => handleParameterChange("wall_thickness", param)}
        />
        <ParameterEditor
          label="Length (mm)"
          param={template.length}
          onChange={(param) => handleParameterChange("length", param)}
        />
      </div>

      {/* Materials */}
      <div>
        <label className="block text-sm font-medium text-gray-700 mb-2">
          Compatible Materials
        </label>
        <div className="grid grid-cols-2 gap-2">
          {MATERIAL_OPTIONS.map((material) => (
            <label key={material} className="flex items-center gap-2">
              <input
                type="checkbox"
                checked={template.compatible_materials.includes(material)}
                onChange={(e) => {
                  const newMaterials = e.target.checked
                    ? [...template.compatible_materials, material]
                    : template.compatible_materials.filter(
                        (m) => m !== material
                      );
                  handleChange("compatible_materials", newMaterials);
                }}
                className="w-4 h-4 text-blue-600 rounded"
              />
              <span className="text-sm text-gray-700">{material}</span>
            </label>
          ))}
        </div>
      </div>
    </div>
  );
};

export default TemplateEditor;
