import { jsx as _jsx, jsxs as _jsxs } from "react/jsx-runtime";
/**
 * Tube Element Configuration Dialog
 * Complete 5-tab dialog for tube design with real-time validation and preview
 * Phase 29 Day 6: Complete implementation
 */
import React, { useState, useCallback, useMemo } from "react";
// ============ CONSTANTS ============
const MATERIAL_DENSITIES = {
    Steel: 7.85,
    Aluminum: 2.70,
    Copper: 8.96,
    Titanium: 4.54,
    Custom: 5.0,
};
const DEFAULT_CONFIG = {
    geometry: {
        outerDiameter: 16.0,
        wallThickness: 1.5,
        length: 1000,
        material: "Steel",
    },
    bends: [],
    ports: [],
    advanced: {
        springbackFactor: 0.3,
        bendCompensation: false,
        lengthTolerance: 2.0,
        angleTolerance: 0.5,
        surfaceFinish: "Raw",
    },
};
const GeometryTab = ({ config, onChange, errors, calculatedMass, }) => {
    const geometry = config.geometry;
    const handleChange = useCallback((field, value) => {
        onChange({
            ...config,
            geometry: { ...geometry, [field]: value },
        });
    }, [config, onChange, geometry]);
    const getFieldError = (field) => errors.find((e) => e.field === field)?.message || "";
    const hasFieldError = (field) => getFieldError(field) !== "";
    // Calculate cross-section area
    const crossSectionArea = useMemo(() => {
        const od = geometry.outerDiameter;
        const wt = geometry.wallThickness;
        const id = od - 2 * wt;
        return Math.PI * (od * od - id * id) / 4;
    }, [geometry.outerDiameter, geometry.wallThickness]);
    const density = MATERIAL_DENSITIES[geometry.material] || 5.0;
    return (_jsxs("div", { className: "space-y-4", children: [_jsxs("div", { className: "border-l-4 border-ochre-600 pl-3", children: [_jsx("label", { className: "block text-sm font-medium text-gray-700 mb-1", children: "Outer Diameter (mm)" }), _jsxs("div", { className: "flex items-center gap-2", children: [_jsx("input", { type: "number", min: "1", max: "100", step: "0.1", value: geometry.outerDiameter, onChange: (e) => handleChange("outerDiameter", parseFloat(e.target.value)), className: `flex-1 px-2 py-1 border rounded text-sm focus:outline-none focus:ring-2 focus:ring-blue-500 ${hasFieldError("outerDiameter")
                                    ? "border-red-500 bg-red-50"
                                    : "border-gray-300"}` }), _jsx("span", { className: "text-gray-500 text-sm", children: "mm" })] }), hasFieldError("outerDiameter") && (_jsx("p", { className: "text-red-600 text-xs mt-1", children: getFieldError("outerDiameter") }))] }), _jsxs("div", { className: "border-l-4 border-ochre-600 pl-3", children: [_jsx("label", { className: "block text-sm font-medium text-gray-700 mb-1", children: "Wall Thickness (mm)" }), _jsxs("div", { className: "flex items-center gap-2", children: [_jsx("input", { type: "number", min: "0.1", max: "10", step: "0.1", value: geometry.wallThickness, onChange: (e) => handleChange("wallThickness", parseFloat(e.target.value)), className: `flex-1 px-2 py-1 border rounded text-sm focus:outline-none focus:ring-2 focus:ring-blue-500 ${hasFieldError("wallThickness")
                                    ? "border-red-500 bg-red-50"
                                    : "border-gray-300"}` }), _jsx("span", { className: "text-gray-500 text-sm", children: "mm" })] }), hasFieldError("wallThickness") && (_jsx("p", { className: "text-red-600 text-xs mt-1", children: getFieldError("wallThickness") }))] }), _jsxs("div", { className: "border-l-4 border-ochre-600 pl-3", children: [_jsx("label", { className: "block text-sm font-medium text-gray-700 mb-1", children: "Length (mm)" }), _jsxs("div", { className: "flex items-center gap-2", children: [_jsx("input", { type: "number", min: "10", max: "5000", step: "1", value: geometry.length, onChange: (e) => handleChange("length", parseInt(e.target.value)), className: `flex-1 px-2 py-1 border rounded text-sm focus:outline-none focus:ring-2 focus:ring-blue-500 ${hasFieldError("length") ? "border-red-500 bg-red-50" : "border-gray-300"}` }), _jsx("span", { className: "text-gray-500 text-sm", children: "mm" })] }), hasFieldError("length") && (_jsx("p", { className: "text-red-600 text-xs mt-1", children: getFieldError("length") }))] }), _jsxs("div", { className: "border-l-4 border-ochre-600 pl-3", children: [_jsx("label", { className: "block text-sm font-medium text-gray-700 mb-1", children: "Material" }), _jsxs("select", { value: geometry.material, onChange: (e) => handleChange("material", e.target.value), className: "w-full px-2 py-1 border border-gray-300 rounded text-sm focus:outline-none focus:ring-2 focus:ring-blue-500", children: [_jsx("option", { value: "Steel", children: "Steel" }), _jsx("option", { value: "Aluminum", children: "Aluminum" }), _jsx("option", { value: "Copper", children: "Copper" }), _jsx("option", { value: "Titanium", children: "Titanium" }), _jsx("option", { value: "Custom", children: "Custom" })] })] }), _jsxs("div", { className: "bg-blue-50 border border-blue-200 rounded p-3 mt-6", children: [_jsx("h4", { className: "text-sm font-semibold text-gray-800 mb-3", children: "Calculated Properties" }), _jsxs("div", { className: "space-y-2 text-xs", children: [_jsxs("div", { className: "flex justify-between py-1", children: [_jsx("span", { className: "text-gray-700", children: "Cross-section Area:" }), _jsxs("span", { className: "text-gray-900 font-medium", children: [crossSectionArea.toFixed(2), " mm\u00B2"] })] }), _jsxs("div", { className: "flex justify-between py-1", children: [_jsx("span", { className: "text-gray-700", children: "Material Density:" }), _jsxs("span", { className: "text-gray-900 font-medium", children: [density.toFixed(2), " g/cm\u00B3"] })] }), _jsxs("div", { className: "flex justify-between py-1", children: [_jsx("span", { className: "text-gray-700", children: "Calculated Mass:" }), _jsxs("span", { className: "text-gray-900 font-medium", children: [calculatedMass !== null ? calculatedMass.toFixed(3) : "—", " kg"] })] })] })] })] }));
};
const BendsTab = ({ bends, tubeLength, onAdd, onEdit, onDelete, errors, maxBendRadius, }) => {
    const [editingIndex, setEditingIndex] = useState(null);
    const [editingBend, setEditingBend] = useState(null);
    const startEdit = (index) => {
        setEditingIndex(index);
        setEditingBend({ ...bends[index] });
    };
    const saveEdit = () => {
        if (editingIndex !== null && editingBend) {
            onEdit(editingIndex, editingBend);
            setEditingIndex(null);
            setEditingBend(null);
        }
    };
    const cancelEdit = () => {
        setEditingIndex(null);
        setEditingBend(null);
    };
    const totalBendAngle = bends.reduce((sum, bend) => sum + bend.angle, 0);
    return (_jsxs("div", { className: "space-y-4", children: [bends.length === 0 ? (_jsx("div", { className: "text-center py-6 text-gray-500", children: _jsx("p", { className: "text-sm", children: "No bends defined yet" }) })) : (_jsx("div", { className: "border border-gray-300 rounded overflow-hidden", children: _jsxs("table", { className: "w-full text-sm", children: [_jsx("thead", { className: "bg-gray-100 border-b border-gray-300", children: _jsxs("tr", { children: [_jsx("th", { className: "px-3 py-2 text-left text-xs font-semibold text-gray-700", children: "Position" }), _jsx("th", { className: "px-3 py-2 text-left text-xs font-semibold text-gray-700", children: "Radius" }), _jsx("th", { className: "px-3 py-2 text-left text-xs font-semibold text-gray-700", children: "Angle" }), _jsx("th", { className: "px-3 py-2 text-center text-xs font-semibold text-gray-700", children: "Actions" })] }) }), _jsx("tbody", { children: bends.map((bend, index) => (_jsxs("tr", { className: "border-t border-gray-200 hover:bg-gray-50", children: [_jsxs("td", { className: "px-3 py-2 text-gray-900", children: [bend.position.toFixed(1), " mm"] }), _jsxs("td", { className: "px-3 py-2 text-gray-900", children: [bend.radius.toFixed(1), " mm"] }), _jsxs("td", { className: "px-3 py-2 text-gray-900", children: [bend.angle.toFixed(1), "\u00B0"] }), _jsxs("td", { className: "px-3 py-2 text-center", children: [_jsx("button", { onClick: () => startEdit(index), className: "px-2 py-1 text-xs text-blue-600 hover:bg-blue-50 rounded mr-2", children: "Edit" }), _jsx("button", { onClick: () => onDelete(index), className: "px-2 py-1 text-xs text-red-600 hover:bg-red-50 rounded", children: "Delete" })] })] }, bend.id))) })] }) })), editingIndex !== null && editingBend && (_jsxs("div", { className: "bg-blue-50 border border-blue-300 rounded p-3", children: [_jsx("h4", { className: "text-sm font-semibold text-gray-800 mb-3", children: "Edit Bend" }), _jsxs("div", { className: "space-y-3", children: [_jsxs("div", { children: [_jsx("label", { className: "block text-xs font-medium text-gray-700 mb-1", children: "Position (mm)" }), _jsx("input", { type: "number", min: "0", max: tubeLength, value: editingBend.position, onChange: (e) => setEditingBend({
                                            ...editingBend,
                                            position: parseFloat(e.target.value),
                                        }), className: "w-full px-2 py-1 border border-gray-300 rounded text-sm focus:outline-none focus:ring-2 focus:ring-blue-500" })] }), _jsxs("div", { children: [_jsx("label", { className: "block text-xs font-medium text-gray-700 mb-1", children: "Radius (mm)" }), _jsx("input", { type: "number", min: "10", max: "500", value: editingBend.radius, onChange: (e) => setEditingBend({
                                            ...editingBend,
                                            radius: parseFloat(e.target.value),
                                        }), className: "w-full px-2 py-1 border border-gray-300 rounded text-sm focus:outline-none focus:ring-2 focus:ring-blue-500" })] }), _jsxs("div", { children: [_jsx("label", { className: "block text-xs font-medium text-gray-700 mb-1", children: "Angle (\u00B0)" }), _jsx("input", { type: "number", min: "1", max: "180", value: editingBend.angle, onChange: (e) => setEditingBend({
                                            ...editingBend,
                                            angle: parseFloat(e.target.value),
                                        }), className: "w-full px-2 py-1 border border-gray-300 rounded text-sm focus:outline-none focus:ring-2 focus:ring-blue-500" })] }), _jsxs("div", { className: "flex gap-2 justify-end", children: [_jsx("button", { onClick: cancelEdit, className: "px-3 py-1 text-xs border border-gray-300 rounded hover:bg-gray-100", children: "Cancel" }), _jsx("button", { onClick: saveEdit, className: "px-3 py-1 text-xs bg-blue-500 text-white rounded hover:bg-blue-600", children: "Save" })] })] })] })), _jsxs("div", { className: "bg-yellow-50 border border-yellow-200 rounded p-3", children: [_jsxs("div", { className: "flex justify-between text-sm", children: [_jsx("span", { className: "text-gray-700", children: "Total Bends:" }), _jsx("span", { className: "text-gray-900 font-medium", children: bends.length })] }), _jsxs("div", { className: "flex justify-between text-sm mt-1", children: [_jsx("span", { className: "text-gray-700", children: "Total Angle:" }), _jsxs("span", { className: "text-gray-900 font-medium", children: [totalBendAngle.toFixed(1), "\u00B0"] })] })] }), _jsx("button", { onClick: onAdd, className: "w-full px-3 py-2 bg-green-600 text-white text-sm font-medium rounded hover:bg-green-700 transition", children: "+ Add New Bend" })] }));
};
const PortsTab = ({ ports, tubeLength, tubeOD, onAdd, onEdit, onDelete, errors, }) => {
    const [editingIndex, setEditingIndex] = useState(null);
    const [editingPort, setEditingPort] = useState(null);
    const startEdit = (index) => {
        setEditingIndex(index);
        setEditingPort({ ...ports[index] });
    };
    const saveEdit = () => {
        if (editingIndex !== null && editingPort) {
            onEdit(editingIndex, editingPort);
            setEditingIndex(null);
            setEditingPort(null);
        }
    };
    const cancelEdit = () => {
        setEditingIndex(null);
        setEditingPort(null);
    };
    return (_jsxs("div", { className: "space-y-4", children: [ports.length === 0 ? (_jsx("div", { className: "text-center py-6 text-gray-500", children: _jsx("p", { className: "text-sm", children: "No ports defined yet" }) })) : (_jsx("div", { className: "border border-gray-300 rounded overflow-hidden", children: _jsxs("table", { className: "w-full text-sm", children: [_jsx("thead", { className: "bg-gray-100 border-b border-gray-300", children: _jsxs("tr", { children: [_jsx("th", { className: "px-3 py-2 text-left text-xs font-semibold text-gray-700", children: "Name" }), _jsx("th", { className: "px-3 py-2 text-left text-xs font-semibold text-gray-700", children: "Position" }), _jsx("th", { className: "px-3 py-2 text-left text-xs font-semibold text-gray-700", children: "Type" }), _jsx("th", { className: "px-3 py-2 text-left text-xs font-semibold text-gray-700", children: "Diameter" }), _jsx("th", { className: "px-3 py-2 text-center text-xs font-semibold text-gray-700", children: "Actions" })] }) }), _jsx("tbody", { children: ports.map((port, index) => (_jsxs("tr", { className: "border-t border-gray-200 hover:bg-gray-50", children: [_jsx("td", { className: "px-3 py-2 text-gray-900", children: port.name }), _jsxs("td", { className: "px-3 py-2 text-gray-900", children: [port.position.toFixed(1), " mm"] }), _jsx("td", { className: "px-3 py-2 text-gray-900", children: port.type }), _jsxs("td", { className: "px-3 py-2 text-gray-900", children: [port.diameter.toFixed(1), " mm"] }), _jsxs("td", { className: "px-3 py-2 text-center", children: [_jsx("button", { onClick: () => startEdit(index), className: "px-2 py-1 text-xs text-blue-600 hover:bg-blue-50 rounded mr-2", children: "Edit" }), _jsx("button", { onClick: () => onDelete(index), className: "px-2 py-1 text-xs text-red-600 hover:bg-red-50 rounded", children: "Delete" })] })] }, port.id))) })] }) })), editingIndex !== null && editingPort && (_jsxs("div", { className: "bg-blue-50 border border-blue-300 rounded p-3", children: [_jsx("h4", { className: "text-sm font-semibold text-gray-800 mb-3", children: "Edit Port" }), _jsxs("div", { className: "space-y-3", children: [_jsxs("div", { children: [_jsx("label", { className: "block text-xs font-medium text-gray-700 mb-1", children: "Port Name" }), _jsx("input", { type: "text", value: editingPort.name, onChange: (e) => setEditingPort({
                                            ...editingPort,
                                            name: e.target.value,
                                        }), className: "w-full px-2 py-1 border border-gray-300 rounded text-sm focus:outline-none focus:ring-2 focus:ring-blue-500" })] }), _jsxs("div", { children: [_jsx("label", { className: "block text-xs font-medium text-gray-700 mb-1", children: "Position (mm)" }), _jsx("input", { type: "number", min: "0", max: tubeLength, value: editingPort.position, onChange: (e) => setEditingPort({
                                            ...editingPort,
                                            position: parseFloat(e.target.value),
                                        }), className: "w-full px-2 py-1 border border-gray-300 rounded text-sm focus:outline-none focus:ring-2 focus:ring-blue-500" })] }), _jsxs("div", { children: [_jsx("label", { className: "block text-xs font-medium text-gray-700 mb-1", children: "Type" }), _jsxs("select", { value: editingPort.type, onChange: (e) => setEditingPort({
                                            ...editingPort,
                                            type: e.target.value,
                                        }), className: "w-full px-2 py-1 border border-gray-300 rounded text-sm focus:outline-none focus:ring-2 focus:ring-blue-500", children: [_jsx("option", { value: "Connection", children: "Connection" }), _jsx("option", { value: "Drain", children: "Drain" }), _jsx("option", { value: "Vent", children: "Vent" })] })] }), _jsxs("div", { children: [_jsx("label", { className: "block text-xs font-medium text-gray-700 mb-1", children: "Diameter (mm)" }), _jsx("input", { type: "number", min: "1", max: Math.max(tubeOD - 1, 20), value: editingPort.diameter, onChange: (e) => setEditingPort({
                                            ...editingPort,
                                            diameter: parseFloat(e.target.value),
                                        }), className: "w-full px-2 py-1 border border-gray-300 rounded text-sm focus:outline-none focus:ring-2 focus:ring-blue-500" })] }), _jsxs("div", { className: "flex gap-2 justify-end", children: [_jsx("button", { onClick: cancelEdit, className: "px-3 py-1 text-xs border border-gray-300 rounded hover:bg-gray-100", children: "Cancel" }), _jsx("button", { onClick: saveEdit, className: "px-3 py-1 text-xs bg-blue-500 text-white rounded hover:bg-blue-600", children: "Save" })] })] })] })), _jsx("div", { className: "bg-purple-50 border border-purple-200 rounded p-3", children: _jsxs("div", { className: "text-sm text-gray-700", children: [_jsx("span", { className: "font-medium", children: ports.length }), " port", ports.length !== 1 ? "s" : "", " defined"] }) }), _jsx("button", { onClick: onAdd, className: "w-full px-3 py-2 bg-green-600 text-white text-sm font-medium rounded hover:bg-green-700 transition", children: "+ Add New Port" })] }));
};
const AdvancedTab = ({ config, onChange }) => {
    const advanced = config.advanced;
    const handleChange = useCallback((field, value) => {
        onChange({
            ...config,
            advanced: { ...advanced, [field]: value },
        });
    }, [config, onChange, advanced]);
    const resetDefaults = () => {
        onChange({
            ...config,
            advanced: DEFAULT_CONFIG.advanced,
        });
    };
    return (_jsxs("div", { className: "space-y-4", children: [_jsxs("div", { className: "border-l-4 border-ochre-600 pl-3", children: [_jsx("label", { className: "block text-sm font-medium text-gray-700 mb-2", children: "Springback Factor" }), _jsxs("div", { className: "flex items-center gap-3", children: [_jsx("input", { type: "range", min: "0", max: "1", step: "0.05", value: advanced.springbackFactor, onChange: (e) => handleChange("springbackFactor", parseFloat(e.target.value)), className: "flex-1 h-2 bg-gray-300 rounded-lg appearance-none cursor-pointer" }), _jsx("span", { className: "text-gray-900 font-medium min-w-12", children: advanced.springbackFactor.toFixed(2) })] }), _jsx("p", { className: "text-xs text-gray-600 mt-1", children: "0 = No springback, 1 = Maximum springback" })] }), _jsxs("div", { className: "border-l-4 border-ochre-600 pl-3", children: [_jsxs("label", { className: "flex items-center gap-3 cursor-pointer", children: [_jsx("input", { type: "checkbox", checked: advanced.bendCompensation, onChange: (e) => handleChange("bendCompensation", e.target.checked), className: "w-4 h-4 text-blue-600 rounded focus:ring-2 focus:ring-blue-500" }), _jsx("span", { className: "text-sm font-medium text-gray-700", children: "Enable Bend Compensation" })] }), _jsx("p", { className: "text-xs text-gray-600 mt-1 ml-7", children: "Automatically adjust bend radius to compensate for springback" })] }), _jsxs("div", { className: "border-l-4 border-ochre-600 pl-3", children: [_jsx("label", { className: "block text-sm font-medium text-gray-700 mb-1", children: "Length Tolerance (\u00B1mm)" }), _jsxs("div", { className: "flex items-center gap-2", children: [_jsx("input", { type: "number", min: "0", max: "10", step: "0.1", value: advanced.lengthTolerance, onChange: (e) => handleChange("lengthTolerance", parseFloat(e.target.value)), className: "flex-1 px-2 py-1 border border-gray-300 rounded text-sm focus:outline-none focus:ring-2 focus:ring-blue-500" }), _jsx("span", { className: "text-gray-500 text-sm", children: "mm" })] })] }), _jsxs("div", { className: "border-l-4 border-ochre-600 pl-3", children: [_jsx("label", { className: "block text-sm font-medium text-gray-700 mb-1", children: "Angle Tolerance (\u00B1\u00B0)" }), _jsxs("div", { className: "flex items-center gap-2", children: [_jsx("input", { type: "number", min: "0", max: "5", step: "0.1", value: advanced.angleTolerance, onChange: (e) => handleChange("angleTolerance", parseFloat(e.target.value)), className: "flex-1 px-2 py-1 border border-gray-300 rounded text-sm focus:outline-none focus:ring-2 focus:ring-blue-500" }), _jsx("span", { className: "text-gray-500 text-sm", children: "\u00B0" })] })] }), _jsxs("div", { className: "border-l-4 border-ochre-600 pl-3", children: [_jsx("label", { className: "block text-sm font-medium text-gray-700 mb-1", children: "Surface Finish" }), _jsxs("select", { value: advanced.surfaceFinish, onChange: (e) => handleChange("surfaceFinish", e.target.value), className: "w-full px-2 py-1 border border-gray-300 rounded text-sm focus:outline-none focus:ring-2 focus:ring-blue-500", children: [_jsx("option", { value: "Raw", children: "Raw" }), _jsx("option", { value: "Polished", children: "Polished" }), _jsx("option", { value: "Powder-coated", children: "Powder-coated" })] })] }), _jsx("button", { onClick: resetDefaults, className: "w-full px-3 py-2 border border-gray-300 text-gray-700 text-sm font-medium rounded hover:bg-gray-100 transition", children: "Reset to Defaults" })] }));
};
const PreviewTab = ({ config, calculatedMass, isValid, errors, }) => {
    const geometry = config.geometry;
    const density = MATERIAL_DENSITIES[geometry.material] || 5.0;
    const totalBendAngle = config.bends.reduce((sum, bend) => sum + bend.angle, 0);
    // Determine status
    const hasErrors = errors.length > 0;
    const isReady = !hasErrors && config.bends.length > 0;
    return (_jsxs("div", { className: "space-y-4", children: [_jsxs("div", { className: "bg-white border border-gray-300 rounded p-4", children: [_jsx("h4", { className: "text-sm font-semibold text-gray-800 mb-3", children: "Tube Configuration" }), _jsxs("div", { className: "space-y-2 text-sm", children: [_jsxs("div", { className: "flex justify-between", children: [_jsx("span", { className: "text-gray-700", children: "Material:" }), _jsxs("span", { className: "text-gray-900 font-medium", children: [geometry.material, " (", density.toFixed(2), " g/cm\u00B3)"] })] }), _jsxs("div", { className: "flex justify-between", children: [_jsx("span", { className: "text-gray-700", children: "Outer Diameter:" }), _jsxs("span", { className: "text-gray-900 font-medium", children: [geometry.outerDiameter.toFixed(1), " mm"] })] }), _jsxs("div", { className: "flex justify-between", children: [_jsx("span", { className: "text-gray-700", children: "Wall Thickness:" }), _jsxs("span", { className: "text-gray-900 font-medium", children: [geometry.wallThickness.toFixed(1), " mm"] })] }), _jsxs("div", { className: "flex justify-between", children: [_jsx("span", { className: "text-gray-700", children: "Length:" }), _jsxs("span", { className: "text-gray-900 font-medium", children: [geometry.length, " mm"] })] }), _jsxs("div", { className: "flex justify-between pt-2 border-t border-gray-200", children: [_jsx("span", { className: "text-gray-700", children: "Calculated Mass:" }), _jsx("span", { className: "text-gray-900 font-medium", children: calculatedMass !== null ? `${calculatedMass.toFixed(3)} kg` : "—" })] })] })] }), _jsxs("div", { className: "bg-white border border-gray-300 rounded p-4", children: [_jsx("h4", { className: "text-sm font-semibold text-gray-800 mb-3", children: "Geometry" }), _jsxs("div", { className: "space-y-2 text-sm", children: [_jsxs("div", { className: "flex justify-between", children: [_jsx("span", { className: "text-gray-700", children: "Bends:" }), _jsxs("span", { className: "text-gray-900 font-medium", children: [config.bends.length, config.bends.length > 0 && ` (total angle: ${totalBendAngle.toFixed(0)}°)`] })] }), _jsxs("div", { className: "flex justify-between", children: [_jsx("span", { className: "text-gray-700", children: "Ports:" }), _jsxs("span", { className: "text-gray-900 font-medium", children: [config.ports.length, config.ports.length > 0 && ` (${config.ports.map((p) => p.type.toLowerCase()).join(", ")})`] })] }), _jsxs("div", { className: "flex justify-between", children: [_jsx("span", { className: "text-gray-700", children: "Springback Factor:" }), _jsx("span", { className: "text-gray-900 font-medium", children: config.advanced.springbackFactor.toFixed(2) })] }), _jsxs("div", { className: "flex justify-between", children: [_jsx("span", { className: "text-gray-700", children: "Surface Finish:" }), _jsx("span", { className: "text-gray-900 font-medium", children: config.advanced.surfaceFinish })] })] })] }), _jsx("div", { className: `border-l-4 p-3 rounded ${isValid
                    ? "bg-green-50 border-green-500"
                    : "bg-red-50 border-red-500"}`, children: _jsxs("div", { className: "flex items-start gap-2", children: [_jsx("span", { className: `text-lg font-bold ${isValid ? "text-green-600" : "text-red-600"}`, children: isValid ? "✓" : "✗" }), _jsxs("div", { children: [_jsx("p", { className: "text-sm font-semibold text-gray-800", children: isValid && config.bends.length > 0
                                        ? "Ready to manufacture"
                                        : hasErrors
                                            ? "Validation errors found"
                                            : "Configuration incomplete" }), hasErrors && (_jsx("div", { className: "mt-2 text-xs text-red-700", children: errors.map((err, idx) => (_jsxs("div", { className: "mt-1", children: ["\u2022 ", err.message] }, idx))) }))] })] }) }), config.advanced.springbackFactor > 0.7 && (_jsx("div", { className: "bg-yellow-50 border-l-4 border-yellow-500 p-3 rounded", children: _jsxs("p", { className: "text-xs font-medium text-yellow-800", children: ["Warning: High springback factor (", config.advanced.springbackFactor.toFixed(2), ") may affect accuracy"] }) }))] }));
};
// ============ MAIN TUBE DIALOG COMPONENT ============
const TubeDialog = ({ isOpen, onClose, onSave, initialConfig, }) => {
    const [activeTab, setActiveTab] = useState("geometry");
    const [config, setConfig] = useState(initialConfig || DEFAULT_CONFIG);
    const [errors, setErrors] = useState([]);
    const [calculatedMass, setCalculatedMass] = useState(null);
    // Validation logic
    const validateConfig = useCallback((cfg) => {
        const newErrors = [];
        const { geometry, bends, ports } = cfg;
        // Geometry validation
        if (geometry.outerDiameter <= 0 || geometry.outerDiameter > 100) {
            newErrors.push({
                field: "outerDiameter",
                message: "OD must be between 1 and 100 mm",
            });
        }
        if (geometry.wallThickness <= 0.1 || geometry.wallThickness >= geometry.outerDiameter / 2) {
            newErrors.push({
                field: "wallThickness",
                message: `WT must be between 0.1 and ${(geometry.outerDiameter / 2).toFixed(2)} mm`,
            });
        }
        if (geometry.length <= 0 || geometry.length > 5000) {
            newErrors.push({
                field: "length",
                message: "Length must be between 10 and 5000 mm",
            });
        }
        const odWtRatio = geometry.outerDiameter / geometry.wallThickness;
        if (odWtRatio < 2 || odWtRatio > 100) {
            newErrors.push({
                field: "wallThickness",
                message: `OD/WT ratio must be between 2 and 100 (current: ${odWtRatio.toFixed(1)})`,
            });
        }
        // Bends validation
        bends.forEach((bend, idx) => {
            if (bend.position < 0 || bend.position >= geometry.length) {
                newErrors.push({
                    field: `bend_${idx}_position`,
                    message: `Bend ${idx + 1}: position must be between 0 and ${geometry.length} mm`,
                });
            }
            if (bend.radius < geometry.outerDiameter / 2) {
                newErrors.push({
                    field: `bend_${idx}_radius`,
                    message: `Bend ${idx + 1}: radius must be at least ${(geometry.outerDiameter / 2).toFixed(1)} mm`,
                });
            }
            if (bend.angle <= 0 || bend.angle > 180) {
                newErrors.push({
                    field: `bend_${idx}_angle`,
                    message: `Bend ${idx + 1}: angle must be between 1 and 180 degrees`,
                });
            }
        });
        // Ports validation
        ports.forEach((port, idx) => {
            if (port.position < 0 || port.position >= geometry.length) {
                newErrors.push({
                    field: `port_${idx}_position`,
                    message: `Port ${idx + 1}: position must be between 0 and ${geometry.length} mm`,
                });
            }
            if (port.diameter >= geometry.outerDiameter) {
                newErrors.push({
                    field: `port_${idx}_diameter`,
                    message: `Port ${idx + 1}: diameter must be less than OD (${geometry.outerDiameter} mm)`,
                });
            }
        });
        setErrors(newErrors);
        return newErrors.length === 0;
    }, []);
    // Calculate mass when geometry changes
    const calculateMass = useCallback(() => {
        const { outerDiameter, wallThickness, length, material } = config.geometry;
        const density = MATERIAL_DENSITIES[material] || 5.0;
        // Calculate volume
        const innerDiameter = outerDiameter - 2 * wallThickness;
        const area = Math.PI * (outerDiameter * outerDiameter - innerDiameter * innerDiameter) / 4;
        const volume = length * area / 1000; // Convert to cm³
        const mass = (volume * density) / 1000; // Convert to kg
        setCalculatedMass(mass);
    }, [config.geometry]);
    // Handle bends
    const handleAddBend = () => {
        const newBend = {
            id: `bend_${Date.now()}`,
            position: config.geometry.length / 2,
            radius: Math.max(config.geometry.outerDiameter, 50),
            angle: 90,
        };
        setConfig({
            ...config,
            bends: [...config.bends, newBend],
        });
    };
    const handleEditBend = (index, bend) => {
        const newBends = [...config.bends];
        newBends[index] = bend;
        setConfig({
            ...config,
            bends: newBends,
        });
    };
    const handleDeleteBend = (index) => {
        setConfig({
            ...config,
            bends: config.bends.filter((_, i) => i !== index),
        });
    };
    // Handle ports
    const handleAddPort = () => {
        const newPort = {
            id: `port_${Date.now()}`,
            name: `Port${config.ports.length + 1}`,
            position: config.geometry.length / 2,
            type: "Connection",
            diameter: 6,
        };
        setConfig({
            ...config,
            ports: [...config.ports, newPort],
        });
    };
    const handleEditPort = (index, port) => {
        const newPorts = [...config.ports];
        newPorts[index] = port;
        setConfig({
            ...config,
            ports: newPorts,
        });
    };
    const handleDeletePort = (index) => {
        setConfig({
            ...config,
            ports: config.ports.filter((_, i) => i !== index),
        });
    };
    // Handle save
    const handleSave = () => {
        validateConfig(config);
        if (errors.length === 0) {
            onSave(config);
            onClose();
        }
    };
    // Validate and calculate mass whenever config changes
    React.useEffect(() => {
        validateConfig(config);
        calculateMass();
    }, [config, validateConfig, calculateMass]);
    if (!isOpen)
        return null;
    const isValid = errors.length === 0;
    return (_jsx("div", { className: "fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50", children: _jsxs("div", { className: "bg-white rounded-lg shadow-xl w-full max-w-2xl max-h-[90vh] flex flex-col", children: [_jsxs("div", { className: "flex justify-between items-center p-6 border-b border-gray-200", children: [_jsx("h2", { className: "text-lg font-bold text-gray-900", children: "Tube Element Configuration" }), _jsx("button", { onClick: onClose, className: "text-gray-500 hover:text-gray-700 text-2xl leading-none", children: "\u00D7" })] }), _jsx("div", { className: "flex gap-1 px-6 pt-4 border-b border-gray-200 bg-gray-50", children: [
                        { id: "geometry", label: "Geometry" },
                        { id: "bends", label: "Bends" },
                        { id: "ports", label: "Ports" },
                        { id: "advanced", label: "Advanced" },
                        { id: "preview", label: "Preview" },
                    ].map((tab) => (_jsx("button", { onClick: () => setActiveTab(tab.id), className: `px-4 py-2 text-sm font-medium rounded-t transition ${activeTab === tab.id
                            ? "bg-white text-blue-600 border-b-2 border-blue-500"
                            : "text-gray-700 hover:text-gray-900"}`, children: tab.label }, tab.id))) }), _jsxs("div", { className: "flex-1 overflow-y-auto p-6", children: [activeTab === "geometry" && (_jsx(GeometryTab, { config: config, onChange: setConfig, errors: errors, calculatedMass: calculatedMass })), activeTab === "bends" && (_jsx(BendsTab, { bends: config.bends, tubeLength: config.geometry.length, onAdd: handleAddBend, onEdit: handleEditBend, onDelete: handleDeleteBend, errors: errors })), activeTab === "ports" && (_jsx(PortsTab, { ports: config.ports, tubeLength: config.geometry.length, tubeOD: config.geometry.outerDiameter, onAdd: handleAddPort, onEdit: handleEditPort, onDelete: handleDeletePort, errors: errors })), activeTab === "advanced" && (_jsx(AdvancedTab, { config: config, onChange: setConfig })), activeTab === "preview" && (_jsx(PreviewTab, { config: config, calculatedMass: calculatedMass, isValid: isValid, errors: errors }))] }), _jsxs("div", { className: "flex justify-between items-center p-6 border-t border-gray-200 bg-gray-50", children: [_jsx("button", { onClick: onClose, className: "px-4 py-2 border border-gray-300 text-gray-700 rounded font-medium hover:bg-gray-100 transition", children: "Cancel" }), _jsxs("div", { className: "flex items-center gap-2", children: [isValid && (_jsx("span", { className: "text-green-600 font-medium flex items-center gap-1", children: "\u2713 Valid" })), _jsx("button", { onClick: handleSave, disabled: !isValid, className: `px-4 py-2 rounded font-medium transition ${isValid
                                        ? "bg-blue-600 text-white hover:bg-blue-700"
                                        : "bg-gray-300 text-gray-500 cursor-not-allowed"}`, children: "Save" })] })] })] }) }));
};
export default TubeDialog;
//# sourceMappingURL=TubeDialog.js.map