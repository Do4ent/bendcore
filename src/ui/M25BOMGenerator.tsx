import React, { useState } from 'react';
export interface BOMGeneratorProps { isOpen: boolean; onClose: () => void; isDarkMode?: boolean; }
const BOMGenerator: React.FC<BOMGeneratorProps> = ({ isOpen, onClose, isDarkMode = false }) => {
  const [bomItems, setBomItems] = useState<any[]>([]);
  const bgColor = isDarkMode ? '#2a2a2a' : '#ffffff';
  const textColor = isDarkMode ? '#ffffff' : '#000000';
  if (!isOpen) return null;
  return (
    <div className="bom-overlay">
      <div className="bom-dialog" style={{ backgroundColor: bgColor, color: textColor }}>
        <div className="bom-header"><h2>BOM Generator</h2><button className="close-btn" onClick={onClose}>×</button></div>
        <div className="bom-body"><div className="bom-items"><div className="section-title">Items ({bomItems.length})</div>{bomItems.length === 0 && <div className="empty-state">No items</div>}</div></div>
        <div className="bom-footer"><button className="btn btn-cancel" onClick={onClose}>Close</button></div>
      </div>
    </div>
  );
};
export default BOMGenerator;
