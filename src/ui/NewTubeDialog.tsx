/**
 * New Tube Dialog Component
 * 3-tab wizard for creating new tubes with profile-based configuration
 * Phase 35A: Profile Database and New Tube Dialog
 */

import React, { useState, useEffect } from "react";
import { getColorHex } from "./ColorScheme";

interface ProfileDiameter {
  id: string;
  OD: number;
  R: number;
  S_min: number;
  N: number;
  B_min: number;
  B_max: number;
  discrete_type: string;
  discrete_values?: number[];
}

interface BendingProfile {
  id: string;
  name: string;
  category: string;
  manufacturer: string;
  type: string;
  description: string;
  capacity_max_OD?: number;
  capacity_max_wall?: number;
  diameters: ProfileDiameter[];
}

interface TubeFormData {
  // Tab 1: Specification
  standard: string;
  material: string;
  type: string;
  description: string;
  // Tab 2: Tool Profile
  profileId: string;
  diameter: number;
  radius: number;
  wallThickness: number;
  // Tab 3: Blank
  trimming: number;
  firstSegmentAllowance: number;
  lastSegmentAllowance: number;
  calculateAllowance: boolean;
}

interface NewTubeDialogProps {
  isOpen: boolean;
  onClose: () => void;
  onCreate?: (data: TubeFormData) => void;
}

interface ValidationError {
  [key: string]: string;
}

const STANDARDS = [
  "ANSI/ASME",
  "DIN",
  "ISO",
  "ASTM",
  "SAE",
  "EN",
  "Custom",
];

const MATERIALS = [
  { id: "steel-1020", name: "Steel 1020" },
  { id: "steel-1045", name: "Steel 1045" },
  { id: "ss-304", name: "Stainless Steel 304" },
  { id: "ss-316", name: "Stainless Steel 316" },
  { id: "aluminum-6061", name: "Aluminum 6061" },
  { id: "copper", name: "Copper" },
  { id: "titanium", name: "Titanium" },
];

const TUBE_TYPES = ["Tube", "Pipe", "Structural", "Hose"];

export const NewTubeDialog: React.FC<NewTubeDialogProps> = ({
  isOpen,
  onClose,
  onCreate,
}) => {
  const [currentTab, setCurrentTab] = useState(0);
  const [profiles, setProfiles] = useState<BendingProfile[]>([]);
  const [selectedDiameters, setSelectedDiameters] = useState<
    ProfileDiameter[]
  >([]);
  const [errors, setErrors] = useState<ValidationError>({});

  const [formData, setFormData] = useState<TubeFormData>({
    standard: "ANSI/ASME",
    material: "steel-1020",
    type: "Tube",
    description: "",
    profileId: "",
    diameter: 0,
    radius: 0,
    wallThickness: 0.065,
    trimming: 0.5,
    firstSegmentAllowance: 0.25,
    lastSegmentAllowance: 0.25,
    calculateAllowance: true,
  });

  // Load profiles on mount
  useEffect(() => {
    if (isOpen) {
      loadProfiles();
    }
  }, [isOpen]);

  // Update available diameters when profile changes
  useEffect(() => {
    if (formData.profileId) {
      const profile = profiles.find((p) => p.id === formData.profileId);
      if (profile) {
        setSelectedDiameters(profile.diameters);
        if (profile.diameters.length > 0) {
          setFormData((prev) => ({
            ...prev,
            diameter: profile.diameters[0].OD,
            radius: profile.diameters[0].R,
          }));
        }
      }
    }
  }, [formData.profileId, profiles]);

  // Update radius when diameter changes
  useEffect(() => {
    if (formData.profileId && formData.diameter > 0) {
      const diameter = selectedDiameters.find(
        (d) => Math.abs(d.OD - formData.diameter) < 0.001
      );
      if (diameter) {
        setFormData((prev) => ({ ...prev, radius: diameter.R }));
      }
    }
  }, [formData.diameter, selectedDiameters, formData.profileId]);

  const loadProfiles = async () => {
    try {
      const response = await fetch("./data/bending_profiles.json");
      const data = await response.json();
      setProfiles(data.profiles || []);
      if (data.profiles && data.profiles.length > 0) {
        setFormData((prev) => ({ ...prev, profileId: data.profiles[0].id }));
      }
    } catch (error) {
      console.error("Failed to load profiles:", error);
    }
  };

  const validateTab = (tabIndex: number): boolean => {
    const newErrors: ValidationError = {};

    if (tabIndex === 0) {
      // Tab 1: Specification
      if (!formData.material) {
        newErrors.material = "Material is required";
      }
      if (!formData.type) {
        newErrors.type = "Tube type is required";
      }
    } else if (tabIndex === 1) {
      // Tab 2: Tool Profile
      if (!formData.profileId) {
        newErrors.profileId = "Profile is required";
      }
      if (formData.diameter <= 0) {
        newErrors.diameter = "Diameter must be positive";
      }
      if (formData.wallThickness <= 0) {
        newErrors.wallThickness = "Wall thickness must be positive";
      }
      if (formData.wallThickness >= formData.diameter) {
        newErrors.wallThickness =
          "Wall thickness must be less than diameter";
      }
    } else if (tabIndex === 2) {
      // Tab 3: Blank
      if (formData.trimming < 0) {
        newErrors.trimming = "Trimming must be non-negative";
      }
      if (formData.firstSegmentAllowance < 0) {
        newErrors.firstSegmentAllowance =
          "First segment allowance must be non-negative";
      }
      if (formData.lastSegmentAllowance < 0) {
        newErrors.lastSegmentAllowance =
          "Last segment allowance must be non-negative";
      }
    }

    setErrors(newErrors);
    return Object.keys(newErrors).length === 0;
  };

  const handleNext = () => {
    if (validateTab(currentTab)) {
      if (currentTab < 2) {
        setCurrentTab(currentTab + 1);
      }
    }
  };

  const handleBack = () => {
    if (currentTab > 0) {
      setCurrentTab(currentTab - 1);
    }
  };

  const handleCreate = () => {
    if (validateTab(currentTab) && onCreate) {
      onCreate(formData);
      handleClose();
    }
  };

  const handleClose = () => {
    setCurrentTab(0);
    setErrors({});
    setFormData({
      standard: "ANSI/ASME",
      material: "steel-1020",
      type: "Tube",
      description: "",
      profileId: profiles[0]?.id || "",
      diameter: 0,
      radius: 0,
      wallThickness: 0.065,
      trimming: 0.5,
      firstSegmentAllowance: 0.25,
      lastSegmentAllowance: 0.25,
      calculateAllowance: true,
    });
    onClose();
  };

  const handleInputChange = (field: keyof TubeFormData, value: any) => {
    setFormData((prev) => ({ ...prev, [field]: value }));
    if (errors[field]) {
      setErrors((prev) => {
        const newErrors = { ...prev };
        delete newErrors[field];
        return newErrors;
      });
    }
  };

  const getInputStyle = (field: string) => ({
    width: "100%",
    padding: "10px 12px",
    marginTop: "6px",
    border: errors[field]
      ? `2px solid #e74c3c`
      : `1px solid ${getColorHex("border")}`,
    borderRadius: "4px",
    backgroundColor: getColorHex("input"),
    color: getColorHex("text"),
    boxSizing: "border-box" as const,
    fontSize: "14px",
  });

  if (!isOpen) return null;

  return (
    <div
      style={{
        position: "fixed",
        top: 0,
        left: 0,
        width: "100%",
        height: "100%",
        backgroundColor: "rgba(0, 0, 0, 0.5)",
        display: "flex",
        alignItems: "center",
        justifyContent: "center",
        zIndex: 1000,
      }}
    >
      <div
        style={{
          backgroundColor: getColorHex("surface"),
          borderRadius: "8px",
          width: "90%",
          maxWidth: "700px",
          boxShadow: "0 4px 20px rgba(0, 0, 0, 0.3)",
          display: "flex",
          flexDirection: "column",
          maxHeight: "90vh",
          overflow: "hidden",
        }}
      >
        {/* Header with Tabs */}
        <div
          style={{
            borderBottom: `1px solid ${getColorHex("border")}`,
            padding: "0 24px",
          }}
        >
          <h2 style={{ margin: "16px 0 0 0", color: getColorHex("text") }}>
            Create New Tube
          </h2>

          <div
            style={{
              display: "flex",
              gap: "24px",
              marginTop: "16px",
              borderBottom: `2px solid ${getColorHex("border")}`,
            }}
          >
            {[
              { idx: 0, label: "Specification" },
              { idx: 1, label: "Tool Profile" },
              { idx: 2, label: "Blank" },
            ].map(({ idx, label }) => (
              <button
                key={idx}
                onClick={() => {
                  if (validateTab(currentTab)) {
                    setCurrentTab(idx);
                  }
                }}
                style={{
                  padding: "12px 0",
                  marginBottom: "-1px",
                  backgroundColor: "transparent",
                  border: "none",
                  borderBottom:
                    currentTab === idx
                      ? `3px solid ${getColorHex("primary")}`
                      : "3px solid transparent",
                  color:
                    currentTab === idx
                      ? getColorHex("primary")
                      : getColorHex("textSecondary"),
                  cursor: "pointer",
                  fontSize: "14px",
                  fontWeight: currentTab === idx ? "bold" : "normal",
                }}
              >
                {idx + 1}. {label}
              </button>
            ))}
          </div>
        </div>

        {/* Content Area */}
        <div
          style={{
            flex: 1,
            padding: "24px",
            overflowY: "auto",
            color: getColorHex("text"),
          }}
        >
          {currentTab === 0 && (
            // Tab 1: Specification
            <div>
              <h3 style={{ marginTop: 0 }}>Tube Specification</h3>

              <div style={{ marginBottom: "16px" }}>
                <label>Standard:</label>
                <select
                  value={formData.standard}
                  onChange={(e) =>
                    handleInputChange("standard", e.target.value)
                  }
                  style={getInputStyle("standard")}
                >
                  {STANDARDS.map((std) => (
                    <option key={std} value={std}>
                      {std}
                    </option>
                  ))}
                </select>
              </div>

              <div style={{ marginBottom: "16px" }}>
                <label>
                  Material: {errors.material && <span style={{ color: "#e74c3c" }}> {errors.material}</span>}
                </label>
                <select
                  value={formData.material}
                  onChange={(e) =>
                    handleInputChange("material", e.target.value)
                  }
                  style={getInputStyle("material")}
                >
                  {MATERIALS.map((mat) => (
                    <option key={mat.id} value={mat.id}>
                      {mat.name}
                    </option>
                  ))}
                </select>
              </div>

              <div style={{ marginBottom: "16px" }}>
                <label>
                  Tube Type: {errors.type && <span style={{ color: "#e74c3c" }}> {errors.type}</span>}
                </label>
                <select
                  value={formData.type}
                  onChange={(e) => handleInputChange("type", e.target.value)}
                  style={getInputStyle("type")}
                >
                  {TUBE_TYPES.map((type) => (
                    <option key={type} value={type}>
                      {type}
                    </option>
                  ))}
                </select>
              </div>

              <div style={{ marginBottom: "16px" }}>
                <label>Description (optional):</label>
                <textarea
                  value={formData.description}
                  onChange={(e) =>
                    handleInputChange("description", e.target.value)
                  }
                  style={{
                    ...getInputStyle("description"),
                    height: "80px",
                    fontFamily: "monospace",
                  }}
                  placeholder="Enter tube description..."
                />
              </div>
            </div>
          )}

          {currentTab === 1 && (
            // Tab 2: Tool Profile
            <div>
              <h3 style={{ marginTop: 0 }}>Bending Tool Profile</h3>

              <div style={{ marginBottom: "16px" }}>
                <label>
                  Profile: {errors.profileId && <span style={{ color: "#e74c3c" }}> {errors.profileId}</span>}
                </label>
                <select
                  value={formData.profileId}
                  onChange={(e) =>
                    handleInputChange("profileId", e.target.value)
                  }
                  style={getInputStyle("profileId")}
                >
                  <option value="">Select a profile...</option>
                  {profiles.map((profile) => (
                    <option key={profile.id} value={profile.id}>
                      {profile.name} ({profile.category})
                    </option>
                  ))}
                </select>
              </div>

              {formData.profileId && (
                <>
                  <div style={{ marginBottom: "16px" }}>
                    <label>
                      Diameter (OD):
                      {errors.diameter && (
                        <span style={{ color: "#e74c3c" }}> {errors.diameter}</span>
                      )}
                    </label>
                    <select
                      value={formData.diameter}
                      onChange={(e) =>
                        handleInputChange("diameter", parseFloat(e.target.value))
                      }
                      style={getInputStyle("diameter")}
                    >
                      <option value="">Select diameter...</option>
                      {selectedDiameters.map((diam) => (
                        <option key={diam.id} value={diam.OD}>
                          {diam.OD}" (R: {diam.R}", Min S: {diam.S_min}")
                        </option>
                      ))}
                    </select>
                  </div>

                  <div style={{ marginBottom: "16px" }}>
                    <label>Bend Radius (read-only):</label>
                    <div
                      style={{
                        ...getInputStyle("radius"),
                        backgroundColor: getColorHex("inputDisabled"),
                        cursor: "not-allowed",
                        padding: "10px 12px",
                        marginTop: "6px",
                      }}
                    >
                      {formData.radius}"
                    </div>
                  </div>
                </>
              )}

              <div style={{ marginBottom: "16px" }}>
                <label>
                  Wall Thickness:
                  {errors.wallThickness && (
                    <span style={{ color: "#e74c3c" }}> {errors.wallThickness}</span>
                  )}
                </label>
                <input
                  type="number"
                  step={0.001}
                  value={formData.wallThickness}
                  onChange={(e) =>
                    handleInputChange(
                      "wallThickness",
                      parseFloat(e.target.value) || 0
                    )
                  }
                  style={getInputStyle("wallThickness")}
                  placeholder='Wall thickness (inches)'
                />
              </div>
            </div>
          )}

          {currentTab === 2 && (
            // Tab 3: Blank
            <div>
              <h3 style={{ marginTop: 0 }}>Blank Configuration</h3>

              <div style={{ marginBottom: "16px" }}>
                <label>Trimming:</label>
                <input
                  type="number"
                  step={0.01}
                  value={formData.trimming}
                  onChange={(e) =>
                    handleInputChange("trimming", parseFloat(e.target.value) || 0)
                  }
                  style={getInputStyle("trimming")}
                  placeholder='Trimming allowance'
                />
                {errors.trimming && (
                  <div style={{ color: "#e74c3c", fontSize: "12px", marginTop: "4px" }}>
                    {errors.trimming}
                  </div>
                )}
              </div>

              <div style={{ marginBottom: "16px" }}>
                <label>First Segment Allowance:</label>
                <input
                  type="number"
                  step={0.01}
                  value={formData.firstSegmentAllowance}
                  onChange={(e) =>
                    handleInputChange(
                      "firstSegmentAllowance",
                      parseFloat(e.target.value) || 0
                    )
                  }
                  style={getInputStyle("firstSegmentAllowance")}
                  placeholder='First segment allowance'
                />
                {errors.firstSegmentAllowance && (
                  <div style={{ color: "#e74c3c", fontSize: "12px", marginTop: "4px" }}>
                    {errors.firstSegmentAllowance}
                  </div>
                )}
              </div>

              <div style={{ marginBottom: "16px" }}>
                <label>Last Segment Allowance:</label>
                <input
                  type="number"
                  step={0.01}
                  value={formData.lastSegmentAllowance}
                  onChange={(e) =>
                    handleInputChange(
                      "lastSegmentAllowance",
                      parseFloat(e.target.value) || 0
                    )
                  }
                  style={getInputStyle("lastSegmentAllowance")}
                  placeholder='Last segment allowance'
                />
                {errors.lastSegmentAllowance && (
                  <div style={{ color: "#e74c3c", fontSize: "12px", marginTop: "4px" }}>
                    {errors.lastSegmentAllowance}
                  </div>
                )}
              </div>

              <div
                style={{
                  padding: "12px",
                  backgroundColor: getColorHex("inputBackground"),
                  borderRadius: "4px",
                  marginBottom: "16px",
                }}
              >
                <label
                  style={{
                    display: "flex",
                    alignItems: "center",
                    cursor: "pointer",
                  }}
                >
                  <input
                    type="checkbox"
                    checked={formData.calculateAllowance}
                    onChange={(e) =>
                      handleInputChange("calculateAllowance", e.target.checked)
                    }
                    style={{ marginRight: "8px", cursor: "pointer" }}
                  />
                  Automatically calculate trimming allowance
                </label>
              </div>
            </div>
          )}
        </div>

        {/* Footer */}
        <div
          style={{
            padding: "16px 24px",
            borderTop: `1px solid ${getColorHex("border")}`,
            display: "flex",
            justifyContent: "space-between",
            alignItems: "center",
            gap: "8px",
          }}
        >
          <div style={{ display: "flex", gap: "8px" }}>
            <button
              onClick={handleBack}
              disabled={currentTab === 0}
              style={{
                padding: "10px 20px",
                backgroundColor:
                  currentTab === 0
                    ? getColorHex("inputDisabled")
                    : getColorHex("secondary"),
                color: "white",
                border: "none",
                borderRadius: "4px",
                cursor: currentTab === 0 ? "not-allowed" : "pointer",
                opacity: currentTab === 0 ? 0.5 : 1,
              }}
            >
              ← Back
            </button>
          </div>

          <div style={{ display: "flex", gap: "8px" }}>
            <button
              onClick={handleClose}
              style={{
                padding: "10px 20px",
                backgroundColor: getColorHex("border"),
                color: getColorHex("text"),
                border: "none",
                borderRadius: "4px",
                cursor: "pointer",
              }}
            >
              Cancel
            </button>

            {currentTab < 2 && (
              <button
                onClick={handleNext}
                style={{
                  padding: "10px 20px",
                  backgroundColor: getColorHex("primary"),
                  color: "white",
                  border: "none",
                  borderRadius: "4px",
                  cursor: "pointer",
                }}
              >
                Next →
              </button>
            )}

            {currentTab === 2 && (
              <button
                onClick={handleCreate}
                style={{
                  padding: "10px 20px",
                  backgroundColor: getColorHex("primary"),
                  color: "white",
                  border: "none",
                  borderRadius: "4px",
                  cursor: "pointer",
                }}
              >
                Create ✓
              </button>
            )}
          </div>
        </div>
      </div>
    </div>
  );
};

export default NewTubeDialog;
