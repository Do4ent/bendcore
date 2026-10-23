/**
 * CommandManager RibbonBar
 * Main ribbon interface for BendCore CAD with 5 context-sensitive tabs
 * Reference: BENDCORE_SPECIFICATION_FINAL_LOCKED.md Section 6
 */

import React, { useState, useCallback, useEffect } from "react";
import RibbonButton, { RibbonButtonGroup } from "./RibbonButton";
import { RibbonTabBar } from "./RibbonTab";
import TubeDialog, { TubeConfig } from "./TubeDialog";
import SearchBar from "./SearchBar";
import FeatureTree from "./FeatureTree";
import { initializeShortcutListeners, formatShortcutForDisplay } from "./KeyboardShortcuts";
import BendDataDialog from "./BendDataDialog";
import CheckPanel from "./CheckPanel";
import InstancesPanel from "./InstancesPanel";
import DrawingWorkspace from "./DrawingWorkspace";
import { t } from "./core/i18n";
import type { CheckTubeInput, Feed } from "./core/types";

/** Труба документа для панелей проверки, вывода и чертежа. */
export interface DocumentTube extends CheckTubeInput {
  name: string;
  designation?: string;
  material?: string;
  wall?: number;
  feed?: Feed;
  bothOrders?: boolean;
  /** Технологическая или геометрическая ошибка по последней проверке. */
  blocksOutput?: boolean;
}

export interface CommandHandler {
  (commandId: string, args?: Record<string, any>): Promise<void> | void;
}

export interface RibbonBarProps {
  onCommand: CommandHandler;
  /** Трубы открытого документа (сборки или детали). */
  tubes?: DocumentTube[];
  /** Активная (выбранная) труба для вывода и чертежа. */
  activeTubeId?: string;
  /** Роль пользователя для выпуска чертежей (7.7). */
  role?: string;
  user?: string;
  onNavigate?: (tubeId: string, element: number) => void;
  activeDocument?: {
    type: "part" | "assembly" | "drawing" | null;
  };
}

interface GapViolation {
  bendIndex: number;
  location: string;
  actualGap: number;
  minimumGap: number;
  severity: "ERROR" | "WARNING" | "OK";
}

interface MassResult {
  mass: number;           // kg
  unit: string;
  material: string;
  outerDiameter: number;  // mm
  wallThickness: number;  // mm
  segments: number;
  density: number;        // g/cm³
}

interface TechConstraints {
  tubeMaxOD: number;
  tubeMaxWT: number;
  tubeMaxLength: number;
  bendMaxAngle: number;
  bendMinRadius: string; // "OD/2"
  materials: Array<{ name: string; density: number }>;
  maxComplexityScore: number;
  maxBendCount: string; // "unlimited"
}

const RibbonBar: React.FC<RibbonBarProps> = ({
  onCommand,
  activeDocument,
  tubes = [],
  activeTubeId,
  role,
  user,
  onNavigate,
}) => {
  // Панель в рабочей области: проверка, экземпляры или чертёж; диалог данных гибки — поверх.
  const [workPanel, setWorkPanel] = useState<"check" | "instances" | "drawing" | null>(null);
  const [bendDataOpen, setBendDataOpen] = useState(false);
  const activeTube = tubes.find((tb) => tb.id === activeTubeId) ?? tubes[0];
  const [activeTab, setActiveTab] = useState<string>("home");
  const [gapViolations, setGapViolations] = useState<GapViolation[]>([]);
  const [showGapPanel, setShowGapPanel] = useState(false);
  const [massResult, setMassResult] = useState<MassResult | null>(null);
  const [showMassPanel, setShowMassPanel] = useState(false);
  const [tubeDialogOpen, setTubeDialogOpen] = useState(false);
  const [selectedTubeConfig, setSelectedTubeConfig] = useState<TubeConfig | undefined>();
  const [showTechSpecs, setShowTechSpecs] = useState(false);
  const [showFeatureTree, setShowFeatureTree] = useState(true);

  // Initialize keyboard shortcuts on mount
  useEffect(() => {
    const commandHandler = (commandId: string) => {
      // This will be called when keyboard shortcuts are triggered
      // We'll pass it to the handleCommand logic below
    };
    initializeShortcutListeners(commandHandler);
  }, []);

  const handleCommand = useCallback(
    (commandId: string) => {
      if (commandId === "check-all") {
        setWorkPanel("check");
        onCommand(commandId);
      } else if (commandId === "instances") {
        setWorkPanel("instances");
        onCommand(commandId);
      } else if (commandId === "drawing-workspace" || commandId === "unfold-drawing") {
        setWorkPanel("drawing");
        onCommand(commandId);
      } else if (commandId === "bend-data") {
        setBendDataOpen(true);
        onCommand(commandId);
      } else if (commandId === "check-gaps") {
        // Simulate gap check and display violations
        setShowGapPanel(true);
        onCommand(commandId);
      } else if (commandId === "calculate-mass") {
        // Calculate mass and display results
        setShowMassPanel(true);
        onCommand(commandId);
      } else if (commandId === "new-tube") {
        // Open new tube dialog
        setSelectedTubeConfig(undefined);
        setTubeDialogOpen(true);
        onCommand(commandId);
      } else if (commandId === "edit-tube") {
        // Open edit tube dialog with existing config
        setTubeDialogOpen(true);
        onCommand(commandId);
      } else {
        onCommand(commandId);
      }
    },
    [onCommand]
  );

  const handleTubeSave = useCallback(
    (config: TubeConfig) => {
      console.log("Saving tube config:", config);
      // Send to backend via IPC
      if (window.electron?.ipc) {
        const command = selectedTubeConfig ? "edit-tube" : "new-tube";
        window.electron.ipc.invoke("bendcore:command", {
          command: command,
          args: config,
        });
      }
      setTubeDialogOpen(false);
    },
    [selectedTubeConfig]
  );

  const handleTubeDialogClose = useCallback(() => {
    setTubeDialogOpen(false);
    setSelectedTubeConfig(undefined);
  }, []);

  // HOME TAB: File operations
  const homeTabContent = (
    <>
      <RibbonButtonGroup title="File">
        <RibbonButton
          id="new-document"
          label="New"
          colorCategory="create"
          onClick={handleCommand}
          tooltip="Create new document"
        />
        <RibbonButton
          id="open"
          label="Open"
          colorCategory="import"
          onClick={handleCommand}
          tooltip="Open existing document"
        />
        <RibbonButton
          id="save"
          label="Save"
          colorCategory="update"
          onClick={handleCommand}
          tooltip="Save document"
        />
        <RibbonButton
          id="save-as"
          label="Save As"
          colorCategory="edit"
          onClick={handleCommand}
          tooltip="Save document with new name"
        />
      </RibbonButtonGroup>

      <RibbonButtonGroup title="Print">
        <RibbonButton
          id="print"
          label="Print"
          colorCategory="auxiliary"
          onClick={handleCommand}
          tooltip="Print document"
        />
      </RibbonButtonGroup>

      <RibbonButtonGroup title="Options">
        <RibbonButton
          id="options"
          label="Options"
          colorCategory="auxiliary"
          onClick={handleCommand}
          tooltip="Application settings"
        />
        <RibbonButton
          id="tech-specs"
          label="Tech Specs"
          colorCategory="auxiliary"
          onClick={() => setShowTechSpecs(true)}
          tooltip="View technical constraints and specifications"
        />
      </RibbonButtonGroup>
    </>
  );

  // PART TAB: Part design and sketches
  const partTabContent = (
    <>
      <RibbonButtonGroup title="Sketch">
        <RibbonButton
          id="new-sketch"
          label="New Sketch"
          colorCategory="create"
          onClick={handleCommand}
          tooltip="Create new sketch"
        />
        <RibbonButton
          id="edit-sketch"
          label="Edit"
          colorCategory="edit"
          onClick={handleCommand}
          tooltip="Edit selected sketch"
        />
        <RibbonButton
          id="finish-sketch"
          label="Finish"
          colorCategory="update"
          onClick={handleCommand}
          tooltip="Exit sketch mode"
        />
      </RibbonButtonGroup>

      <RibbonButtonGroup title="Tube">
        <RibbonButton
          id="new-tube"
          label="New Tube"
          colorCategory="tube"
          onClick={handleCommand}
          tooltip="Create new tube element"
        />
        <RibbonButton
          id="edit-tube"
          label="Edit"
          colorCategory="edit"
          onClick={handleCommand}
          tooltip="Edit selected tube"
        />
      </RibbonButtonGroup>

      <RibbonButtonGroup title="Auxiliary">
        <RibbonButton
          id="reference-plane"
          label="Plane"
          colorCategory="auxiliary"
          onClick={handleCommand}
          tooltip="Create reference plane"
        />
        <RibbonButton
          id="reference-axis"
          label="Axis"
          colorCategory="auxiliary"
          onClick={handleCommand}
          tooltip="Create reference axis"
        />
        <RibbonButton
          id="reference-point"
          label="Point"
          colorCategory="auxiliary"
          onClick={handleCommand}
          tooltip="Create reference point"
        />
        <RibbonButton
          id="coordinate-system"
          label="CS"
          colorCategory="auxiliary"
          onClick={handleCommand}
          tooltip="Create coordinate system"
        />
      </RibbonButtonGroup>
    </>
  );

  // ASSEMBLY TAB: Assembly and import
  const assemblyTabContent = (
    <>
      <RibbonButtonGroup title="Insert">
        <RibbonButton
          id="insert-component"
          label="Component"
          colorCategory="create"
          onClick={handleCommand}
          tooltip="Insert component"
        />
      </RibbonButtonGroup>

      <RibbonButtonGroup title="Import">
        <RibbonButton
          id="import-step"
          label="STEP"
          colorCategory="import"
          onClick={handleCommand}
          tooltip="Import STEP file"
        />
        <RibbonButton
          id="import-dwfx"
          label="DWFx"
          colorCategory="import"
          onClick={handleCommand}
          tooltip="Import DWFx file"
        />
      </RibbonButtonGroup>

      <RibbonButtonGroup title={t("inst.title")}>
        <RibbonButton
          id="instances"
          label={t("inst.title")}
          colorCategory="links"
          onClick={handleCommand}
          tooltip={`${t("inst.makePrimary")} · ${t("inst.detach")}`}
        />
      </RibbonButtonGroup>

      <RibbonButtonGroup title="Port">
        <RibbonButton
          id="new-port"
          label="New Port"
          colorCategory="links"
          onClick={handleCommand}
          tooltip="Create port"
        />
        <RibbonButton
          id="trace-tube"
          label="Trace"
          colorCategory="links"
          onClick={handleCommand}
          tooltip="Trace tube to port"
        />
      </RibbonButtonGroup>

      <RibbonButtonGroup title="Support">
        <RibbonButton
          id="add-support"
          label="Support"
          colorCategory="links"
          onClick={handleCommand}
          tooltip="Add mounting support"
        />
      </RibbonButtonGroup>

      <RibbonButtonGroup title="Recognize">
        <RibbonButton
          id="recognize-geometry"
          label="Auto"
          colorCategory="update"
          onClick={handleCommand}
          tooltip="Auto-recognize geometry"
        />
      </RibbonButtonGroup>
    </>
  );

  // ANALYSIS TAB: Checks and analysis
  const analysisTabContent = (
    <div className="w-full">
      <div className="border-b border-gray-300 pb-4 mb-4">
        <div className="flex gap-2 flex-wrap">
          <RibbonButtonGroup title="Validation">
            <RibbonButton
              id="check-all"
              label="Check All"
              colorCategory="auxiliary"
              onClick={handleCommand}
              tooltip="Run all checks"
            />
            <RibbonButton
              id="check-collisions"
              label="Collisions"
              colorCategory="delete"
              onClick={handleCommand}
              tooltip="Check collisions"
            />
            <RibbonButton
              id="check-gaps"
              label="Gaps"
              colorCategory="auxiliary"
              onClick={handleCommand}
              tooltip="Check clearances"
            />
          </RibbonButtonGroup>

          <RibbonButtonGroup title="Tools">
            <RibbonButton
              id="measure"
              label="Measure"
              colorCategory="auxiliary"
              onClick={handleCommand}
              tooltip="Measurement tool"
            />
            <RibbonButton
              id="calculate-mass"
              label="Mass"
              colorCategory="auxiliary"
              onClick={handleCommand}
              tooltip="Calculate mass"
            />
          </RibbonButtonGroup>

          <RibbonButtonGroup title="Report">
            <RibbonButton
              id="generate-report"
              label="Report"
              colorCategory="update"
              onClick={handleCommand}
              tooltip="Generate report"
            />
          </RibbonButtonGroup>
        </div>
      </div>

      {/* Gap Violations Panel */}
      {showGapPanel && (
        <div className="px-4 py-3 bg-white border border-gray-200 rounded mb-4">
          <div className="flex justify-between items-center mb-3">
            <h3 className="text-sm font-semibold text-gray-800">
              Gap Violations (3 found)
            </h3>
            <button
              onClick={() => setShowGapPanel(false)}
              className="text-gray-500 hover:text-gray-700 text-lg"
            >
              ×
            </button>
          </div>

          <div className="space-y-2 text-xs">
            {/* Sample Gap Violation 1 - ERROR */}
            <div className="p-2 bg-red-50 border-l-4 border-red-500 rounded flex justify-between items-center">
              <div>
                <span className="font-medium text-gray-900">Between Bend 1-2:</span>
                <span className="text-gray-700"> 1.5mm (min: 5.0mm)</span>
              </div>
              <span className="px-2 py-1 bg-red-200 text-red-800 rounded text-xs font-semibold">
                ERROR
              </span>
            </div>

            {/* Sample Gap Violation 2 - WARNING */}
            <div className="p-2 bg-yellow-50 border-l-4 border-yellow-500 rounded flex justify-between items-center">
              <div>
                <span className="font-medium text-gray-900">Between Bend 2-3:</span>
                <span className="text-gray-700"> 3.2mm (min: 5.0mm)</span>
              </div>
              <span className="px-2 py-1 bg-yellow-200 text-yellow-800 rounded text-xs font-semibold">
                WARNING
              </span>
            </div>

            {/* Sample Gap Violation 3 - WARNING */}
            <div className="p-2 bg-yellow-50 border-l-4 border-yellow-500 rounded flex justify-between items-center">
              <div>
                <span className="font-medium text-gray-900">Between Bend 3-4:</span>
                <span className="text-gray-700"> 4.8mm (min: 5.0mm)</span>
              </div>
              <span className="px-2 py-1 bg-yellow-200 text-yellow-800 rounded text-xs font-semibold">
                WARNING
              </span>
            </div>
          </div>
        </div>
      )}

      {/* Mass Calculation Panel */}
      {showMassPanel && (
        <div className="px-4 py-3 bg-white border border-gray-200 rounded">
          <div className="flex justify-between items-center mb-3">
            <h3 className="text-sm font-semibold text-gray-800">
              Mass Calculation Results
            </h3>
            <button
              onClick={() => setShowMassPanel(false)}
              className="text-gray-500 hover:text-gray-700 text-lg"
            >
              ×
            </button>
          </div>

          <div className="space-y-2 text-xs bg-yellow-50 border-l-4 border-yellow-500 p-3 rounded">
            {/* Sample mass result: OD=16mm, WT=1.5mm, Material=Steel */}
            <div className="flex justify-between py-1">
              <span className="font-medium text-gray-700">Total Mass:</span>
              <span className="text-gray-900 font-semibold">0.54 kg</span>
            </div>
            <div className="flex justify-between py-1">
              <span className="font-medium text-gray-700">Material:</span>
              <span className="text-gray-900">Steel</span>
            </div>
            <div className="flex justify-between py-1">
              <span className="font-medium text-gray-700">Tube OD:</span>
              <span className="text-gray-900">16.0 mm</span>
            </div>
            <div className="flex justify-between py-1">
              <span className="font-medium text-gray-700">Wall Thickness:</span>
              <span className="text-gray-900">1.5 mm</span>
            </div>
            <div className="flex justify-between py-1">
              <span className="font-medium text-gray-700">Segments:</span>
              <span className="text-gray-900">5</span>
            </div>
            <div className="flex justify-between py-1">
              <span className="font-medium text-gray-700">Density:</span>
              <span className="text-gray-900">7.85 g/cm³</span>
            </div>
          </div>
        </div>
      )}
    </div>
  );

  // DRAWING TAB: Drawing and documentation
  const drawingTabContent = (
    <>
      <RibbonButtonGroup title={t("draw.title")}>
        <RibbonButton
          id="drawing-workspace"
          label={t("draw.title")}
          colorCategory="create"
          onClick={handleCommand}
          tooltip={t("draw.title")}
        />
        <RibbonButton
          id="bend-data"
          label="XYZ / LRA"
          colorCategory="import"
          onClick={handleCommand}
          tooltip={t("bend.title")}
        />
      </RibbonButtonGroup>

      <RibbonButtonGroup title="Sheet">
        <RibbonButton
          id="new-sheet"
          label="New Sheet"
          colorCategory="create"
          onClick={handleCommand}
          tooltip="Create new drawing sheet"
        />
      </RibbonButtonGroup>

      <RibbonButtonGroup title="View">
        <RibbonButton
          id="create-view"
          label="View"
          colorCategory="create"
          onClick={handleCommand}
          tooltip="Create view"
        />
      </RibbonButtonGroup>

      <RibbonButtonGroup title="Tables">
        <RibbonButton
          id="bend-table"
          label="Bends"
          colorCategory="auxiliary"
          onClick={handleCommand}
          tooltip="Insert bend table"
        />
      </RibbonButtonGroup>

      <RibbonButtonGroup title="Unfolding">
        <RibbonButton
          id="unfold-drawing"
          label="Unfold"
          colorCategory="update"
          onClick={handleCommand}
          tooltip="Create unfolding drawing"
        />
      </RibbonButtonGroup>

      <RibbonButtonGroup title="Annotations">
        <RibbonButton
          id="add-dimension"
          label="Dimension"
          colorCategory="auxiliary"
          onClick={handleCommand}
          tooltip="Add dimension"
        />
        <RibbonButton
          id="add-text"
          label="Text"
          colorCategory="auxiliary"
          onClick={handleCommand}
          tooltip="Add text"
        />
      </RibbonButtonGroup>
    </>
  );

  const tabs = [
    { id: "home", name: "Home", content: homeTabContent },
    { id: "part", name: "Part", content: partTabContent },
    { id: "assembly", name: "Assembly", content: assemblyTabContent },
    { id: "analysis", name: "Analysis", content: analysisTabContent },
    { id: "drawing", name: "Drawing", content: drawingTabContent },
  ];

  // Tech Constraints
  const techConstraints: TechConstraints = {
    tubeMaxOD: 100,
    tubeMaxWT: 10,
    tubeMaxLength: 5000,
    bendMaxAngle: 180,
    bendMinRadius: "OD/2",
    materials: [
      { name: "Steel", density: 7.85 },
      { name: "Aluminum", density: 2.70 },
      { name: "Copper", density: 8.96 },
      { name: "Titanium", density: 4.54 },
      { name: "Custom", density: 0 },
    ],
    maxComplexityScore: 100,
    maxBendCount: "unlimited",
  };

  const TechSpecsModal = () => (
    <div className="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50">
      <div className="bg-white rounded-lg shadow-lg max-w-2xl w-full mx-4 max-h-96 overflow-y-auto">
        <div className="p-6">
          <h2 className="text-2xl font-bold mb-6 text-gray-900">
            Technical Constraints & Specifications
          </h2>

          {/* Tube Specifications */}
          <div className="mb-8">
            <h3 className="text-lg font-semibold mb-4 text-gray-800 border-b pb-2">
              Tube Specifications
            </h3>
            <div className="grid grid-cols-2 gap-4">
              <div>
                <p className="text-gray-600 text-sm">Max Outer Diameter</p>
                <p className="text-xl font-semibold text-gray-900">
                  {techConstraints.tubeMaxOD} mm
                </p>
              </div>
              <div>
                <p className="text-gray-600 text-sm">Max Wall Thickness</p>
                <p className="text-xl font-semibold text-gray-900">
                  {techConstraints.tubeMaxWT} mm
                </p>
              </div>
              <div>
                <p className="text-gray-600 text-sm">Max Length</p>
                <p className="text-xl font-semibold text-gray-900">
                  {techConstraints.tubeMaxLength} mm
                </p>
              </div>
              <div>
                <p className="text-gray-600 text-sm">Min Bend Radius</p>
                <p className="text-xl font-semibold text-gray-900">
                  {techConstraints.bendMinRadius}
                </p>
              </div>
            </div>
          </div>

          {/* Bend Specifications */}
          <div className="mb-8">
            <h3 className="text-lg font-semibold mb-4 text-gray-800 border-b pb-2">
              Bend Specifications
            </h3>
            <div className="grid grid-cols-2 gap-4">
              <div>
                <p className="text-gray-600 text-sm">Max Bend Angle</p>
                <p className="text-xl font-semibold text-gray-900">
                  {techConstraints.bendMaxAngle}°
                </p>
              </div>
              <div>
                <p className="text-gray-600 text-sm">Max Bends</p>
                <p className="text-xl font-semibold text-gray-900">
                  {techConstraints.maxBendCount}
                </p>
              </div>
            </div>
          </div>

          {/* Material Support */}
          <div className="mb-8">
            <h3 className="text-lg font-semibold mb-4 text-gray-800 border-b pb-2">
              Material Support
            </h3>
            <div className="space-y-2">
              {techConstraints.materials.map((material, idx) => (
                <div key={idx} className="flex justify-between items-center py-2">
                  <span className="text-gray-700">✓ {material.name}</span>
                  {material.density > 0 && (
                    <span className="text-gray-500 text-sm">
                      {material.density} g/cm³
                    </span>
                  )}
                </div>
              ))}
            </div>
          </div>

          {/* Complexity Limits */}
          <div className="mb-8">
            <h3 className="text-lg font-semibold mb-4 text-gray-800 border-b pb-2">
              Complexity Limits
            </h3>
            <div>
              <p className="text-gray-600 text-sm">Max Complexity Score</p>
              <p className="text-xl font-semibold text-gray-900">
                {techConstraints.maxComplexityScore} points
              </p>
            </div>
          </div>

          {/* Close Button */}
          <div className="flex justify-end">
            <button
              onClick={() => setShowTechSpecs(false)}
              className="px-6 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700 transition-colors"
            >
              Close
            </button>
          </div>
        </div>
      </div>
    </div>
  );

  return (
    <>
      {/* Header with Search Bar */}
      <div className="bg-gray-50 border-b border-gray-400">
        <div className="flex items-center gap-4 px-4 py-2 border-b border-gray-300">
          <div className="flex-1 max-w-md">
            <SearchBar onCommand={handleCommand} />
          </div>
          <div className="text-xs text-gray-500">Ctrl+K to search</div>
        </div>

        {/* Ribbon Tabs */}
        <RibbonTabBar
          activeTab={activeTab}
          onTabChange={setActiveTab}
          tabs={tabs}
        />
      </div>

      {/* Main Layout: Feature Tree + Content */}
      <div className="flex flex-1">
        {/* Left Sidebar: Feature Tree */}
        {showFeatureTree && (
          <div className="w-64 border-r border-gray-400 bg-gray-50">
            <FeatureTree onCommand={handleCommand} onSelectFeature={(id) => {}} />
          </div>
        )}

        {/* Рабочая область: панель проверки, экземпляров или чертёж; иначе — место 3D-вида */}
        <div className="flex-1 overflow-auto" style={{ minHeight: 0 }}>
          {workPanel === "check" && (
            <CheckPanel tubes={tubes} onNavigate={(id, el) => onNavigate?.(id, el)} />
          )}
          {workPanel === "instances" && <InstancesPanel />}
          {workPanel === "drawing" && activeTube && (
            <DrawingWorkspace
              role={role}
              user={user}
              tube={{
                path: activeTube.path,
                od: activeTube.od,
                wall: activeTube.wall,
                hasErrors: activeTube.blocksOutput,
                title: {
                  "Обозначение": activeTube.designation ?? activeTube.name,
                  "Наименование": activeTube.name,
                  "Материал": activeTube.material ?? "",
                },
                unfold: {
                  E_A: activeTube.E_A ?? 0,
                  E_B: activeTube.E_B ?? 0,
                  N_first: activeTube.N_first ?? 0,
                  N_last: activeTube.N_last ?? 0,
                  cut_allowed: activeTube.cut_allowed ?? true,
                },
              }}
            />
          )}
          {workPanel === null && (
            <div className="p-4 text-gray-500">3D Viewport and Properties (to be integrated)</div>
          )}
        </div>
      </div>

      {/* Tube Dialog */}
      <TubeDialog
        isOpen={tubeDialogOpen}
        onClose={handleTubeDialogClose}
        onSave={handleTubeSave}
        initialConfig={selectedTubeConfig}
      />

      {activeTube && (
        <BendDataDialog
          open={bendDataOpen}
          onClose={() => setBendDataOpen(false)}
          tube={{
            name: activeTube.designation ?? activeTube.name,
            path: activeTube.path,
            E_A: activeTube.E_A,
            E_B: activeTube.E_B,
            feed: activeTube.feed,
            bothOrders: activeTube.bothOrders,
            blocksOutput: activeTube.blocksOutput,
          }}
        />
      )}

      {/* Tech Specs Modal */}
      {showTechSpecs && <TechSpecsModal />}
    </>
  );
};

export default RibbonBar;
