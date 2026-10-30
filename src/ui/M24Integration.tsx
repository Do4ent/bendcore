import React, { useState, useCallback } from 'react';
import M24AssemblyUI from './M24AssemblyUI';
import M24ComponentTree from './M24ComponentTree';
import M24MateTools from './M24MateTools';
import M24AssemblyConstraintEditor from './M24AssemblyConstraintEditor';
import M24AssemblyValidation from './M24AssemblyValidation';
import M24ExplodedView from './M24ExplodedView';
import M24BendWizard from './M24BendWizard';
import M24BendParametersEditor from './M24BendParametersEditor';
import M24BendPreview from './M24BendPreview';
import M24BendAnalysis from './M24BendAnalysis';
import M24WeldUI from './M24WeldUI';
import M24FeatureTools from './M24FeatureTools';
import M24PathDefinition from './M24PathDefinition';

/**
 * M24Integration Component
 * Orchestrates all 13 M24 assembly and bend operation components
 */

export interface M24IntegrationProps {
  isDarkMode?: boolean;
  onOperationComplete?: (data: any) => void;
}

const M24Integration: React.FC<M24IntegrationProps> = ({
  isDarkMode = false,
  onOperationComplete
}) => {
  const [openDialogs, setOpenDialogs] = useState<{ [key: string]: boolean }>({});
  const [operationData, setOperationData] = useState<any>(null);

  const openDialog = useCallback((dialogName: string) => {
    setOpenDialogs(prev => ({ ...prev, [dialogName]: true }));
  }, []);

  const closeDialog = useCallback((dialogName: string) => {
    setOpenDialogs(prev => ({ ...prev, [dialogName]: false }));
  }, []);

  React.useEffect(() => {
    (window as any).__bendcore_m24 = {
      openDialog,
      closeDialog,
      getOperationData: () => operationData
    };
    return () => {
      delete (window as any).__bendcore_m24;
    };
  }, [openDialog, closeDialog, operationData]);

  return (
    <div className="m24-integration">
      <M24AssemblyUI isOpen={openDialogs['assemblyUI'] || false} onClose={() => closeDialog('assemblyUI')} isDarkMode={isDarkMode} />
      <M24ComponentTree isOpen={openDialogs['componentTree'] || false} onClose={() => closeDialog('componentTree')} isDarkMode={isDarkMode} />
      <M24MateTools isOpen={openDialogs['mateTools'] || false} onClose={() => closeDialog('mateTools')} isDarkMode={isDarkMode} />
      <M24AssemblyConstraintEditor isOpen={openDialogs['constraintEditor'] || false} onClose={() => closeDialog('constraintEditor')} isDarkMode={isDarkMode} />
      <M24AssemblyValidation isOpen={openDialogs['validation'] || false} onClose={() => closeDialog('validation')} isDarkMode={isDarkMode} />
      <M24ExplodedView isOpen={openDialogs['explodedView'] || false} onClose={() => closeDialog('explodedView')} isDarkMode={isDarkMode} />
      <M24BendWizard isOpen={openDialogs['bendWizard'] || false} onClose={() => closeDialog('bendWizard')} isDarkMode={isDarkMode} />
      <M24BendParametersEditor isOpen={openDialogs['bendParams'] || false} onClose={() => closeDialog('bendParams')} isDarkMode={isDarkMode} />
      <M24BendPreview isOpen={openDialogs['bendPreview'] || false} onClose={() => closeDialog('bendPreview')} isDarkMode={isDarkMode} />
      <M24BendAnalysis isOpen={openDialogs['bendAnalysis'] || false} onClose={() => closeDialog('bendAnalysis')} isDarkMode={isDarkMode} />
      <M24WeldUI isOpen={openDialogs['weld'] || false} onClose={() => closeDialog('weld')} isDarkMode={isDarkMode} />
      <M24FeatureTools isOpen={openDialogs['features'] || false} onClose={() => closeDialog('features')} isDarkMode={isDarkMode} />
      <M24PathDefinition isOpen={openDialogs['path'] || false} onClose={() => closeDialog('path')} isDarkMode={isDarkMode} />
    </div>
  );
};

export const useM24Dialogs = () => {
  const api = (window as any).__bendcore_m24;
  return {
    openDialog: api?.openDialog || (() => {}),
    closeDialog: api?.closeDialog || (() => {}),
    getOperationData: api?.getOperationData || (() => null)
  };
};

export default M24Integration;
