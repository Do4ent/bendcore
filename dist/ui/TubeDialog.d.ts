/**
 * Tube Element Configuration Dialog
 * Complete 5-tab dialog for tube design with real-time validation and preview
 * Phase 29 Day 6: Complete implementation
 */
import React from "react";
interface GeometryParams {
    outerDiameter: number;
    wallThickness: number;
    length: number;
    material: "Steel" | "Aluminum" | "Copper" | "Titanium" | "Custom";
}
interface BendParams {
    id: string;
    position: number;
    radius: number;
    angle: number;
}
interface PortParams {
    id: string;
    name: string;
    position: number;
    type: "Connection" | "Drain" | "Vent";
    diameter: number;
}
interface AdvancedParams {
    springbackFactor: number;
    bendCompensation: boolean;
    lengthTolerance: number;
    angleTolerance: number;
    surfaceFinish: "Raw" | "Polished" | "Powder-coated";
}
export interface TubeConfig {
    geometry: GeometryParams;
    bends: BendParams[];
    ports: PortParams[];
    advanced: AdvancedParams;
}
interface TubeDialogProps {
    isOpen: boolean;
    onClose: () => void;
    onSave: (config: TubeConfig) => void;
    initialConfig?: TubeConfig;
}
declare const TubeDialog: React.FC<TubeDialogProps>;
export default TubeDialog;
//# sourceMappingURL=TubeDialog.d.ts.map