/**
 * Profile Dialog Component
 * Manage bending profiles with full table editing, validation, and export
 * Phase 35A: Profile Database and New Tube Dialog
 */

import React, { useState, useCallback, useEffect } from "react";
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

interface ProfileDialogProps {
  isOpen: boolean;
  onClose: () => void;
  onSave?: (profile: BendingProfile) => void;
}

interface ValidationState {
  [key: string]: "valid" | "warning" | "error";
}

export const ProfileDialog: React.FC<ProfileDialogProps> = ({
  isOpen,
  onClose,
  onSave,
}) => {
  const [profiles, setProfiles] = useState<BendingProfile[]>([]);
  const [selectedProfileId, setSelectedProfileId] = useState<string>("");
  const [searchTerm, setSearchTerm] = useState("");
  const [editMode, setEditMode] = useState(false);
  const [editingProfile, setEditingProfile] = useState<BendingProfile | null>(
    null
  );
  const [validationState, setValidationState] = useState<ValidationState>({});

  // Load profiles on mount
  useEffect(() => {
    if (isOpen) {
      loadProfiles();
    }
  }, [isOpen]);

  const loadProfiles = async () => {
    try {
      const response = await fetch("./data/bending_profiles.json");
      const data = await response.json();
      setProfiles(data.profiles || []);
      if (data.profiles && data.profiles.length > 0) {
        setSelectedProfileId(data.profiles[0].id);
      }
    } catch (error) {
      console.error("Failed to load profiles:", error);
    }
  };

  const filteredProfiles = profiles.filter(
    (p) =>
      p.name.toLowerCase().includes(searchTerm.toLowerCase()) ||
      p.category.toLowerCase().includes(searchTerm.toLowerCase()) ||
      p.manufacturer.toLowerCase().includes(searchTerm.toLowerCase())
  );

  const selectedProfile =
    profiles.find((p) => p.id === selectedProfileId) ||
    profiles[0];

  const handleEditClick = () => {
    if (selectedProfile) {
      setEditingProfile(JSON.parse(JSON.stringify(selectedProfile)));
      setEditMode(true);
      validateProfile(selectedProfile);
    }
  };

  const handleSaveEdit = () => {
    if (editingProfile) {
      setProfiles((prev) =>
        prev.map((p) => (p.id === editingProfile.id ? editingProfile : p))
      );
      setEditMode(false);
      setEditingProfile(null);

      if (onSave) {
        onSave(editingProfile);
      }
    }
  };

  const handleDiameterChange = (
    diameterIndex: number,
    field: keyof ProfileDiameter,
    value: any
  ) => {
    if (!editingProfile) return;

    const updated = JSON.parse(JSON.stringify(editingProfile));
    updated.diameters[diameterIndex][field] = value;
    setEditingProfile(updated);
    validateProfile(updated);
  };

  const validateProfile = (profile: BendingProfile) => {
    const errors: ValidationState = {};

    // Validate basic fields
    if (!profile.name || profile.name.trim().length === 0) {
      errors.name = "error";
    }
    if (!profile.manufacturer || profile.manufacturer.trim().length === 0) {
      errors.manufacturer = "error";
    }

    // Validate diameters
    profile.diameters.forEach((diam, idx) => {
      const prefix = `diameter_${idx}`;

      if (diam.OD <= 0) errors[`${prefix}_OD`] = "error";
      if (diam.R <= 0) errors[`${prefix}_R`] = "error";
      if (diam.S_min < 0) errors[`${prefix}_S_min`] = "error";
      if (diam.N <= 0) errors[`${prefix}_N`] = "error";
      if (diam.B_min < 0) errors[`${prefix}_B_min`] = "error";
      if (diam.B_max <= diam.B_min)
        errors[`${prefix}_B_max`] = "error";
      if (diam.B_max > 179) errors[`${prefix}_B_max`] = "warning";

      // Cross-diameter validation
      if (diam.R < diam.OD * 0.5) {
        errors[`${prefix}_R`] = "warning";
      }
    });

    setValidationState(errors);
  };

  const getValidationColor = (field: string) => {
    const state = validationState[field];
    if (state === "error") return "#e74c3c";
    if (state === "warning") return "#f39c12";
    return "transparent";
  };

  const handleExport = () => {
    if (selectedProfile) {
      const dataStr = JSON.stringify(selectedProfile, null, 2);
      const dataBlob = new Blob([dataStr], { type: "application/json" });
      const url = URL.createObjectURL(dataBlob);
      const link = document.createElement("a");
      link.href = url;
      link.download = `${selectedProfile.id}.json`;
      link.click();
      URL.revokeObjectURL(url);
    }
  };

  const handleImport = (event: React.ChangeEvent<HTMLInputElement>) => {
    const file = event.target.files?.[0];
    if (!file) return;

    const reader = new FileReader();
    reader.onload = (e) => {
      try {
        const content = e.target?.result as string;
        const imported = JSON.parse(content) as BendingProfile;
        setProfiles((prev) => {
          const existing = prev.find((p) => p.id === imported.id);
          if (existing) {
            return prev.map((p) => (p.id === imported.id ? imported : p));
          }
          return [...prev, imported];
        });
        setSelectedProfileId(imported.id);
      } catch (error) {
        console.error("Failed to import profile:", error);
        alert("Failed to import profile. Please check the file format.");
      }
    };
    reader.readAsText(file);
  };

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
          maxWidth: "1200px",
          height: "80vh",
          display: "flex",
          flexDirection: "column",
          boxShadow: "0 4px 20px rgba(0, 0, 0, 0.3)",
        }}
      >
        {/* Header */}
        <div
          style={{
            padding: "16px 24px",
            borderBottom: `1px solid ${getColorHex("border")}`,
            display: "flex",
            justifyContent: "space-between",
            alignItems: "center",
          }}
        >
          <h2 style={{ margin: 0, color: getColorHex("text") }}>
            {editMode ? "Edit Profile" : "Profile Manager"}
          </h2>
          <button
            onClick={onClose}
            style={{
              background: "none",
              border: "none",
              fontSize: "20px",
              cursor: "pointer",
              color: getColorHex("text"),
            }}
          >
            ×
          </button>
        </div>

        <div style={{ display: "flex", flex: 1, overflow: "hidden" }}>
          {/* Left Panel: Profile List */}
          {!editMode && (
            <div
              style={{
                width: "25%",
                borderRight: `1px solid ${getColorHex("border")}`,
                display: "flex",
                flexDirection: "column",
                overflow: "hidden",
              }}
            >
              <div style={{ padding: "12px" }}>
                <input
                  type="text"
                  placeholder="Search profiles..."
                  value={searchTerm}
                  onChange={(e) => setSearchTerm(e.target.value)}
                  style={{
                    width: "100%",
                    padding: "8px 12px",
                    border: `1px solid ${getColorHex("border")}`,
                    borderRadius: "4px",
                    backgroundColor: getColorHex("input"),
                    color: getColorHex("text"),
                    boxSizing: "border-box",
                  }}
                />
              </div>

              <div
                style={{
                  flex: 1,
                  overflowY: "auto",
                  padding: "8px",
                }}
              >
                {filteredProfiles.map((profile) => (
                  <div
                    key={profile.id}
                    onClick={() => setSelectedProfileId(profile.id)}
                    style={{
                      padding: "12px",
                      marginBottom: "4px",
                      borderRadius: "4px",
                      backgroundColor:
                        profile.id === selectedProfileId
                          ? getColorHex("highlight")
                          : "transparent",
                      cursor: "pointer",
                      color: getColorHex("text"),
                    }}
                  >
                    <div style={{ fontWeight: "bold", fontSize: "14px" }}>
                      {profile.name}
                    </div>
                    <div
                      style={{
                        fontSize: "12px",
                        color: getColorHex("textSecondary"),
                      }}
                    >
                      {profile.category}
                    </div>
                  </div>
                ))}
              </div>
            </div>
          )}

          {/* Center/Main Panel: Profile Details or Editor */}
          <div
            style={{
              flex: 1,
              padding: "24px",
              overflowY: "auto",
              display: "flex",
              flexDirection: "column",
            }}
          >
            {editMode && editingProfile ? (
              // Edit Mode
              <div>
                <h3 style={{ color: getColorHex("text"), marginTop: 0 }}>
                  Basic Information
                </h3>

                <div style={{ marginBottom: "16px" }}>
                  <label style={{ color: getColorHex("text") }}>
                    Name:
                  </label>
                  <input
                    type="text"
                    value={editingProfile.name}
                    onChange={(e) => {
                      const updated = { ...editingProfile };
                      updated.name = e.target.value;
                      setEditingProfile(updated);
                    }}
                    style={{
                      width: "100%",
                      padding: "8px 12px",
                      marginTop: "4px",
                      border: `2px solid ${getValidationColor("name")}`,
                      borderRadius: "4px",
                      backgroundColor: getColorHex("input"),
                      color: getColorHex("text"),
                      boxSizing: "border-box",
                    }}
                  />
                </div>

                <div style={{ marginBottom: "16px" }}>
                  <label style={{ color: getColorHex("text") }}>
                    Manufacturer:
                  </label>
                  <input
                    type="text"
                    value={editingProfile.manufacturer}
                    onChange={(e) => {
                      const updated = { ...editingProfile };
                      updated.manufacturer = e.target.value;
                      setEditingProfile(updated);
                    }}
                    style={{
                      width: "100%",
                      padding: "8px 12px",
                      marginTop: "4px",
                      border: `2px solid ${getValidationColor("manufacturer")}`,
                      borderRadius: "4px",
                      backgroundColor: getColorHex("input"),
                      color: getColorHex("text"),
                      boxSizing: "border-box",
                    }}
                  />
                </div>

                <h3 style={{ color: getColorHex("text") }}>Diameters</h3>
                <div style={{ overflowX: "auto" }}>
                  <table
                    style={{
                      width: "100%",
                      borderCollapse: "collapse",
                      color: getColorHex("text"),
                    }}
                  >
                    <thead>
                      <tr style={{ borderBottom: `2px solid ${getColorHex("border")}` }}>
                        <th style={{ padding: "8px", textAlign: "left" }}>OD</th>
                        <th style={{ padding: "8px", textAlign: "left" }}>R</th>
                        <th style={{ padding: "8px", textAlign: "left" }}>S_min</th>
                        <th style={{ padding: "8px", textAlign: "left" }}>N</th>
                        <th style={{ padding: "8px", textAlign: "left" }}>B_min</th>
                        <th style={{ padding: "8px", textAlign: "left" }}>B_max</th>
                      </tr>
                    </thead>
                    <tbody>
                      {editingProfile.diameters.map((diam, idx) => (
                        <tr
                          key={idx}
                          style={{
                            borderBottom: `1px solid ${getColorHex("border")}`,
                          }}
                        >
                          <td style={{ padding: "8px" }}>
                            <input
                              type="number"
                              step={0.01}
                              value={diam.OD}
                              onChange={(e) =>
                                handleDiameterChange(
                                  idx,
                                  "OD",
                                  parseFloat(e.target.value) || 0
                                )
                              }
                              style={{
                                width: "80px",
                                padding: "4px 8px",
                                border: `2px solid ${getValidationColor(`diameter_${idx}_OD`)}`,
                                borderRadius: "2px",
                                backgroundColor: getColorHex("input"),
                                color: getColorHex("text"),
                              }}
                            />
                          </td>
                          <td style={{ padding: "8px" }}>
                            <input
                              type="number"
                              step={0.01}
                              value={diam.R}
                              onChange={(e) =>
                                handleDiameterChange(
                                  idx,
                                  "R",
                                  parseFloat(e.target.value) || 0
                                )
                              }
                              style={{
                                width: "80px",
                                padding: "4px 8px",
                                border: `2px solid ${getValidationColor(`diameter_${idx}_R`)}`,
                                borderRadius: "2px",
                                backgroundColor: getColorHex("input"),
                                color: getColorHex("text"),
                              }}
                            />
                          </td>
                          <td style={{ padding: "8px" }}>
                            <input
                              type="number"
                              step={0.01}
                              value={diam.S_min}
                              onChange={(e) =>
                                handleDiameterChange(
                                  idx,
                                  "S_min",
                                  parseFloat(e.target.value) || 0
                                )
                              }
                              style={{
                                width: "80px",
                                padding: "4px 8px",
                                border: `2px solid ${getValidationColor(`diameter_${idx}_S_min`)}`,
                                borderRadius: "2px",
                                backgroundColor: getColorHex("input"),
                                color: getColorHex("text"),
                              }}
                            />
                          </td>
                          <td style={{ padding: "8px" }}>
                            <input
                              type="number"
                              value={diam.N}
                              onChange={(e) =>
                                handleDiameterChange(
                                  idx,
                                  "N",
                                  parseInt(e.target.value) || 1
                                )
                              }
                              style={{
                                width: "60px",
                                padding: "4px 8px",
                                border: `2px solid ${getValidationColor(`diameter_${idx}_N`)}`,
                                borderRadius: "2px",
                                backgroundColor: getColorHex("input"),
                                color: getColorHex("text"),
                              }}
                            />
                          </td>
                          <td style={{ padding: "8px" }}>
                            <input
                              type="number"
                              step={0.5}
                              value={diam.B_min}
                              onChange={(e) =>
                                handleDiameterChange(
                                  idx,
                                  "B_min",
                                  parseFloat(e.target.value) || 0
                                )
                              }
                              style={{
                                width: "80px",
                                padding: "4px 8px",
                                border: `2px solid ${getValidationColor(`diameter_${idx}_B_min`)}`,
                                borderRadius: "2px",
                                backgroundColor: getColorHex("input"),
                                color: getColorHex("text"),
                              }}
                            />
                          </td>
                          <td style={{ padding: "8px" }}>
                            <input
                              type="number"
                              step={0.5}
                              value={diam.B_max}
                              onChange={(e) =>
                                handleDiameterChange(
                                  idx,
                                  "B_max",
                                  parseFloat(e.target.value) || 0
                                )
                              }
                              style={{
                                width: "80px",
                                padding: "4px 8px",
                                border: `2px solid ${getValidationColor(`diameter_${idx}_B_max`)}`,
                                borderRadius: "2px",
                                backgroundColor: getColorHex("input"),
                                color: getColorHex("text"),
                              }}
                            />
                          </td>
                        </tr>
                      ))}
                    </tbody>
                  </table>
                </div>
              </div>
            ) : selectedProfile ? (
              // View Mode
              <div>
                <h3 style={{ color: getColorHex("text"), marginTop: 0 }}>
                  {selectedProfile.name}
                </h3>

                <div
                  style={{
                    display: "grid",
                    gridTemplateColumns: "1fr 1fr",
                    gap: "16px",
                    marginBottom: "24px",
                  }}
                >
                  <div>
                    <div
                      style={{
                        color: getColorHex("textSecondary"),
                        fontSize: "12px",
                        textTransform: "uppercase",
                      }}
                    >
                      Category
                    </div>
                    <div style={{ color: getColorHex("text") }}>
                      {selectedProfile.category}
                    </div>
                  </div>
                  <div>
                    <div
                      style={{
                        color: getColorHex("textSecondary"),
                        fontSize: "12px",
                        textTransform: "uppercase",
                      }}
                    >
                      Manufacturer
                    </div>
                    <div style={{ color: getColorHex("text") }}>
                      {selectedProfile.manufacturer}
                    </div>
                  </div>
                  <div>
                    <div
                      style={{
                        color: getColorHex("textSecondary"),
                        fontSize: "12px",
                        textTransform: "uppercase",
                      }}
                    >
                      Type
                    </div>
                    <div style={{ color: getColorHex("text") }}>
                      {selectedProfile.type}
                    </div>
                  </div>
                  <div>
                    <div
                      style={{
                        color: getColorHex("textSecondary"),
                        fontSize: "12px",
                        textTransform: "uppercase",
                      }}
                    >
                      Supported Diameters
                    </div>
                    <div style={{ color: getColorHex("text") }}>
                      {selectedProfile.diameters.length}
                    </div>
                  </div>
                </div>

                <h4 style={{ color: getColorHex("text") }}>
                  Supported Diameters
                </h4>
                <div style={{ overflowX: "auto" }}>
                  <table
                    style={{
                      width: "100%",
                      borderCollapse: "collapse",
                      color: getColorHex("text"),
                      fontSize: "13px",
                    }}
                  >
                    <thead>
                      <tr
                        style={{
                          borderBottom: `2px solid ${getColorHex("border")}`,
                        }}
                      >
                        <th style={{ padding: "8px", textAlign: "left" }}>
                          OD (in)
                        </th>
                        <th style={{ padding: "8px", textAlign: "left" }}>
                          R (in)
                        </th>
                        <th style={{ padding: "8px", textAlign: "left" }}>
                          S_min (in)
                        </th>
                        <th style={{ padding: "8px", textAlign: "left" }}>N</th>
                        <th style={{ padding: "8px", textAlign: "left" }}>
                          B_min (°)
                        </th>
                        <th style={{ padding: "8px", textAlign: "left" }}>
                          B_max (°)
                        </th>
                      </tr>
                    </thead>
                    <tbody>
                      {selectedProfile.diameters.map((diam, idx) => (
                        <tr
                          key={idx}
                          style={{
                            borderBottom: `1px solid ${getColorHex("border")}`,
                          }}
                        >
                          <td style={{ padding: "8px" }}>{diam.OD}</td>
                          <td style={{ padding: "8px" }}>{diam.R}</td>
                          <td style={{ padding: "8px" }}>{diam.S_min}</td>
                          <td style={{ padding: "8px" }}>{diam.N}</td>
                          <td style={{ padding: "8px" }}>{diam.B_min}</td>
                          <td style={{ padding: "8px" }}>{diam.B_max}</td>
                        </tr>
                      ))}
                    </tbody>
                  </table>
                </div>
              </div>
            ) : null}
          </div>
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
            {!editMode && (
              <>
                <button
                  onClick={handleEditClick}
                  disabled={!selectedProfile}
                  style={{
                    padding: "8px 16px",
                    backgroundColor: getColorHex("primary"),
                    color: "white",
                    border: "none",
                    borderRadius: "4px",
                    cursor: "pointer",
                  }}
                >
                  Edit
                </button>
                <button
                  onClick={handleExport}
                  disabled={!selectedProfile}
                  style={{
                    padding: "8px 16px",
                    backgroundColor: getColorHex("secondary"),
                    color: "white",
                    border: "none",
                    borderRadius: "4px",
                    cursor: "pointer",
                  }}
                >
                  Export
                </button>
                <label
                  style={{
                    padding: "8px 16px",
                    backgroundColor: getColorHex("secondary"),
                    color: "white",
                    border: "none",
                    borderRadius: "4px",
                    cursor: "pointer",
                    display: "inline-block",
                  }}
                >
                  Import
                  <input
                    type="file"
                    accept=".json"
                    onChange={handleImport}
                    style={{ display: "none" }}
                  />
                </label>
              </>
            )}
          </div>

          <div style={{ display: "flex", gap: "8px" }}>
            {editMode && (
              <button
                onClick={() => {
                  setEditMode(false);
                  setEditingProfile(null);
                }}
                style={{
                  padding: "8px 16px",
                  backgroundColor: getColorHex("border"),
                  color: getColorHex("text"),
                  border: "none",
                  borderRadius: "4px",
                  cursor: "pointer",
                }}
              >
                Cancel
              </button>
            )}
            {editMode && (
              <button
                onClick={handleSaveEdit}
                style={{
                  padding: "8px 16px",
                  backgroundColor: getColorHex("primary"),
                  color: "white",
                  border: "none",
                  borderRadius: "4px",
                  cursor: "pointer",
                }}
              >
                Save
              </button>
            )}
            {!editMode && (
              <button
                onClick={onClose}
                style={{
                  padding: "8px 16px",
                  backgroundColor: getColorHex("border"),
                  color: getColorHex("text"),
                  border: "none",
                  borderRadius: "4px",
                  cursor: "pointer",
                }}
              >
                Close
              </button>
            )}
          </div>
        </div>
      </div>
    </div>
  );
};

export default ProfileDialog;
