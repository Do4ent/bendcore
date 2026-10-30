import React, { useState, useCallback } from 'react';
import M23SketchUI from './M23SketchUI';
import M23ConstraintTools from './M23ConstraintTools';
import M23DimensionTools from './M23DimensionTools';
import M23GeometricPrimitives from './M23GeometricPrimitives';
import M23SketchConstraintDiagnostics from './M23SketchConstraintDiagnostics';
import M23SketchSolver from './M23SketchSolver';
import M23SketchValidation from './M23SketchValidation';
import M23SketchTransformation from './M23SketchTransformation';
import M23SketchLayerPanel from './M23SketchLayerPanel';
import M23SnapGrid from './M23SnapGrid';
import M23SketchPalette from './M23SketchPalette';
import M23DynamicInput from './M23DynamicInput';
import M23SketchHistory from './M23SketchHistory';
import M23SketchExport from './M23SketchExport';

/**
 * M23Integration Component
 * Orchestrates all 14 M23 sketch editor components
 * Manages state, dialog visibility, and callbacks
 */

export interface M23IntegrationProps {
  isDarkMode?: boolean;
  onSketchSaved?: (data: any) => void;
}

const M23Integration: React.FC<M23IntegrationProps> = ({
  isDarkMode = false,
  onSketchSaved
}) => {
  const [openDialogs, setOpenDialogs] = useState<{ [key: string]: boolean }>({});
  const [sketchData, setSketchData] = useState<any>(null);

  /**
   * Open dialog
   */
  const openDialog = useCallback((dialogName: string) => {
    setOpenDialogs(prev => ({ ...prev, [dialogName]: true }));
  }, []);

  /**
   * Close dialog
   */
  const closeDialog = useCallback((dialogName: string) => {
    setOpenDialogs(prev => ({ ...prev, [dialogName]: false }));
  }, []);

  /**
   * Handle sketch save
   */
  const handleSketchSave = useCallback((elements: any[], constraints: any[]) => {
    const data = { elements, constraints, timestamp: new Date() };
    setSketchData(data);
    if (onSketchSaved) {
      onSketchSaved(data);
    }
  }, [onSketchSaved]);

  /**
   * Expose API to window for external access
   */
  React.useEffect(() => {
    (window as any).__bendcore_m23 = {
      openDialog,
      closeDialog,
      getSketchData: () => sketchData
    };
    return () => {
      delete (window as any).__bendcore_m23;
    };
  }, [openDialog, closeDialog, sketchData]);

  return (
    <div className="m23-integration">
      {/* Main Sketch Editor */}
      <M23SketchUI
        isOpen={openDialogs['sketchUI'] || false}
        onClose={() => closeDialog('sketchUI')}
        onSave={handleSketchSave}
        isDarkMode={isDarkMode}
      />

      {/* Constraint Tools */}
      <M23ConstraintTools
        isOpen={openDialogs['constraintTools'] || false}
        onClose={() => closeDialog('constraintTools')}
        isDarkMode={isDarkMode}
      />

      {/* Dimension Tools */}
      <M23DimensionTools
        isOpen={openDialogs['dimensionTools'] || false}
        onClose={() => closeDialog('dimensionTools')}
        isDarkMode={isDarkMode}
      />

      {/* Geometric Primitives */}
      <M23GeometricPrimitives
        isOpen={openDialogs['geometricPrimitives'] || false}
        onClose={() => closeDialog('geometricPrimitives')}
        isDarkMode={isDarkMode}
      />

      {/* Constraint Diagnostics */}
      <M23SketchConstraintDiagnostics
        isOpen={openDialogs['diagnostics'] || false}
        onClose={() => closeDialog('diagnostics')}
        isDarkMode={isDarkMode}
      />

      {/* Sketch Solver */}
      <M23SketchSolver
        isOpen={openDialogs['solver'] || false}
        onClose={() => closeDialog('solver')}
        isDarkMode={isDarkMode}
      />

      {/* Sketch Validation */}
      <M23SketchValidation
        isOpen={openDialogs['validation'] || false}
        onClose={() => closeDialog('validation')}
        isDarkMode={isDarkMode}
      />

      {/* Sketch Transformation */}
      <M23SketchTransformation
        isOpen={openDialogs['transformation'] || false}
        onClose={() => closeDialog('transformation')}
        isDarkMode={isDarkMode}
      />

      {/* Sketch Layer Panel */}
      <M23SketchLayerPanel
        isOpen={openDialogs['layers'] || false}
        onClose={() => closeDialog('layers')}
        isDarkMode={isDarkMode}
      />

      {/* Snap & Grid */}
      <M23SnapGrid
        isOpen={openDialogs['snapGrid'] || false}
        onClose={() => closeDialog('snapGrid')}
        isDarkMode={isDarkMode}
      />

      {/* Sketch Palette */}
      <M23SketchPalette
        isOpen={openDialogs['palette'] || false}
        onClose={() => closeDialog('palette')}
        isDarkMode={isDarkMode}
      />

      {/* Dynamic Input */}
      <M23DynamicInput
        isOpen={openDialogs['dynamicInput'] || false}
        onClose={() => closeDialog('dynamicInput')}
        isDarkMode={isDarkMode}
      />

      {/* Sketch History */}
      <M23SketchHistory
        isOpen={openDialogs['history'] || false}
        onClose={() => closeDialog('history')}
        isDarkMode={isDarkMode}
      />

      {/* Sketch Export */}
      <M23SketchExport
        isOpen={openDialogs['export'] || false}
        onClose={() => closeDialog('export')}
        isDarkMode={isDarkMode}
      />
    </div>
  );
};

/**
 * Custom hook for using M23 dialogs
 */
export const useM23Dialogs = () => {
  const api = (window as any).__bendcore_m23;
  return {
    openDialog: api?.openDialog || (() => {}),
    closeDialog: api?.closeDialog || (() => {}),
    getSketchData: api?.getSketchData || (() => null)
  };
};

export default M23Integration;
