import React, { useState } from 'react';
export interface ReportGeneratorProps { isOpen: boolean; onClose: () => void; isDarkMode?: boolean; }
const ReportGenerator: React.FC<ReportGeneratorProps> = ({ isOpen, onClose, isDarkMode = false }) => {
  const [format, setFormat] = useState('pdf');
  const bgColor = isDarkMode ? '#2a2a2a' : '#ffffff';
  const textColor = isDarkMode ? '#ffffff' : '#000000';
  if (!isOpen) return null;
  return (
    <div className="report-overlay">
      <div className="report-dialog" style={{ backgroundColor: bgColor, color: textColor }}>
        <div className="report-header"><h2>Report Generator</h2><button className="close-btn" onClick={onClose}>×</button></div>
        <div className="report-body"><label>Format:</label><select value={format} onChange={(e) => setFormat(e.target.value)}><option>pdf</option><option>html</option></select></div>
        <div className="report-footer"><button className="btn btn-cancel" onClick={onClose}>Close</button></div>
      </div>
    </div>
  );
};
export default ReportGenerator;
