/**
 * Materials Library Panel
 * Comprehensive materials database browser with search, filter, and custom materials
 * Phase 30 Part A: Extended Materials Library
 */

import React, { useState, useCallback, useMemo, useEffect } from "react";
import { getColorHex } from "./ColorScheme";

// ============ INTERFACES ============

interface MaterialProperties {
  youngsModulus: number;      // GPa
  yieldStrength: number;      // MPa
  tensileStrength: number;    // MPa
  elongation: number;         // %
  hardness: number;           // HV
  cost: number;               // $/kg
}

interface Material {
  id: string;
  name: string;
  category: string;
  density: number;            // g/cm³
  properties: MaterialProperties;
  standards: string[];
  applications: string[];
  availability: "Common" | "Standard" | "Premium" | "Custom";
  color: string;              // Hex color
}

interface MaterialsPanelProps {
  isOpen: boolean;
  onClose: () => void;
  onSelect?: (material: Material) => void;
}

// ============ CONSTANTS ============

const CATEGORIES = [
  "Carbon Steel",
  "Stainless Steel",
  "Aluminum Alloy",
  "Copper",
  "Titanium",
  "Nickel Alloy",
  "Special"
];

// ============ MAIN COMPONENT ============

export const MaterialsPanel: React.FC<MaterialsPanelProps> = ({
  isOpen,
  onClose,
  onSelect
}) => {
  const [materials, setMaterials] = useState<Material[]>([]);
  const [selectedMaterial, setSelectedMaterial] = useState<Material | null>(null);
  const [searchQuery, setSearchQuery] = useState("");
  const [selectedCategory, setSelectedCategory] = useState<string>("");
  const [showCustomForm, setShowCustomForm] = useState(false);
  const [loading, setLoading] = useState(false);

  // Load materials from IPC
  useEffect(() => {
    if (!isOpen) return;

    const loadMaterials = async () => {
      setLoading(true);
      try {
        // In a real app, this would call the IPC handler
        // For now, we'll use fetch from the data file
        const response = await fetch("data/materials.json");
        if (response.ok) {
          const data = await response.json();
          setMaterials(data.materials || []);
        }
      } catch (error) {
        console.error("Failed to load materials:", error);
      } finally {
        setLoading(false);
      }
    };

    loadMaterials();
  }, [isOpen]);

  // Filter materials based on search and category
  const filteredMaterials = useMemo(() => {
    return materials.filter((mat) => {
      const matchesSearch =
        mat.name.toLowerCase().includes(searchQuery.toLowerCase()) ||
        mat.id.toLowerCase().includes(searchQuery.toLowerCase());
      const matchesCategory = !selectedCategory || mat.category === selectedCategory;
      return matchesSearch && matchesCategory;
    });
  }, [materials, searchQuery, selectedCategory]);

  const handleSelectMaterial = (material: Material) => {
    setSelectedMaterial(material);
    if (onSelect) {
      onSelect(material);
    }
  };

  if (!isOpen) return null;

  return (
    <div className="materials-panel" style={styles.container}>
      {/* Header */}
      <div style={styles.header}>
        <h2>Materials Library</h2>
        <button
          onClick={onClose}
          style={styles.closeButton}
          title="Close materials library"
        >
          ✕
        </button>
      </div>

      {/* Search & Filter Section */}
      <div style={styles.controlsSection}>
        <input
          type="text"
          placeholder="Search materials..."
          value={searchQuery}
          onChange={(e) => setSearchQuery(e.target.value)}
          style={styles.searchInput}
        />

        <select
          value={selectedCategory}
          onChange={(e) => setSelectedCategory(e.target.value)}
          style={styles.categorySelect}
        >
          <option value="">All Categories</option>
          {CATEGORIES.map((cat) => (
            <option key={cat} value={cat}>
              {cat}
            </option>
          ))}
        </select>

        <button
          onClick={() => setShowCustomForm(!showCustomForm)}
          style={styles.addButton}
        >
          + Add Custom Material
        </button>
      </div>

      {/* Materials List & Details Split */}
      <div style={styles.contentWrapper}>
        {/* Materials List */}
        <div style={styles.listSection}>
          <h3 style={styles.listTitle}>
            Materials ({filteredMaterials.length})
          </h3>
          <div style={styles.materialsList}>
            {loading ? (
              <div style={styles.loadingMessage}>Loading materials...</div>
            ) : filteredMaterials.length === 0 ? (
              <div style={styles.emptyMessage}>No materials found</div>
            ) : (
              filteredMaterials.map((material) => (
                <div
                  key={material.id}
                  onClick={() => handleSelectMaterial(material)}
                  style={{
                    ...styles.materialItem,
                    ...(selectedMaterial?.id === material.id
                      ? styles.materialItemSelected
                      : {})
                  }}
                >
                  <div style={{
                      ...styles.materialItemColor,
                      backgroundColor: material.color,
                      ...styles.materialItemColorInner
                    }}
                  />
                  <div style={styles.materialItemContent}>
                    <div style={styles.materialItemName}>{material.name}</div>
                    <div style={styles.materialItemMeta}>
                      {material.density.toFixed(2)} g/cm³ · {material.category}
                    </div>
                  </div>
                  <div style={styles.materialItemDensity}>
                    {material.density.toFixed(2)}
                  </div>
                </div>
              ))
            )}
          </div>
        </div>

        {/* Material Details */}
        <div style={styles.detailsSection}>
          {selectedMaterial ? (
            <MaterialDetails material={selectedMaterial} />
          ) : (
            <div style={styles.noSelectionMessage}>
              Select a material to view details
            </div>
          )}
        </div>
      </div>

      {/* Custom Material Form */}
      {showCustomForm && (
        <CustomMaterialForm
          onClose={() => setShowCustomForm(false)}
          onSubmit={(newMaterial) => {
            setMaterials([...materials, newMaterial]);
            setShowCustomForm(false);
          }}
        />
      )}
    </div>
  );
};

// ============ MATERIAL DETAILS COMPONENT ============

interface MaterialDetailsProps {
  material: Material;
}

const MaterialDetails: React.FC<MaterialDetailsProps> = ({ material }) => {
  return (
    <div style={styles.details}>
      <h3>{material.name}</h3>

      {/* Physical Properties */}
      <div style={styles.section}>
        <h4 style={styles.sectionTitle}>Physical Properties</h4>
        <div style={styles.propertyGrid}>
          <PropertyRow
            label="Density"
            value={`${material.density.toFixed(2)} g/cm³`}
          />
          <PropertyRow
            label="Young's Modulus"
            value={`${material.properties.youngsModulus.toFixed(0)} GPa`}
          />
          <PropertyRow
            label="Yield Strength"
            value={`${material.properties.yieldStrength.toFixed(0)} MPa`}
          />
          <PropertyRow
            label="Tensile Strength"
            value={`${material.properties.tensileStrength.toFixed(0)} MPa`}
          />
          <PropertyRow
            label="Elongation"
            value={`${material.properties.elongation.toFixed(0)}%`}
          />
          <PropertyRow
            label="Hardness"
            value={`${material.properties.hardness.toFixed(0)} HV`}
          />
        </div>
      </div>

      {/* Cost */}
      <div style={styles.section}>
        <h4 style={styles.sectionTitle}>Cost</h4>
        <div style={styles.costValue}>
          ${material.properties.cost.toFixed(2)}/kg
        </div>
      </div>

      {/* Standards */}
      {material.standards.length > 0 && (
        <div style={styles.section}>
          <h4 style={styles.sectionTitle}>Standards</h4>
          <div style={styles.standardsList}>
            {material.standards.map((std) => (
              <span key={std} style={styles.standardTag}>
                {std}
              </span>
            ))}
          </div>
        </div>
      )}

      {/* Applications */}
      {material.applications.length > 0 && (
        <div style={styles.section}>
          <h4 style={styles.sectionTitle}>Applications</h4>
          <ul style={styles.applicationsList}>
            {material.applications.map((app, idx) => (
              <li key={idx} style={styles.applicationsItem}>
                {app}
              </li>
            ))}
          </ul>
        </div>
      )}

      {/* Availability */}
      <div style={styles.section}>
        <h4 style={styles.sectionTitle}>Availability</h4>
        <div
          style={{
            ...styles.availabilityBadge,
            ...(material.availability === "Common"
              ? styles.availabilityCommon
              : material.availability === "Standard"
              ? styles.availabilityStandard
              : material.availability === "Premium"
              ? styles.availabilityPremium
              : styles.availabilityCustom)
          }}
        >
          {material.availability}
        </div>
      </div>

      {/* Category */}
      <div style={styles.section}>
        <h4 style={styles.sectionTitle}>Category</h4>
        <div style={styles.categoryBadge}>{material.category}</div>
      </div>
    </div>
  );
};

// ============ PROPERTY ROW COMPONENT ============

interface PropertyRowProps {
  label: string;
  value: string;
}

const PropertyRow: React.FC<PropertyRowProps> = ({ label, value }) => (
  <div style={styles.propertyRow}>
    <div style={styles.propertyLabel}>{label}:</div>
    <div style={styles.propertyValue}>{value}</div>
  </div>
);

// ============ CUSTOM MATERIAL FORM ============

interface CustomMaterialFormProps {
  onClose: () => void;
  onSubmit: (material: Material) => void;
}

const CustomMaterialForm: React.FC<CustomMaterialFormProps> = ({
  onClose,
  onSubmit
}) => {
  const [formData, setFormData] = useState<Partial<Material>>({
    id: "custom-",
    name: "",
    category: "Special",
    density: 5.0,
    properties: {
      youngsModulus: 200,
      yieldStrength: 400,
      tensileStrength: 500,
      elongation: 10,
      hardness: 200,
      cost: 0
    },
    standards: [],
    applications: [],
    availability: "Custom",
    color: "#808080"
  });

  const handleSubmit = (e: React.FormEvent) => {
    e.preventDefault();
    if (formData.id && formData.name) {
      onSubmit(formData as Material);
    }
  };

  return (
    <div style={styles.formOverlay}>
      <div style={styles.formContainer}>
        <h3>Add Custom Material</h3>
        <form onSubmit={handleSubmit}>
          <div style={styles.formGroup}>
            <label>Material ID:</label>
            <input
              type="text"
              value={formData.id || ""}
              onChange={(e) =>
                setFormData({ ...formData, id: e.target.value })
              }
              required
            />
          </div>

          <div style={styles.formGroup}>
            <label>Material Name:</label>
            <input
              type="text"
              value={formData.name || ""}
              onChange={(e) =>
                setFormData({ ...formData, name: e.target.value })
              }
              required
            />
          </div>

          <div style={styles.formGroup}>
            <label>Density (g/cm³):</label>
            <input
              type="number"
              step="0.01"
              value={formData.density || 5.0}
              onChange={(e) =>
                setFormData({
                  ...formData,
                  density: parseFloat(e.target.value)
                })
              }
            />
          </div>

          <div style={styles.formButtonsGroup}>
            <button
              type="submit"
              style={styles.submitButton}
            >
              Add Material
            </button>
            <button
              type="button"
              onClick={onClose}
              style={styles.cancelButton}
            >
              Cancel
            </button>
          </div>
        </form>
      </div>
    </div>
  );
};

// ============ STYLES ============

const styles: Record<string, React.CSSProperties> = {
  container: {
    position: "fixed",
    top: 0,
    left: 0,
    right: 0,
    bottom: 0,
    backgroundColor: "rgba(0, 0, 0, 0.5)",
    display: "flex",
    flexDirection: "column",
    zIndex: 1000,
    fontFamily: "-apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif"
  },

  header: {
    display: "flex",
    justifyContent: "space-between",
    alignItems: "center",
    padding: "16px 20px",
    backgroundColor: "#f5f5f5",
    borderBottom: "1px solid #ddd",
    color: "#333"
  },

  closeButton: {
    backgroundColor: "transparent",
    border: "none",
    fontSize: "24px",
    cursor: "pointer",
    padding: "0 8px",
    color: "#666"
  },

  controlsSection: {
    display: "flex",
    gap: "8px",
    padding: "12px 16px",
    backgroundColor: "#fafafa",
    borderBottom: "1px solid #e0e0e0"
  },

  searchInput: {
    flex: 1,
    padding: "8px 12px",
    border: "1px solid #ddd",
    borderRadius: "4px",
    fontSize: "14px"
  },

  categorySelect: {
    padding: "8px 12px",
    border: "1px solid #ddd",
    borderRadius: "4px",
    fontSize: "14px",
    minWidth: "150px"
  },

  addButton: {
    padding: "8px 12px",
    backgroundColor: "#4CAF50",
    color: "white",
    border: "none",
    borderRadius: "4px",
    cursor: "pointer",
    fontSize: "14px"
  },

  contentWrapper: {
    display: "flex",
    flex: 1,
    overflow: "hidden",
    backgroundColor: "white"
  },

  listSection: {
    flex: 1,
    display: "flex",
    flexDirection: "column",
    borderRight: "1px solid #e0e0e0",
    maxWidth: "400px"
  },

  listTitle: {
    padding: "12px 16px",
    margin: 0,
    fontSize: "14px",
    fontWeight: "600",
    color: "#666",
    borderBottom: "1px solid #e0e0e0"
  },

  materialsList: {
    flex: 1,
    overflowY: "auto",
    padding: "8px"
  },

  loadingMessage: {
    padding: "16px",
    textAlign: "center",
    color: "#999"
  },

  emptyMessage: {
    padding: "16px",
    textAlign: "center",
    color: "#999"
  },

  materialItem: {
    display: "flex",
    alignItems: "center",
    gap: "12px",
    padding: "8px 12px",
    marginBottom: "4px",
    borderRadius: "4px",
    cursor: "pointer",
    transition: "background-color 0.2s",
    backgroundColor: "#f9f9f9"
  },

  materialItemSelected: {
    backgroundColor: "#e3f2fd"
  },

  materialItemColor: {
    width: "24px",
    height: "24px",
    borderRadius: "4px",
    border: "1px solid #ddd"
  },

  materialItemColorInner: {
    width: "24px",
    height: "24px"
  },

  materialItemContent: {
    flex: 1,
    minWidth: 0
  },

  materialItemName: {
    fontSize: "13px",
    fontWeight: "500",
    color: "#333",
    overflow: "hidden",
    textOverflow: "ellipsis",
    whiteSpace: "nowrap"
  },

  materialItemMeta: {
    fontSize: "12px",
    color: "#999",
    overflow: "hidden",
    textOverflow: "ellipsis",
    whiteSpace: "nowrap"
  },

  materialItemDensity: {
    fontSize: "12px",
    color: "#666",
    minWidth: "50px",
    textAlign: "right"
  },

  detailsSection: {
    flex: 1,
    display: "flex",
    flexDirection: "column",
    overflowY: "auto",
    padding: "16px 20px"
  },

  noSelectionMessage: {
    display: "flex",
    alignItems: "center",
    justifyContent: "center",
    height: "100%",
    color: "#999",
    fontSize: "14px"
  },

  details: {
    color: "#333"
  },

  section: {
    marginBottom: "20px"
  },

  sectionTitle: {
    fontSize: "12px",
    fontWeight: "600",
    color: "#666",
    textTransform: "uppercase",
    margin: "0 0 8px 0",
    letterSpacing: "0.5px"
  },

  propertyGrid: {
    display: "grid",
    gridTemplateColumns: "1fr 1fr",
    gap: "8px"
  },

  propertyRow: {
    display: "flex",
    justifyContent: "space-between",
    fontSize: "13px",
    padding: "6px 0",
    borderBottom: "1px solid #f0f0f0"
  },

  propertyLabel: {
    color: "#666"
  },

  propertyValue: {
    color: "#333",
    fontWeight: "500"
  },

  costValue: {
    fontSize: "16px",
    fontWeight: "600",
    color: "#4CAF50",
    padding: "8px"
  },

  standardsList: {
    display: "flex",
    flexWrap: "wrap",
    gap: "6px"
  },

  standardTag: {
    backgroundColor: "#f0f0f0",
    padding: "4px 8px",
    borderRadius: "3px",
    fontSize: "12px",
    color: "#333"
  },

  applicationsList: {
    margin: 0,
    paddingLeft: "20px",
    fontSize: "13px",
    color: "#555"
  },

  applicationsItem: {
    marginBottom: "4px"
  },

  availabilityBadge: {
    display: "inline-block",
    padding: "4px 8px",
    borderRadius: "3px",
    fontSize: "12px",
    fontWeight: "500"
  },

  availabilityCommon: {
    backgroundColor: "#c8e6c9",
    color: "#2e7d32"
  },

  availabilityStandard: {
    backgroundColor: "#bbdefb",
    color: "#1565c0"
  },

  availabilityPremium: {
    backgroundColor: "#ffe0b2",
    color: "#e65100"
  },

  availabilityCustom: {
    backgroundColor: "#f0f0f0",
    color: "#333"
  },

  categoryBadge: {
    display: "inline-block",
    padding: "4px 8px",
    borderRadius: "3px",
    fontSize: "12px",
    backgroundColor: "#f0f0f0",
    color: "#333"
  },

  formOverlay: {
    position: "absolute",
    top: 0,
    left: 0,
    right: 0,
    bottom: 0,
    backgroundColor: "rgba(0, 0, 0, 0.7)",
    display: "flex",
    alignItems: "center",
    justifyContent: "center",
    zIndex: 1001
  },

  formContainer: {
    backgroundColor: "white",
    borderRadius: "8px",
    padding: "24px",
    maxWidth: "400px",
    width: "90%",
    boxShadow: "0 4px 12px rgba(0, 0, 0, 0.15)"
  },

  formGroup: {
    marginBottom: "16px"
  },

  formButtonsGroup: {
    display: "flex",
    gap: "8px",
    marginTop: "20px"
  },

  submitButton: {
    flex: 1,
    padding: "8px 16px",
    backgroundColor: "#4CAF50",
    color: "white",
    border: "none",
    borderRadius: "4px",
    cursor: "pointer",
    fontSize: "14px"
  },

  cancelButton: {
    flex: 1,
    padding: "8px 16px",
    backgroundColor: "#f0f0f0",
    color: "#333",
    border: "1px solid #ddd",
    borderRadius: "4px",
    cursor: "pointer",
    fontSize: "14px"
  }
};

export default MaterialsPanel;
