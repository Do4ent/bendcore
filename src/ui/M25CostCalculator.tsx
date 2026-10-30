import React, { useState } from 'react';
export interface CostCalculatorProps { isOpen: boolean; onClose: () => void; isDarkMode?: boolean; }
const CostCalculator: React.FC<CostCalculatorProps> = ({ isOpen, onClose, isDarkMode = false }) => {
  const [materialCost, setMaterialCost] = useState(0);
  const [laborCost, setLaborCost] = useState(0);
  const bgColor = isDarkMode ? '#2a2a2a' : '#ffffff';
  const textColor = isDarkMode ? '#ffffff' : '#000000';
  if (!isOpen) return null;
  return (
    <div className="cost-calc-overlay">
      <div className="cost-calc-dialog" style={{ backgroundColor: bgColor, color: textColor }}>
        <div className="cost-header"><h2>Cost Calculator</h2><button className="close-btn" onClick={onClose}>×</button></div>
        <div className="cost-body"><div><label>Material Cost:</label><input type="number" value={materialCost} onChange={(e) => setMaterialCost(parseFloat(e.target.value))} /></div><div><label>Labor Cost:</label><input type="number" value={laborCost} onChange={(e) => setLaborCost(parseFloat(e.target.value))} /></div><div className="total-cost">Total: ${(materialCost + laborCost).toFixed(2)}</div></div>
        <div className="cost-footer"><button className="btn btn-cancel" onClick={onClose}>Close</button></div>
      </div>
    </div>
  );
};
export default CostCalculator;
