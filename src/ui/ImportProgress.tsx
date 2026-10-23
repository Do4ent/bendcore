// Import Progress Component (Q3 2027, Task 1, Subtask 8)
import React from 'react';

interface ImportProgressProps {
  progress: number;
  currentStep: string;
}

const ImportProgress: React.FC<ImportProgressProps> = ({ progress, currentStep }) => {
  return (
    <div className="import-progress-container">
      <h3>Importing file</h3>
      <div className="progress-info">
        <p>{currentStep}</p>
        <div className="progress-bar">
          <div className="progress-fill" style={{ width: `${progress}%` }} />
        </div>
        <span className="progress-percent">{Math.round(progress)}%</span>
      </div>
    </div>
  );
};

export default ImportProgress;
