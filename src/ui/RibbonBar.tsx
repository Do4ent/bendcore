/**
 * CommandManager RibbonBar
 * Main ribbon interface for BendCore CAD with 5 context-sensitive tabs
 * Reference: BENDCORE_SPECIFICATION_FINAL_LOCKED.md Section 6
 */

import React, { useState, useCallback } from "react";
import RibbonButton, { RibbonButtonGroup } from "./RibbonButton";
import { RibbonTabBar } from "./RibbonTab";

export interface CommandHandler {
  (commandId: string, args?: Record<string, any>): Promise<void> | void;
}

export interface RibbonBarProps {
  onCommand: CommandHandler;
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

const RibbonBar: React.FC<RibbonBarProps> = ({
  onCommand,
  activeDocument,
}) => {
  const [activeTab, setActiveTab] = useState<string>("home");
  const [gapViolations, setGapViolations] = useState<GapViolation[]>([]);
  const [showGapPanel, setShowGapPanel] = useState(false);
  const [massResult, setMassResult] = useState<MassResult | null>(null);
  const [showMassPanel, setShowMassPanel] = useState(false);

  const handleCommand = useCallback(
    (commandId: string) => {
      if (commandId === "check-gaps") {
        // Simulate gap check and display violations
        setShowGapPanel(true);
        onCommand(commandId);
      } else if (commandId === "calculate-mass") {
        // Calculate mass and display results
        setShowMassPanel(true);
        onCommand(commandId);
      } else {
        onCommand(commandId);
      }
    },
    [onCommand]
  );

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
        <RibbonButton
          id="import-dwfx"
          label="DWFx"
          colorCategory="import"
          onClick={handleCommand}
          tooltip="Import DWFx file"
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
    <div className="bg-gray-50 border-b border-gray-400">
      <RibbonTabBar
        activeTab={activeTab}
        onTabChange={setActiveTab}
        tabs={tabs}
      />
    </div>
  );
};

export default RibbonBar;
