/**
 * ImportWithViewport: Import dialog + 3D viewport
 * Allows importing STEP/IGES files and viewing results in real-time 3D
 */

import React, { useState } from 'react';
import { ImportDialog } from './ImportDialog';
import { Viewport3D } from '../Viewport3D';
import type { ViewportMesh } from '../Viewport3D';

export interface ImportWithViewportProps {
  width?: number;
  height?: number;
}

export const ImportWithViewport: React.FC<ImportWithViewportProps> = ({
  width = 1400,
  height = 900,
}) => {
  const [isImportOpen, setIsImportOpen] = useState(false);
  const [meshes, setMeshes] = useState<ViewportMesh[]>([]);

  const handleMeshLoaded = (mesh: ViewportMesh) => {
    // Replace previous mesh with new one
    setMeshes([mesh]);
  };

  const handleImportClose = () => {
    setIsImportOpen(false);
  };

  const handleImportSuccess = () => {
    // Keep dialog open to allow user to see results
  };

  return (
    <div style={{ display: 'flex', flexDirection: 'column', width: '100%', height: '100vh' }}>
      {/* Toolbar */}
      <div
        style={{
          padding: '12px 16px',
          backgroundColor: '#f5f5f5',
          borderBottom: '1px solid #e0e0e0',
          display: 'flex',
          gap: '8px',
          alignItems: 'center'
        }}
      >
        <button
          onClick={() => setIsImportOpen(true)}
          style={{
            padding: '8px 16px',
            backgroundColor: '#2196F3',
            color: 'white',
            border: 'none',
            borderRadius: '4px',
            cursor: 'pointer',
            fontSize: '14px',
            fontWeight: 500
          }}
        >
          📦 Import STEP/IGES
        </button>
        {meshes.length > 0 && (
          <span style={{ fontSize: '13px', color: '#666' }}>
            ✓ {meshes.length} mesh{meshes.length !== 1 ? 'es' : ''} loaded
          </span>
        )}
      </div>

      {/* Viewport */}
      <div style={{ flex: 1, overflow: 'hidden' }}>
        <Viewport3D meshes={meshes} width={width} height={height - 60} />
      </div>

      {/* Import Dialog */}
      <ImportDialog
        isOpen={isImportOpen}
        onClose={handleImportClose}
        onSuccess={handleImportSuccess}
        onMeshLoaded={handleMeshLoaded}
      />
    </div>
  );
};

export default ImportWithViewport;
