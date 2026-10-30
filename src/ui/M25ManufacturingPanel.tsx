import React, { useState } from 'react';
export interface ManufacturingPanelProps { isOpen: boolean; onClose: () => void; isDarkMode?: boolean; }
const ManufacturingPanel: React.FC<ManufacturingPanelProps> = ({ isOpen, onClose, isDarkMode = false }) => {
  const [processes, setProcesses] = useState<any[]>([]);
  const bgColor = isDarkMode ? '#2a2a2a' : '#ffffff';
  const textColor = isDarkMode ? '#ffffff' : '#000000';
  if (!isOpen) return null;
  return (
    <div className="mfg-panel-overlay">
      <div className="mfg-panel-dialog" style={{ backgroundColor: bgColor, color: textColor }}>
        <div className="mfg-header"><h2>Manufacturing Panel</h2><button className="close-btn" onClick={onClose}>×</button></div>
        <div className="mfg-body"><div className="processes-list"><div className="section-title">Processes</div>{processes.length === 0 && <div className="empty-state">No processes</div>}</div></div>
        <div className="mfg-footer"><button className="btn btn-cancel" onClick={onClose}>Close</button></div>
      </div>
    </div>
  );
};
export default ManufacturingPanel;
