/**
 * CommandManager RibbonBar
 * Main ribbon interface for BendCore CAD with 5 context-sensitive tabs
 * Reference: BENDCORE_SPECIFICATION_FINAL_LOCKED.md Section 6
 */

import React, { useState, useCallback, useEffect } from "react";
import RibbonButton, { RibbonButtonGroup } from "./RibbonButton";
import { RibbonTabBar } from "./RibbonTab";
import TubeDialog, { TubeConfig } from "./TubeDialog";
import NewTubeDialog from "./NewTubeDialog";
import SearchBar from "./SearchBar";
import FeatureTree from "./FeatureTree";
import { initializeShortcutListeners, formatShortcutForDisplay } from "./KeyboardShortcuts";
import BendDataDialog from "./BendDataDialog";
import CheckPanel from "./CheckPanel";
import InstancesPanel from "./InstancesPanel";
import DrawingWorkspace from "./DrawingWorkspace";
import Viewport3D from "./Viewport3D";
import PropertiesPanel from "./components/PropertiesPanel";
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
  const [newTubeDialogOpen, setNewTubeDialogOpen] = useState(false);
  const [tubeDialogOpen, setTubeDialogOpen] = useState(false);
  const [selectedTubeConfig, setSelectedTubeConfig] = useState<TubeConfig | undefined>();
  const [showFeatureTree, setShowFeatureTree] = useState(true);

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
      } else if (commandId === "new-tube") {
        // Open new tube creation wizard (3-tab form)
        setNewTubeDialogOpen(true);
        onCommand(commandId);
      } else if (commandId === "edit-tube") {
        // Open tube editing dialog (5-tab form for existing tube)
        setSelectedTubeConfig(undefined);
        setTubeDialogOpen(true);
        onCommand(commandId);
      } else {
        onCommand(commandId);
      }
    },
    [onCommand]
  );

  // Context-sensitive ribbon: open the tab matching the active document type.
  const activeDocumentType = activeDocument?.type ?? null;
  useEffect(() => {
    if (activeDocumentType) {
      setActiveTab(activeDocumentType);
    }
  }, [activeDocumentType]);

  // Initialize keyboard shortcuts on mount
  useEffect(() => initializeShortcutListeners(handleCommand), [handleCommand]);

  const handleTubeSave = useCallback(
    async (config: TubeConfig) => {
      console.log("Saving tube config:", config);
      try {
        // Send to backend via IPC and WAIT for result
        if (window.electron?.ipc) {
          const command = selectedTubeConfig ? "edit-tube" : "new-tube";
          const result = await window.electron.ipc.invoke("bendcore:command", {
            command: command,
            args: config,
          });

          // Ответ главного процесса: { status: 'ok', data } или { status: 'error', message }.
          const res = result as { status?: string; message?: string } | null;
          if (!res || res.status !== "ok") {
            console.error("Save failed:", res?.message || "Unknown error");
            // Could show error toast here
            return;
          }

          console.log("Tube saved successfully");
        }

        // Only close dialog AFTER successful save
        setTubeDialogOpen(false);
        setSelectedTubeConfig(undefined);
      } catch (error) {
        console.error("Failed to save tube config:", error);
        // Could show error dialog here
      }
    },
    [selectedTubeConfig]
  );

  const handleTubeDialogClose = useCallback(() => {
    setTubeDialogOpen(false);
    setSelectedTubeConfig(undefined);
  }, []);

  const handleNewTubeCreate = useCallback(
    async (formData: any) => {
      console.log("Creating new tube from wizard:", formData);
      try {
        // Send to backend via IPC
        if (window.electron?.ipc) {
          const result = await window.electron.ipc.invoke("bendcore:command", {
            command: "new-tube",
            args: formData,
          });

          const res = result as { status?: string; message?: string } | null;
          if (!res || res.status !== "ok") {
            console.error("Tube creation failed:", res?.message || "Unknown error");
            return;
          }

          console.log("Tube created successfully");
        }

        setNewTubeDialogOpen(false);
      } catch (error) {
        console.error("Failed to create tube:", error);
      }
    },
    []
  );

  const handleNewTubeDialogClose = useCallback(() => {
    setNewTubeDialogOpen(false);
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
          </RibbonButtonGroup>

          <RibbonButtonGroup title="Tools">
            <RibbonButton
              id="measure"
              label="Measure"
              colorCategory="auxiliary"
              onClick={handleCommand}
              tooltip="Measurement tool"
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
            <div className="flex h-full w-full gap-4 p-4 bg-white">
              {/* 3D Viewport - Center */}
              <div className="flex-1 rounded border border-gray-300 bg-gray-900 relative overflow-hidden">
                <Viewport3D
                  meshes={[]}
                  onCommand={handleCommand}
                />
                {activeTube && (
                  <div className="absolute bottom-4 left-4 text-xs text-white bg-black/50 px-3 py-2 rounded">
                    Tube: {activeTube.name} | Segments: {Array.isArray(activeTube.path) ? activeTube.path.length : '?'}
                  </div>
                )}
                {!activeTube && (
                  <div className="absolute inset-0 flex items-center justify-center text-gray-400 pointer-events-none">
                    <div className="text-center">
                      <p className="mb-2">No tube loaded</p>
                      <p className="text-xs">Create new or import file</p>
                    </div>
                  </div>
                )}
              </div>

              {/* Properties Panel - Right */}
              <div className="w-80 border-l border-gray-300 overflow-y-auto bg-gray-50">
                {activeTube ? (
                  <div className="p-4">
                    <h3 className="font-bold mb-4 text-sm">Tube Properties</h3>
                    <div className="space-y-3 text-sm">
                      <div>
                        <label className="text-gray-600">Name</label>
                        <p className="font-medium">{activeTube.name}</p>
                      </div>
                      <div>
                        <label className="text-gray-600">Designation</label>
                        <p className="font-medium">{activeTube.designation || '—'}</p>
                      </div>
                      <div>
                        <label className="text-gray-600">Material</label>
                        <p className="font-medium">{activeTube.material || '—'}</p>
                      </div>
                      <div>
                        <label className="text-gray-600">Segments</label>
                        <p className="font-medium">{Array.isArray(activeTube.path) ? activeTube.path.length : '?'}</p>
                      </div>
                      <button
                        onClick={() => setTubeDialogOpen(true)}
                        className="w-full mt-4 px-3 py-2 bg-blue-600 text-white rounded text-xs font-medium hover:bg-blue-700"
                      >
                        Edit Properties
                      </button>
                    </div>
                  </div>
                ) : (
                  <div className="p-4 text-gray-500 text-sm">
                    <p className="mb-4 font-medium">Properties</p>
                    <p className="mb-2">• Create new tube</p>
                    <p>• Or import DWF/STEP file</p>
                  </div>
                )}
              </div>
            </div>
          )}
        </div>
      </div>

      {/* New Tube Creation Wizard (3-tab) */}
      <NewTubeDialog
        isOpen={newTubeDialogOpen}
        onClose={handleNewTubeDialogClose}
        onCreate={handleNewTubeCreate}
      />

      {/* Tube Editing Dialog (5-tab) */}
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

    </>
  );
};

export default RibbonBar;
