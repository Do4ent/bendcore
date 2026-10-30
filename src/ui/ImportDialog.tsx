/**
 * BendCore Import Dialog Component
 * Handle STEP/IGES file import with geometry recognition
 *
 * Features:
 * - Import STEP/IGES files
 * - Auto-detect tubes and grouping
 * - Apply filters (diameter, material, contact)
 * - Preview recognized geometry
 */

import React, { useState, useCallback } from "react";
import { analyzeStep, MeshData, RecognizedTube } from "./tubeService";
import "./ImportDialog.css";

/**
 * Grouping rules for import filtering
 */
export enum GroupingRule {
  RULE_1 = "diameter",      // Tubes by diameter
  RULE_2 = "material",      // Tubes by material
  RULE_3 = "bending_profile", // Tubes by bending profile
  RULE_4 = "contact",       // Contact bodies (touching tubes)
  RULE_5 = "isolated",      // Isolated tubes (no contact)
}

/**
 * Recognized tube candidate
 */
export interface TubeCandidate {
  id: string;
  name: string;
  diameter: number;
  wall: number;
  length: number;
  material?: string;
  bendingProfile?: string;
  confidence: number;  // 0-100
  bendAngles: number[];
  bendRadii: number[];
  mesh: MeshData;      // сетка распознанного тела (ядро, OCCT)
}

export interface ImportOptions {
  groupingRules: GroupingRule[];
  applyRounding: boolean;
  roundingPreferences: ImportState["roundingPreferences"];
}

/**
 * Import group
 */
interface ImportGroup {
  rule: GroupingRule;
  name: string;
  tubes: TubeCandidate[];
}

/**
 * Import dialog state
 */
interface ImportState {
  step: "idle" | "selecting" | "analyzing" | "reviewing" | "complete";
  fileName?: string;
  fileSize?: number;
  groups?: ImportGroup[];
  selectedTubes: Set<string>;
  groupingRules: GroupingRule[];
  applyRounding: boolean;
  roundingPreferences: {
    round_diameters: boolean;
    round_radii: boolean;
    round_lengths: boolean;
    round_angles: boolean;
    length_precision: number;
  };
  error?: string;
  progress?: number;
}

interface ImportDialogProps {
  onImport: (tubes: TubeCandidate[], options: ImportOptions) => void;
  onCancel?: () => void;
}

/**
 * File input component
 */
const FileSelector: React.FC<{
  onFileSelected: (file: File) => void;
}> = ({ onFileSelected }) => {
  const inputRef = React.useRef<HTMLInputElement>(null);

  const handleFileChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (file) {
      const ext = file.name.toLowerCase().split(".").pop();
      if (["step", "stp", "iges", "igs"].includes(ext || "")) {
        onFileSelected(file);
      } else {
        alert("Please select a STEP (.step, .stp) or IGES (.iges, .igs) file");
      }
    }
  };

  return (
    <div className="import-file-selector">
      <div className="file-drop-zone" onClick={() => inputRef.current?.click()}>
        <div className="drop-icon">📁</div>
        <h3>Select CAD File</h3>
        <p>Click to browse or drag & drop a STEP/IGES file</p>
      </div>
      <input
        ref={inputRef}
        type="file"
        accept=".step,.stp"
        onChange={handleFileChange}
        className="hidden-input"
      />
    </div>
  );
};

/**
 * Analysis progress component
 */
const AnalysisProgress: React.FC<{ progress: number }> = ({ progress }) => {
  const steps = [
    "Reading file...",
    "Detecting cylinders...",
    "Matching profiles...",
    "Detecting bends...",
    "Grouping tubes...",
  ];
  const currentStep = Math.min(Math.floor((progress / 100) * steps.length), steps.length - 1);

  return (
    <div className="import-analysis">
      <h3>Analyzing Import</h3>
      <div className="progress-info">
        <p>{steps[currentStep]}</p>
        <div className="progress-bar">
          <div className="progress-fill" style={{ width: `${progress}%` }} />
        </div>
        <span className="progress-text">{Math.round(progress)}%</span>
      </div>
    </div>
  );
};

/**
 * Tube reviewer component
 */
interface TubeReviewerProps {
  groups: ImportGroup[];
  selectedTubes: Set<string>;
  onSelectionChange: (tubeId: string, selected: boolean) => void;
}

const TubeReviewer: React.FC<TubeReviewerProps> = ({
  groups,
  selectedTubes,
  onSelectionChange,
}) => {
  const [expandedGroups, setExpandedGroups] = useState<Set<string>>(new Set());

  const toggleGroup = (rule: GroupingRule) => {
    const newExpanded = new Set(expandedGroups);
    if (newExpanded.has(rule)) {
      newExpanded.delete(rule);
    } else {
      newExpanded.add(rule);
    }
    setExpandedGroups(newExpanded);
  };

  return (
    <div className="import-tube-reviewer">
      <h3>Recognized Tubes</h3>
      <div className="groups-list">
        {groups.map((group) => (
          <div key={group.rule} className="group-section">
            <div
              className="group-header"
              onClick={() => toggleGroup(group.rule)}
            >
              <span className="group-icon">
                {expandedGroups.has(group.rule) ? "▼" : "▶"}
              </span>
              <span className="group-name">{group.name}</span>
              <span className="group-count">({group.tubes.length})</span>
            </div>

            {expandedGroups.has(group.rule) && (
              <div className="group-tubes">
                {group.tubes.map((tube) => (
                  <div key={tube.id} className="tube-item">
                    <input
                      type="checkbox"
                      checked={selectedTubes.has(tube.id)}
                      onChange={(e) => onSelectionChange(tube.id, e.target.checked)}
                      className="tube-checkbox"
                    />
                    <div className="tube-info">
                      <div className="tube-name">{tube.name}</div>
                      <div className="tube-details">
                        <span>OD: {tube.diameter.toFixed(1)} mm</span>
                        {tube.material && <span>Material: {tube.material}</span>}
                        {tube.bendingProfile && (
                          <span>Profile: {tube.bendingProfile}</span>
                        )}
                        <span className="confidence">
                          Confidence: {tube.confidence}%
                        </span>
                      </div>
                    </div>
                    <div className="confidence-bar">
                      <div
                        className="confidence-fill"
                        style={{
                          width: `${tube.confidence}%`,
                          background:
                            tube.confidence > 80
                              ? "#4CAF50"
                              : tube.confidence > 60
                              ? "#FF9800"
                              : "#f44336",
                        }}
                      />
                    </div>
                  </div>
                ))}
              </div>
            )}
          </div>
        ))}
      </div>
    </div>
  );
};

/**
 * Rounding preferences component
 */
interface RoundingPrefsProps {
  preferences: ImportState["roundingPreferences"];
  applyRounding: boolean;
  onChange: (newPrefs: ImportState["roundingPreferences"], apply: boolean) => void;
}

const RoundingPreferences: React.FC<RoundingPrefsProps> = ({
  preferences,
  applyRounding,
  onChange,
}) => {
  return (
    <div className="import-rounding-prefs">
      <h3>Geometry Rounding</h3>
      <div className="rounding-options">
        <label className="checkbox-option">
          <input
            type="checkbox"
            checked={applyRounding}
            onChange={(e) => onChange(preferences, e.target.checked)}
          />
          <span>Apply rounding to imported geometry</span>
        </label>

        {applyRounding && (
          <div className="rounding-detail-options">
            <label className="checkbox-option">
              <input
                type="checkbox"
                checked={preferences.round_diameters}
                onChange={(e) =>
                  onChange(
                    { ...preferences, round_diameters: e.target.checked },
                    applyRounding
                  )
                }
              />
              <span>Snap diameters to standard sizes</span>
            </label>

            <label className="checkbox-option">
              <input
                type="checkbox"
                checked={preferences.round_radii}
                onChange={(e) =>
                  onChange(
                    { ...preferences, round_radii: e.target.checked },
                    applyRounding
                  )
                }
              />
              <span>Snap radii to profile values</span>
            </label>

            <label className="checkbox-option">
              <input
                type="checkbox"
                checked={preferences.round_lengths}
                onChange={(e) =>
                  onChange(
                    { ...preferences, round_lengths: e.target.checked },
                    applyRounding
                  )
                }
              />
              <span>Snap lengths to precision</span>
            </label>

            <div className="precision-control">
              <label>Length precision (mm):</label>
              <input
                type="number"
                min="0.01"
                max="10"
                step="0.1"
                value={preferences.length_precision}
                onChange={(e) =>
                  onChange(
                    { ...preferences, length_precision: parseFloat(e.target.value) },
                    applyRounding
                  )
                }
              />
            </div>
          </div>
        )}
      </div>
    </div>
  );
};

/**
 * Main import dialog component
 */
function formatMm(v: number): string {
  return Number.isInteger(v) ? String(v) : v.toFixed(2);
}

function candidateFromRecognized(fileName: string, tube: RecognizedTube): TubeCandidate {
  const base = fileName.replace(/\.[^.]+$/, "");
  return {
    id: `import-${base}-${Date.now()}`,
    name: `${base} (D${formatMm(tube.od)} x ${formatMm(tube.wall)})`,
    diameter: tube.od,
    wall: tube.wall,
    length: tube.length,
    bendingProfile: tube.bends.length
      ? tube.bends.map((b) => `${formatMm(b.angle)}°/R${formatMm(b.radius)}`).join(", ")
      : "straight",
    confidence: Math.round(tube.confidence * 100),
    bendAngles: tube.bends.map((b) => b.angle),
    bendRadii: tube.bends.map((b) => b.radius),
    mesh: tube.mesh,
  };
}

export const ImportDialog: React.FC<ImportDialogProps> = ({
  onImport,
  onCancel,
}) => {
  const [state, setState] = useState<ImportState>({
    step: "idle",
    selectedTubes: new Set(),
    groupingRules: [
      GroupingRule.RULE_1,
      GroupingRule.RULE_2,
      GroupingRule.RULE_3,
    ],
    applyRounding: true,
    roundingPreferences: {
      round_diameters: true,
      round_radii: true,
      round_lengths: true,
      round_angles: false,
      length_precision: 0.1,
    },
  });

  // Распознавание: файл уходит в сервис ядра (POST /v1/analyze), ответ — реальные параметры тела.
  const handleFileSelected = useCallback((file: File) => {
    const lower = file.name.toLowerCase();
    if (!lower.endsWith(".step") && !lower.endsWith(".stp")) {
      setState((prev) => ({
        ...prev,
        step: "idle",
        error: "Поддерживается импорт STEP (.step, .stp). IGES ядро пока не читает.",
      }));
      return;
    }
    setState((prev) => ({
      ...prev,
      step: "analyzing",
      fileName: file.name,
      fileSize: file.size,
      progress: 10,
      error: undefined,
    }));

    analyzeStep(file)
      .then((tube: RecognizedTube) => {
        const candidate = candidateFromRecognized(file.name, tube);
        const groups: ImportGroup[] = [
          { rule: GroupingRule.RULE_1, name: `By Diameter (D${formatMm(tube.od)})`, tubes: [candidate] },
        ];
        setState((prev) => ({
          ...prev,
          step: "reviewing",
          groups,
          selectedTubes: new Set(tube.valid ? [candidate.id] : []),
          progress: 100,
          error: tube.valid ? undefined : `Низкая уверенность распознавания: ${tube.notes}`,
        }));
      })
      .catch((err: unknown) => {
        setState((prev) => ({
          ...prev,
          step: "idle",
          progress: undefined,
          error: `Импорт не выполнен: ${err instanceof Error ? err.message : String(err)}`,
        }));
      });
  }, []);

  const handleSelectionChange = useCallback((tubeId: string, selected: boolean) => {
    setState((prev) => {
      const newSelected = new Set(prev.selectedTubes);
      if (selected) {
        newSelected.add(tubeId);
      } else {
        newSelected.delete(tubeId);
      }
      return { ...prev, selectedTubes: newSelected };
    });
  }, []);

  const handleRoundingChange = useCallback(
    (newPrefs: ImportState["roundingPreferences"], apply: boolean) => {
      setState((prev) => ({
        ...prev,
        roundingPreferences: newPrefs,
        applyRounding: apply,
      }));
    },
    []
  );

  const handleImport = useCallback(() => {
    if (!state.groups) return;
    const tubesToImport = state.groups
      .flatMap((g) => g.tubes)
      .filter((t) => state.selectedTubes.has(t.id));

    onImport(tubesToImport, {
      groupingRules: state.groupingRules,
      applyRounding: state.applyRounding,
      roundingPreferences: state.roundingPreferences,
    });

    setState((prev) => ({ ...prev, step: "complete" }));
  }, [state, onImport]);

  return (
    <div className="import-dialog">
      <div className="import-dialog-header">
        <h2>Import CAD File</h2>
        <p>Import STEP/IGES files with automatic tube recognition</p>
      </div>

      <div className="import-dialog-content">
        {state.step === "idle" && (
          <FileSelector onFileSelected={handleFileSelected} />
        )}

        {state.step === "analyzing" && (
          <AnalysisProgress progress={state.progress || 0} />
        )}

        {state.step === "reviewing" && state.groups && (
          <div className="import-review-panels">
            <div className="review-left">
              <TubeReviewer
                groups={state.groups}
                selectedTubes={state.selectedTubes}
                onSelectionChange={handleSelectionChange}
              />
            </div>
            <div className="review-right">
              <RoundingPreferences
                preferences={state.roundingPreferences}
                applyRounding={state.applyRounding}
                onChange={handleRoundingChange}
              />
            </div>
          </div>
        )}

        {state.error && <div className="import-error">{state.error}</div>}
      </div>

      <div className="import-dialog-footer">
        {state.step === "reviewing" && (
          <>
            <button className="btn-import" onClick={handleImport}>
              Import {state.selectedTubes.size} Tubes
            </button>
            {onCancel && (
              <button className="btn-cancel" onClick={onCancel}>
                Cancel
              </button>
            )}
          </>
        )}

        {(state.step === "idle" || state.step === "analyzing") && onCancel && (
          <button className="btn-cancel" onClick={onCancel}>
            Cancel
          </button>
        )}

        {state.step === "complete" && (
          <div className="import-success">✓ Import complete</div>
        )}
      </div>
    </div>
  );
};

export default ImportDialog;
