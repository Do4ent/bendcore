import React, { useState, useCallback } from 'react';
import M25ManufacturingPanel from './M25ManufacturingPanel';
import M25NCCodeViewer from './M25NCCodeViewer';
import M25ToolpathPreview from './M25ToolpathPreview';
import M25BOMGenerator from './M25BOMGenerator';
import M25ManufacturingAnalysis from './M25ManufacturingAnalysis';
import M25FEAResultsViewer from './M25FEAResultsViewer';
import M25ReportGenerator from './M25ReportGenerator';
import M25CostCalculator from './M25CostCalculator';
import M25DrawingGenerator from './M25DrawingGenerator';

/**
 * M25Integration Component
 * Orchestrates all 9 M25 manufacturing and analysis components
 */

export interface M25IntegrationProps {
  isDarkMode?: boolean;
}

const M25Integration: React.FC<M25IntegrationProps> = ({ isDarkMode = false }) => {
  const [openDialogs, setOpenDialogs] = useState<{ [key: string]: boolean }>({});

  const openDialog = useCallback((dialogName: string) => {
    setOpenDialogs(prev => ({ ...prev, [dialogName]: true }));
  }, []);

  const closeDialog = useCallback((dialogName: string) => {
    setOpenDialogs(prev => ({ ...prev, [dialogName]: false }));
  }, []);

  React.useEffect(() => {
    (window as any).__bendcore_m25 = { openDialog, closeDialog };
    return () => { delete (window as any).__bendcore_m25; };
  }, [openDialog, closeDialog]);

  return (
    <div className="m25-integration">
      <M25ManufacturingPanel isOpen={openDialogs['mfgPanel'] || false} onClose={() => closeDialog('mfgPanel')} isDarkMode={isDarkMode} />
      <M25NCCodeViewer isOpen={openDialogs['ncCode'] || false} onClose={() => closeDialog('ncCode')} isDarkMode={isDarkMode} />
      <M25ToolpathPreview isOpen={openDialogs['toolpath'] || false} onClose={() => closeDialog('toolpath')} isDarkMode={isDarkMode} />
      <M25BOMGenerator isOpen={openDialogs['bom'] || false} onClose={() => closeDialog('bom')} isDarkMode={isDarkMode} />
      <M25ManufacturingAnalysis isOpen={openDialogs['mfgAnalysis'] || false} onClose={() => closeDialog('mfgAnalysis')} isDarkMode={isDarkMode} />
      <M25FEAResultsViewer isOpen={openDialogs['feaResults'] || false} onClose={() => closeDialog('feaResults')} isDarkMode={isDarkMode} />
      <M25ReportGenerator isOpen={openDialogs['report'] || false} onClose={() => closeDialog('report')} isDarkMode={isDarkMode} />
      <M25CostCalculator isOpen={openDialogs['cost'] || false} onClose={() => closeDialog('cost')} isDarkMode={isDarkMode} />
      <M25DrawingGenerator isOpen={openDialogs['drawing'] || false} onClose={() => closeDialog('drawing')} isDarkMode={isDarkMode} />
    </div>
  );
};

export const useM25Dialogs = () => {
  const api = (window as any).__bendcore_m25;
  return { openDialog: api?.openDialog || (() => {}), closeDialog: api?.closeDialog || (() => {}) };
};

export default M25Integration;
