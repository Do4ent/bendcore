import { jsx as _jsx, jsxs as _jsxs, Fragment as _Fragment } from "react/jsx-runtime";
/**
 * CommandManager RibbonBar
 * Main ribbon interface for BendCore CAD with 5 context-sensitive tabs
 * Reference: BENDCORE_SPECIFICATION_FINAL_LOCKED.md Section 6
 */
import React, { useState, useCallback } from "react";
import RibbonButton, { RibbonButtonGroup } from "./RibbonButton";
import { RibbonTabBar } from "./RibbonTab";
import TubeDialog from "./TubeDialog";
const RibbonBar = ({ onCommand, activeDocument, }) => {
    const [activeTab, setActiveTab] = useState("home");
    const [gapViolations, setGapViolations] = useState([]);
    const [showGapPanel, setShowGapPanel] = useState(false);
    const [massResult, setMassResult] = useState(null);
    const [showMassPanel, setShowMassPanel] = useState(false);
    const [tubeDialogOpen, setTubeDialogOpen] = useState(false);
    const [selectedTubeConfig, setSelectedTubeConfig] = useState();
    const handleCommand = useCallback((commandId) => {
        if (commandId === "check-gaps") {
            // Simulate gap check and display violations
            setShowGapPanel(true);
            onCommand(commandId);
        }
        else if (commandId === "calculate-mass") {
            // Calculate mass and display results
            setShowMassPanel(true);
            onCommand(commandId);
        }
        else if (commandId === "new-tube") {
            // Open new tube dialog
            setSelectedTubeConfig(undefined);
            setTubeDialogOpen(true);
            onCommand(commandId);
        }
        else if (commandId === "edit-tube") {
            // Open edit tube dialog with existing config
            setTubeDialogOpen(true);
            onCommand(commandId);
        }
        else {
            onCommand(commandId);
        }
    }, [onCommand]);
    const handleTubeSave = useCallback((config) => {
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
    }, [selectedTubeConfig]);
    const handleTubeDialogClose = useCallback(() => {
        setTubeDialogOpen(false);
        setSelectedTubeConfig(undefined);
    }, []);
    // HOME TAB: File operations
    const homeTabContent = (_jsxs(_Fragment, { children: [_jsxs(RibbonButtonGroup, { title: "File", children: [_jsx(RibbonButton, { id: "new-document", label: "New", colorCategory: "create", onClick: handleCommand, tooltip: "Create new document" }), _jsx(RibbonButton, { id: "open", label: "Open", colorCategory: "import", onClick: handleCommand, tooltip: "Open existing document" }), _jsx(RibbonButton, { id: "save", label: "Save", colorCategory: "update", onClick: handleCommand, tooltip: "Save document" }), _jsx(RibbonButton, { id: "save-as", label: "Save As", colorCategory: "edit", onClick: handleCommand, tooltip: "Save document with new name" })] }), _jsx(RibbonButtonGroup, { title: "Print", children: _jsx(RibbonButton, { id: "print", label: "Print", colorCategory: "auxiliary", onClick: handleCommand, tooltip: "Print document" }) }), _jsx(RibbonButtonGroup, { title: "Options", children: _jsx(RibbonButton, { id: "options", label: "Options", colorCategory: "auxiliary", onClick: handleCommand, tooltip: "Application settings" }) })] }));
    // PART TAB: Part design and sketches
    const partTabContent = (_jsxs(_Fragment, { children: [_jsxs(RibbonButtonGroup, { title: "Sketch", children: [_jsx(RibbonButton, { id: "new-sketch", label: "New Sketch", colorCategory: "create", onClick: handleCommand, tooltip: "Create new sketch" }), _jsx(RibbonButton, { id: "edit-sketch", label: "Edit", colorCategory: "edit", onClick: handleCommand, tooltip: "Edit selected sketch" }), _jsx(RibbonButton, { id: "finish-sketch", label: "Finish", colorCategory: "update", onClick: handleCommand, tooltip: "Exit sketch mode" })] }), _jsxs(RibbonButtonGroup, { title: "Tube", children: [_jsx(RibbonButton, { id: "new-tube", label: "New Tube", colorCategory: "tube", onClick: handleCommand, tooltip: "Create new tube element" }), _jsx(RibbonButton, { id: "edit-tube", label: "Edit", colorCategory: "edit", onClick: handleCommand, tooltip: "Edit selected tube" })] }), _jsxs(RibbonButtonGroup, { title: "Auxiliary", children: [_jsx(RibbonButton, { id: "reference-plane", label: "Plane", colorCategory: "auxiliary", onClick: handleCommand, tooltip: "Create reference plane" }), _jsx(RibbonButton, { id: "reference-axis", label: "Axis", colorCategory: "auxiliary", onClick: handleCommand, tooltip: "Create reference axis" }), _jsx(RibbonButton, { id: "reference-point", label: "Point", colorCategory: "auxiliary", onClick: handleCommand, tooltip: "Create reference point" }), _jsx(RibbonButton, { id: "coordinate-system", label: "CS", colorCategory: "auxiliary", onClick: handleCommand, tooltip: "Create coordinate system" })] })] }));
    // ASSEMBLY TAB: Assembly and import
    const assemblyTabContent = (_jsxs(_Fragment, { children: [_jsx(RibbonButtonGroup, { title: "Insert", children: _jsx(RibbonButton, { id: "insert-component", label: "Component", colorCategory: "create", onClick: handleCommand, tooltip: "Insert component" }) }), _jsxs(RibbonButtonGroup, { title: "Import", children: [_jsx(RibbonButton, { id: "import-step", label: "STEP", colorCategory: "import", onClick: handleCommand, tooltip: "Import STEP file" }), _jsx(RibbonButton, { id: "import-dwfx", label: "DWFx", colorCategory: "import", onClick: handleCommand, tooltip: "Import DWFx file" })] }), _jsxs(RibbonButtonGroup, { title: "Port", children: [_jsx(RibbonButton, { id: "new-port", label: "New Port", colorCategory: "links", onClick: handleCommand, tooltip: "Create port" }), _jsx(RibbonButton, { id: "trace-tube", label: "Trace", colorCategory: "links", onClick: handleCommand, tooltip: "Trace tube to port" })] }), _jsx(RibbonButtonGroup, { title: "Support", children: _jsx(RibbonButton, { id: "add-support", label: "Support", colorCategory: "links", onClick: handleCommand, tooltip: "Add mounting support" }) }), _jsx(RibbonButtonGroup, { title: "Recognize", children: _jsx(RibbonButton, { id: "recognize-geometry", label: "Auto", colorCategory: "update", onClick: handleCommand, tooltip: "Auto-recognize geometry" }) })] }));
    // ANALYSIS TAB: Checks and analysis
    const analysisTabContent = (_jsxs("div", { className: "w-full", children: [_jsx("div", { className: "border-b border-gray-300 pb-4 mb-4", children: _jsxs("div", { className: "flex gap-2 flex-wrap", children: [_jsxs(RibbonButtonGroup, { title: "Validation", children: [_jsx(RibbonButton, { id: "check-all", label: "Check All", colorCategory: "auxiliary", onClick: handleCommand, tooltip: "Run all checks" }), _jsx(RibbonButton, { id: "check-collisions", label: "Collisions", colorCategory: "delete", onClick: handleCommand, tooltip: "Check collisions" }), _jsx(RibbonButton, { id: "check-gaps", label: "Gaps", colorCategory: "auxiliary", onClick: handleCommand, tooltip: "Check clearances" })] }), _jsxs(RibbonButtonGroup, { title: "Tools", children: [_jsx(RibbonButton, { id: "measure", label: "Measure", colorCategory: "auxiliary", onClick: handleCommand, tooltip: "Measurement tool" }), _jsx(RibbonButton, { id: "calculate-mass", label: "Mass", colorCategory: "auxiliary", onClick: handleCommand, tooltip: "Calculate mass" })] }), _jsx(RibbonButtonGroup, { title: "Report", children: _jsx(RibbonButton, { id: "generate-report", label: "Report", colorCategory: "update", onClick: handleCommand, tooltip: "Generate report" }) })] }) }), showGapPanel && (_jsxs("div", { className: "px-4 py-3 bg-white border border-gray-200 rounded mb-4", children: [_jsxs("div", { className: "flex justify-between items-center mb-3", children: [_jsx("h3", { className: "text-sm font-semibold text-gray-800", children: "Gap Violations (3 found)" }), _jsx("button", { onClick: () => setShowGapPanel(false), className: "text-gray-500 hover:text-gray-700 text-lg", children: "\u00D7" })] }), _jsxs("div", { className: "space-y-2 text-xs", children: [_jsxs("div", { className: "p-2 bg-red-50 border-l-4 border-red-500 rounded flex justify-between items-center", children: [_jsxs("div", { children: [_jsx("span", { className: "font-medium text-gray-900", children: "Between Bend 1-2:" }), _jsx("span", { className: "text-gray-700", children: " 1.5mm (min: 5.0mm)" })] }), _jsx("span", { className: "px-2 py-1 bg-red-200 text-red-800 rounded text-xs font-semibold", children: "ERROR" })] }), _jsxs("div", { className: "p-2 bg-yellow-50 border-l-4 border-yellow-500 rounded flex justify-between items-center", children: [_jsxs("div", { children: [_jsx("span", { className: "font-medium text-gray-900", children: "Between Bend 2-3:" }), _jsx("span", { className: "text-gray-700", children: " 3.2mm (min: 5.0mm)" })] }), _jsx("span", { className: "px-2 py-1 bg-yellow-200 text-yellow-800 rounded text-xs font-semibold", children: "WARNING" })] }), _jsxs("div", { className: "p-2 bg-yellow-50 border-l-4 border-yellow-500 rounded flex justify-between items-center", children: [_jsxs("div", { children: [_jsx("span", { className: "font-medium text-gray-900", children: "Between Bend 3-4:" }), _jsx("span", { className: "text-gray-700", children: " 4.8mm (min: 5.0mm)" })] }), _jsx("span", { className: "px-2 py-1 bg-yellow-200 text-yellow-800 rounded text-xs font-semibold", children: "WARNING" })] })] })] })), showMassPanel && (_jsxs("div", { className: "px-4 py-3 bg-white border border-gray-200 rounded", children: [_jsxs("div", { className: "flex justify-between items-center mb-3", children: [_jsx("h3", { className: "text-sm font-semibold text-gray-800", children: "Mass Calculation Results" }), _jsx("button", { onClick: () => setShowMassPanel(false), className: "text-gray-500 hover:text-gray-700 text-lg", children: "\u00D7" })] }), _jsxs("div", { className: "space-y-2 text-xs bg-yellow-50 border-l-4 border-yellow-500 p-3 rounded", children: [_jsxs("div", { className: "flex justify-between py-1", children: [_jsx("span", { className: "font-medium text-gray-700", children: "Total Mass:" }), _jsx("span", { className: "text-gray-900 font-semibold", children: "0.54 kg" })] }), _jsxs("div", { className: "flex justify-between py-1", children: [_jsx("span", { className: "font-medium text-gray-700", children: "Material:" }), _jsx("span", { className: "text-gray-900", children: "Steel" })] }), _jsxs("div", { className: "flex justify-between py-1", children: [_jsx("span", { className: "font-medium text-gray-700", children: "Tube OD:" }), _jsx("span", { className: "text-gray-900", children: "16.0 mm" })] }), _jsxs("div", { className: "flex justify-between py-1", children: [_jsx("span", { className: "font-medium text-gray-700", children: "Wall Thickness:" }), _jsx("span", { className: "text-gray-900", children: "1.5 mm" })] }), _jsxs("div", { className: "flex justify-between py-1", children: [_jsx("span", { className: "font-medium text-gray-700", children: "Segments:" }), _jsx("span", { className: "text-gray-900", children: "5" })] }), _jsxs("div", { className: "flex justify-between py-1", children: [_jsx("span", { className: "font-medium text-gray-700", children: "Density:" }), _jsx("span", { className: "text-gray-900", children: "7.85 g/cm\u00B3" })] })] })] }))] }));
    // DRAWING TAB: Drawing and documentation
    const drawingTabContent = (_jsxs(_Fragment, { children: [_jsx(RibbonButtonGroup, { title: "Sheet", children: _jsx(RibbonButton, { id: "new-sheet", label: "New Sheet", colorCategory: "create", onClick: handleCommand, tooltip: "Create new drawing sheet" }) }), _jsx(RibbonButtonGroup, { title: "View", children: _jsx(RibbonButton, { id: "create-view", label: "View", colorCategory: "create", onClick: handleCommand, tooltip: "Create view" }) }), _jsx(RibbonButtonGroup, { title: "Tables", children: _jsx(RibbonButton, { id: "bend-table", label: "Bends", colorCategory: "auxiliary", onClick: handleCommand, tooltip: "Insert bend table" }) }), _jsx(RibbonButtonGroup, { title: "Unfolding", children: _jsx(RibbonButton, { id: "unfold-drawing", label: "Unfold", colorCategory: "update", onClick: handleCommand, tooltip: "Create unfolding drawing" }) }), _jsxs(RibbonButtonGroup, { title: "Annotations", children: [_jsx(RibbonButton, { id: "add-dimension", label: "Dimension", colorCategory: "auxiliary", onClick: handleCommand, tooltip: "Add dimension" }), _jsx(RibbonButton, { id: "add-text", label: "Text", colorCategory: "auxiliary", onClick: handleCommand, tooltip: "Add text" })] })] }));
    const tabs = [
        { id: "home", name: "Home", content: homeTabContent },
        { id: "part", name: "Part", content: partTabContent },
        { id: "assembly", name: "Assembly", content: assemblyTabContent },
        { id: "analysis", name: "Analysis", content: analysisTabContent },
        { id: "drawing", name: "Drawing", content: drawingTabContent },
    ];
    return (_jsxs(_Fragment, { children: [_jsx("div", { className: "bg-gray-50 border-b border-gray-400", children: _jsx(RibbonTabBar, { activeTab: activeTab, onTabChange: setActiveTab, tabs: tabs }) }), _jsx(TubeDialog, { isOpen: tubeDialogOpen, onClose: handleTubeDialogClose, onSave: handleTubeSave, initialConfig: selectedTubeConfig })] }));
};
export default RibbonBar;
//# sourceMappingURL=RibbonBar.js.map