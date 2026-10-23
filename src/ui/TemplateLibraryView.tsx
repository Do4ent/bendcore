/**
 * Template Library View Component
 * Display, filter, and manage tube bend templates
 * Task 2.3: React UI Components (Subtask 3)
 */

import React, { useState, useMemo, useCallback } from "react";
import { Template, useTemplates } from "./hooks/useTemplates";
import { TemplateEditor } from "./TemplateEditor";
import { getColorHex } from "./ColorScheme";

type ViewMode = "grid" | "list";

interface TemplateLibraryViewProps {
  isOpen: boolean;
  onClose: () => void;
}

export const TemplateLibraryView: React.FC<TemplateLibraryViewProps> = ({
  isOpen,
  onClose,
}) => {
  const {
    library,
    addTemplate,
    updateTemplate,
    deleteTemplate,
    getTemplate,
    setActiveTemplate,
  } = useTemplates();

  const [viewMode, setViewMode] = useState<ViewMode>("grid");
  const [categoryFilter, setCategoryFilter] = useState<string>("All");
  const [searchQuery, setSearchQuery] = useState<string>("");
  const [editingTemplate, setEditingTemplate] = useState<Partial<Template> | null>(null);
  const [isCreatingNew, setIsCreatingNew] = useState(false);

  // Filter templates
  const filteredTemplates = useMemo(() => {
    return library.templates.filter((t) => {
      const matchesCategory = categoryFilter === "All" || t.category === categoryFilter;
      const matchesSearch =
        t.name.toLowerCase().includes(searchQuery.toLowerCase()) ||
        t.id.toLowerCase().includes(searchQuery.toLowerCase()) ||
        t.description.toLowerCase().includes(searchQuery.toLowerCase());
      return matchesCategory && matchesSearch;
    });
  }, [library.templates, categoryFilter, searchQuery]);

  const handleEditTemplate = useCallback((template: Template) => {
    setEditingTemplate(template);
    setIsCreatingNew(false);
  }, []);

  const handleCreateNew = useCallback(() => {
    setEditingTemplate({
      id: `custom-${Date.now()}`,
      name: "",
      description: "",
      category: "Custom",
      outer_diameter: { min: 10, max: 50, default: 30, unit: "mm" },
      wall_thickness: { min: 1, max: 3, default: 2, unit: "mm" },
      length: { min: 100, max: 500, default: 300, unit: "mm" },
      use_case: "",
      compatible_materials: [],
    });
    setIsCreatingNew(true);
  }, []);

  const handleSaveTemplate = useCallback(
    (template: Partial<Template>) => {
      if (isCreatingNew && template.id) {
        addTemplate(template as Template);
      } else if (template.id) {
        updateTemplate(template.id, template);
      }
      setEditingTemplate(null);
    },
    [isCreatingNew, addTemplate, updateTemplate]
  );

  const handleDeleteTemplate = useCallback(
    (id: string) => {
      if (confirm("Are you sure you want to delete this template?")) {
        deleteTemplate(id);
      }
    },
    [deleteTemplate]
  );

  const handleSelectTemplate = useCallback(
    (id: string) => {
      setActiveTemplate(id);
    },
    [setActiveTemplate]
  );

  if (!isOpen) return null;

  if (editingTemplate) {
    return (
      <div
        style={{
          position: "fixed",
          inset: 0,
          backgroundColor: "rgba(0,0,0,0.5)",
          display: "flex",
          alignItems: "center",
          justifyContent: "center",
          zIndex: 1000,
        }}
      >
        <div
          style={{
            width: "90%",
            maxWidth: "600px",
            backgroundColor: getColorHex("background"),
            borderRadius: "8px",
            boxShadow: "0 20px 25px rgba(0,0,0,0.15)",
          }}
        >
          <TemplateEditor
            template={editingTemplate}
            onSave={handleSaveTemplate}
            onCancel={() => setEditingTemplate(null)}
            isNew={isCreatingNew}
          />
        </div>
      </div>
    );
  }

  return (
    <div
      style={{
        position: "fixed",
        inset: 0,
        backgroundColor: "rgba(0,0,0,0.5)",
        display: "flex",
        alignItems: "center",
        justifyContent: "center",
        zIndex: 1000,
      }}
    >
      <div
        style={{
          width: "95%",
          maxWidth: "1000px",
          height: "90vh",
          backgroundColor: getColorHex("background"),
          borderRadius: "8px",
          boxShadow: "0 20px 25px rgba(0,0,0,0.15)",
          display: "flex",
          flexDirection: "column",
        }}
      >
        {/* Header */}
        <div
          style={{
            padding: "20px",
            borderBottom: `1px solid ${getColorHex("border")}`,
            display: "flex",
            justifyContent: "space-between",
            alignItems: "center",
          }}
        >
          <h2 style={{ margin: 0, color: getColorHex("text") }}>Template Library</h2>
          <button
            onClick={onClose}
            style={{
              background: "none",
              border: "none",
              fontSize: "24px",
              cursor: "pointer",
              color: getColorHex("text"),
            }}
          >
            ×
          </button>
        </div>

        {/* Toolbar */}
        <div
          style={{
            padding: "16px 20px",
            borderBottom: `1px solid ${getColorHex("border")}`,
            display: "grid",
            gridTemplateColumns: "auto auto 1fr auto auto",
            gap: "12px",
            alignItems: "center",
          }}
        >
          <button
            onClick={handleCreateNew}
            style={{
              padding: "8px 16px",
              backgroundColor: getColorHex("primary"),
              color: "#fff",
              border: "none",
              borderRadius: "4px",
              cursor: "pointer",
              fontWeight: 500,
            }}
          >
            + New Template
          </button>

          <div style={{ display: "flex", gap: "4px" }}>
            <button
              onClick={() => setViewMode("grid")}
              style={{
                padding: "6px 12px",
                backgroundColor: viewMode === "grid" ? getColorHex("primary") : getColorHex("surface"),
                color: viewMode === "grid" ? "#fff" : getColorHex("text"),
                border: `1px solid ${getColorHex("border")}`,
                borderRadius: "4px",
                cursor: "pointer",
                fontSize: "12px",
              }}
            >
              Grid
            </button>
            <button
              onClick={() => setViewMode("list")}
              style={{
                padding: "6px 12px",
                backgroundColor: viewMode === "list" ? getColorHex("primary") : getColorHex("surface"),
                color: viewMode === "list" ? "#fff" : getColorHex("text"),
                border: `1px solid ${getColorHex("border")}`,
                borderRadius: "4px",
                cursor: "pointer",
                fontSize: "12px",
              }}
            >
              List
            </button>
          </div>

          <input
            type="text"
            placeholder="Search templates..."
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            style={{
              padding: "8px 12px",
              border: `1px solid ${getColorHex("border")}`,
              borderRadius: "4px",
              backgroundColor: getColorHex("surface"),
              color: getColorHex("text"),
            }}
          />

          <select
            value={categoryFilter}
            onChange={(e) => setCategoryFilter(e.target.value)}
            style={{
              padding: "8px 12px",
              border: `1px solid ${getColorHex("border")}`,
              borderRadius: "4px",
              backgroundColor: getColorHex("surface"),
              color: getColorHex("text"),
            }}
          >
            <option>All Categories</option>
            <option>Standard</option>
            <option>Reinforced</option>
            <option>Large-Assembly</option>
            <option>Custom</option>
          </select>
        </div>

        {/* Content Area */}
        <div
          style={{
            flex: 1,
            overflowY: "auto",
            padding: "20px",
          }}
        >
          {filteredTemplates.length === 0 ? (
            <div
              style={{
                textAlign: "center",
                padding: "40px 20px",
                color: getColorHex("text"),
              }}
            >
              <p style={{ fontSize: "16px", marginBottom: "8px" }}>No templates found</p>
              <button
                onClick={handleCreateNew}
                style={{
                  padding: "8px 16px",
                  backgroundColor: getColorHex("primary"),
                  color: "#fff",
                  border: "none",
                  borderRadius: "4px",
                  cursor: "pointer",
                  marginTop: "12px",
                }}
              >
                Create First Template
              </button>
            </div>
          ) : viewMode === "grid" ? (
            <div
              style={{
                display: "grid",
                gridTemplateColumns: "repeat(auto-fill, minmax(300px, 1fr))",
                gap: "16px",
              }}
            >
              {filteredTemplates.map((template) => (
                <div
                  key={template.id}
                  style={{
                    padding: "16px",
                    backgroundColor: getColorHex("surface"),
                    border: `1px solid ${getColorHex("border")}`,
                    borderRadius: "8px",
                    cursor: "pointer",
                    transition: "all 0.2s",
                  }}
                  onMouseOver={(e) => {
                    (e.currentTarget as HTMLDivElement).style.boxShadow =
                      "0 4px 12px rgba(0,0,0,0.15)";
                    (e.currentTarget as HTMLDivElement).style.transform = "translateY(-2px)";
                  }}
                  onMouseOut={(e) => {
                    (e.currentTarget as HTMLDivElement).style.boxShadow = "none";
                    (e.currentTarget as HTMLDivElement).style.transform = "translateY(0)";
                  }}
                >
                  <div style={{ marginBottom: "12px" }}>
                    <h4 style={{ margin: "0 0 4px 0", color: getColorHex("text") }}>
                      {template.name}
                    </h4>
                    <p
                      style={{
                        margin: 0,
                        fontSize: "12px",
                        color: getColorHex("textSecondary"),
                      }}
                    >
                      {template.category}
                    </p>
                  </div>
                  <p
                    style={{
                      margin: "8px 0",
                      fontSize: "13px",
                      color: getColorHex("text"),
                      lineHeight: "1.4",
                    }}
                  >
                    {template.description}
                  </p>
                  <div
                    style={{
                      marginTop: "12px",
                      display: "flex",
                      gap: "8px",
                      fontSize: "11px",
                      color: getColorHex("textSecondary"),
                    }}
                  >
                    <span>OD: {template.outer_diameter.min}-{template.outer_diameter.max}mm</span>
                    <span>Wall: {template.wall_thickness.min}-{template.wall_thickness.max}mm</span>
                  </div>
                  <div style={{ marginTop: "12px", display: "flex", gap: "8px" }}>
                    <button
                      onClick={() => handleSelectTemplate(template.id)}
                      style={{
                        flex: 1,
                        padding: "6px 12px",
                        backgroundColor: getColorHex("primary"),
                        color: "#fff",
                        border: "none",
                        borderRadius: "4px",
                        cursor: "pointer",
                        fontSize: "12px",
                        fontWeight: 500,
                      }}
                    >
                      Select
                    </button>
                    <button
                      onClick={() => handleEditTemplate(template)}
                      style={{
                        padding: "6px 12px",
                        backgroundColor: getColorHex("surface"),
                        color: getColorHex("text"),
                        border: `1px solid ${getColorHex("border")}`,
                        borderRadius: "4px",
                        cursor: "pointer",
                        fontSize: "12px",
                      }}
                    >
                      Edit
                    </button>
                    {template.category === "Custom" && (
                      <button
                        onClick={() => handleDeleteTemplate(template.id)}
                        style={{
                          padding: "6px 12px",
                          backgroundColor: getColorHex("error"),
                          color: "#fff",
                          border: "none",
                          borderRadius: "4px",
                          cursor: "pointer",
                          fontSize: "12px",
                        }}
                      >
                        Delete
                      </button>
                    )}
                  </div>
                </div>
              ))}
            </div>
          ) : (
            <table style={{ width: "100%", borderCollapse: "collapse" }}>
              <thead>
                <tr
                  style={{
                    borderBottom: `2px solid ${getColorHex("border")}`,
                  }}
                >
                  <th style={{ padding: "12px", textAlign: "left", fontWeight: 600 }}>Name</th>
                  <th style={{ padding: "12px", textAlign: "left", fontWeight: 600 }}>Category</th>
                  <th style={{ padding: "12px", textAlign: "left", fontWeight: 600 }}>OD Range</th>
                  <th style={{ padding: "12px", textAlign: "left", fontWeight: 600 }}>Wall Range</th>
                  <th style={{ padding: "12px", textAlign: "center", fontWeight: 600 }}>Actions</th>
                </tr>
              </thead>
              <tbody>
                {filteredTemplates.map((template) => (
                  <tr
                    key={template.id}
                    style={{
                      borderBottom: `1px solid ${getColorHex("border")}`,
                    }}
                  >
                    <td style={{ padding: "12px" }}>{template.name}</td>
                    <td style={{ padding: "12px" }}>{template.category}</td>
                    <td style={{ padding: "12px" }}>
                      {template.outer_diameter.min}-{template.outer_diameter.max} mm
                    </td>
                    <td style={{ padding: "12px" }}>
                      {template.wall_thickness.min}-{template.wall_thickness.max} mm
                    </td>
                    <td style={{ padding: "12px", textAlign: "center" }}>
                      <button
                        onClick={() => handleSelectTemplate(template.id)}
                        style={{
                          padding: "4px 8px",
                          backgroundColor: getColorHex("primary"),
                          color: "#fff",
                          border: "none",
                          borderRadius: "4px",
                          cursor: "pointer",
                          fontSize: "12px",
                          marginRight: "4px",
                        }}
                      >
                        Select
                      </button>
                      <button
                        onClick={() => handleEditTemplate(template)}
                        style={{
                          padding: "4px 8px",
                          backgroundColor: getColorHex("surface"),
                          color: getColorHex("text"),
                          border: `1px solid ${getColorHex("border")}`,
                          borderRadius: "4px",
                          cursor: "pointer",
                          fontSize: "12px",
                          marginRight: "4px",
                        }}
                      >
                        Edit
                      </button>
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          )}
        </div>

        {/* Footer */}
        <div
          style={{
            padding: "16px 20px",
            borderTop: `1px solid ${getColorHex("border")}`,
            textAlign: "right",
          }}
        >
          <button
            onClick={onClose}
            style={{
              padding: "8px 16px",
              backgroundColor: getColorHex("surface"),
              color: getColorHex("text"),
              border: `1px solid ${getColorHex("border")}`,
              borderRadius: "4px",
              cursor: "pointer",
              fontWeight: 500,
            }}
          >
            Close
          </button>
        </div>
      </div>
    </div>
  );
};
