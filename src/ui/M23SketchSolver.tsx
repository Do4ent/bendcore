import React, { useState } from 'react';

export interface SketchSolverProps {
  isOpen: boolean;
  onClose: () => void;
  isDarkMode?: boolean;
}

const SketchSolver: React.FC<SketchSolverProps> = ({
  isOpen,
  onClose,
  isDarkMode = false
}) => {
  const [solverType, setSolverType] = useState('default');
  const [status, setStatus] = useState('idle');

  const bgColor = isDarkMode ? '#2a2a2a' : '#ffffff';
  const textColor = isDarkMode ? '#ffffff' : '#000000';

  const solvers = [
    { id: 'default', name: 'Default Solver' },
    { id: 'fast', name: 'Fast Solver' },
    { id: 'precise', name: 'Precise Solver' }
  ];

  if (!isOpen) return null;

  return (
    <div className="solver-overlay">
      <div className="solver-dialog" style={{ backgroundColor: bgColor, color: textColor }}>
        <div className="solver-header">
          <h2>Sketch Solver</h2>
          <button className="close-btn" onClick={onClose}>×</button>
        </div>
        <div className="solver-body">
          <div className="solver-selection">
            {solvers.map(s => (
              <button
                key={s.id}
                className={`solver-btn ${solverType === s.id ? 'active' : ''}`}
                onClick={() => setSolverType(s.id)}
              >
                {s.name}
              </button>
            ))}
          </div>
          <div className="solver-status">
            <span>Status: {status}</span>
          </div>
        </div>
        <div className="solver-footer">
          <button className="btn btn-cancel" onClick={onClose}>Close</button>
        </div>
      </div>
    </div>
  );
};

export default SketchSolver;
