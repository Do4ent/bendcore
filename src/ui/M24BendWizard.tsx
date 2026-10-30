import React, { useState } from 'react';

/**
 * BendWizard Component - GAP 0111
 * Interactive bend operation wizard with multi-step form
 */

export interface BendWizardProps {
  isOpen: boolean;
  onClose: () => void;
  onComplete?: (params: any) => void;
  isDarkMode?: boolean;
}

const BendWizard: React.FC<BendWizardProps> = ({
  isOpen,
  onClose,
  onComplete,
  isDarkMode = false
}) => {
  const [step, setStep] = useState(1);
  const [bendParams, setBendParams] = useState({
    radius: 10,
    angle: 90,
    material: 'steel',
    thickness: 2
  });

  const bgColor = isDarkMode ? '#2a2a2a' : '#ffffff';
  const textColor = isDarkMode ? '#ffffff' : '#000000';

  const handleNext = () => {
    if (step < 4) setStep(step + 1);
  };

  const handlePrev = () => {
    if (step > 1) setStep(step - 1);
  };

  const handleComplete = () => {
    if (onComplete) {
      onComplete(bendParams);
    }
    onClose();
  };

  if (!isOpen) return null;

  return (
    <div className="bend-wizard-overlay">
      <div className="bend-wizard-dialog" style={{ backgroundColor: bgColor, color: textColor }}>
        <div className="wizard-header">
          <h2>Bend Wizard - Step {step} of 4</h2>
          <button className="close-btn" onClick={onClose}>×</button>
        </div>

        <div className="wizard-body">
          <div className="wizard-progress">
            <div className={`step ${step >= 1 ? 'active' : ''}`}>1</div>
            <div className={`step ${step >= 2 ? 'active' : ''}`}>2</div>
            <div className={`step ${step >= 3 ? 'active' : ''}`}>3</div>
            <div className={`step ${step >= 4 ? 'active' : ''}`}>4</div>
          </div>

          {step === 1 && (
            <div className="wizard-step">
              <label>Bend Radius (mm):</label>
              <input
                type="number"
                value={bendParams.radius}
                onChange={(e) => setBendParams({ ...bendParams, radius: parseFloat(e.target.value) })}
              />
            </div>
          )}
          {step === 2 && (
            <div className="wizard-step">
              <label>Bend Angle (degrees):</label>
              <input
                type="number"
                value={bendParams.angle}
                onChange={(e) => setBendParams({ ...bendParams, angle: parseFloat(e.target.value) })}
              />
            </div>
          )}
          {step === 3 && (
            <div className="wizard-step">
              <label>Material:</label>
              <select value={bendParams.material} onChange={(e) => setBendParams({ ...bendParams, material: e.target.value })}>
                <option>steel</option>
                <option>aluminum</option>
                <option>copper</option>
              </select>
            </div>
          )}
          {step === 4 && (
            <div className="wizard-step">
              <label>Thickness (mm):</label>
              <input
                type="number"
                value={bendParams.thickness}
                onChange={(e) => setBendParams({ ...bendParams, thickness: parseFloat(e.target.value) })}
              />
            </div>
          )}
        </div>

        <div className="wizard-footer">
          <button className="btn btn-cancel" onClick={handlePrev} disabled={step === 1}>
            Previous
          </button>
          {step < 4 ? (
            <button className="btn btn-primary" onClick={handleNext}>
              Next
            </button>
          ) : (
            <button className="btn btn-primary" onClick={handleComplete}>
              Complete
            </button>
          )}
          <button className="btn btn-cancel" onClick={onClose}>
            Cancel
          </button>
        </div>
      </div>
    </div>
  );
};

export default BendWizard;
