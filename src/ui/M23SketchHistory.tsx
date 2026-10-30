import React, { useState } from 'react';

export interface SketchHistoryProps {
  isOpen: boolean;
  onClose: () => void;
  isDarkMode?: boolean;
}

const SketchHistory: React.FC<SketchHistoryProps> = ({
  isOpen,
  onClose,
  isDarkMode = false
}) => {
  const [history, setHistory] = useState<Array<{ id: string; action: string; timestamp: Date }>>([]);

  const bgColor = isDarkMode ? '#2a2a2a' : '#ffffff';
  const textColor = isDarkMode ? '#ffffff' : '#000000';

  if (!isOpen) return null;

  return (
    <div className="history-overlay">
      <div className="history-dialog" style={{ backgroundColor: bgColor, color: textColor }}>
        <div className="hist-header">
          <h2>Sketch History</h2>
          <button className="close-btn" onClick={onClose}>×</button>
        </div>
        <div className="hist-body">
          <div className="history-list">
            {history.length > 0 ? (
              history.map(item => (
                <div key={item.id} className="history-item">
                  <span>{item.action}</span>
                  <span className="timestamp">{item.timestamp.toLocaleTimeString()}</span>
                </div>
              ))
            ) : (
              <div className="empty-state">No history</div>
            )}
          </div>
        </div>
        <div className="hist-footer">
          <button className="btn btn-cancel" onClick={onClose}>Close</button>
        </div>
      </div>
    </div>
  );
};

export default SketchHistory;
