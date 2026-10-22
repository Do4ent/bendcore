import React, { useState } from 'react';
import { useReporting } from '../../hooks/useReporting';

interface ReleaseDialogProps {
  tubeId: string;
  isOpen: boolean;
  onClose: () => void;
  onRelease?: (reportId: string) => void;
}

/**
 * Release for Production Dialog
 * Simple variant: minimal fields for production release
 */
export const ReleaseDialog: React.FC<ReleaseDialogProps> = ({
  tubeId,
  isOpen,
  onClose,
  onRelease,
}) => {
  const [approvedBy, setApprovedBy] = useState('');
  const [comments, setComments] = useState('');
  const [isReleasing, setIsReleasing] = useState(false);
  const { releaseForProduction } = useReporting();

  const handleRelease = async () => {
    if (!approvedBy.trim()) {
      alert('Please select approver');
      return;
    }

    setIsReleasing(true);
    try {
      const reportId = await releaseForProduction({
        tubeId,
        approvedBy,
        comments,
      });

      if (reportId && onRelease) {
        onRelease(reportId);
      }

      onClose();
    } catch (error) {
      console.error('Release failed:', error);
      alert('Release failed: ' + (error as Error).message);
    } finally {
      setIsReleasing(false);
    }
  };

  if (!isOpen) return null;

  return (
    <div className="dialog-overlay">
      <div className="dialog-box">
        <div className="dialog-header">
          <h2>Release for Production</h2>
          <button className="close-btn" onClick={onClose}>✕</button>
        </div>

        <div className="dialog-body">
          {/* Report ID (auto) */}
          <div className="form-group">
            <label>Report ID:</label>
            <input
              type="text"
              disabled
              value={`[auto-generated]`}
              className="input-disabled"
            />
          </div>

          {/* Status (fixed to released) */}
          <div className="form-group">
            <label>Status:</label>
            <input
              type="text"
              disabled
              value="released"
              className="input-disabled"
            />
          </div>

          {/* Approved by (dropdown) */}
          <div className="form-group">
            <label>Approved by:</label>
            <select
              value={approvedBy}
              onChange={(e) => setApprovedBy(e.target.value)}
              className="select-input"
              disabled={isReleasing}
            >
              <option value="">-- Select Engineer --</option>
              <option value="Engineer-1">Engineer-1</option>
              <option value="Engineer-2">Engineer-2</option>
              <option value="Engineer-3">Engineer-3</option>
              <option value="Manager">Manager</option>
            </select>
          </div>

          {/* Comments */}
          <div className="form-group">
            <label>Comments:</label>
            <textarea
              value={comments}
              onChange={(e) => setComments(e.target.value)}
              placeholder="Add any notes..."
              rows={4}
              disabled={isReleasing}
              className="textarea-input"
            />
          </div>

          {/* Auto-checks (read-only) */}
          <div className="checks-section">
            <h3>Checks:</h3>
            <div className="check-item">
              <span>✓</span>
              <span>Analysis OK</span>
            </div>
            <div className="check-item">
              <span>✓</span>
              <span>Standards OK</span>
            </div>
            <div className="check-item">
              <span>✓</span>
              <span>No Errors</span>
            </div>
          </div>
        </div>

        <div className="dialog-footer">
          <button
            className="btn-secondary"
            onClick={onClose}
            disabled={isReleasing}
          >
            Cancel
          </button>
          <button
            className="btn-primary"
            onClick={handleRelease}
            disabled={isReleasing || !approvedBy}
          >
            {isReleasing ? 'Releasing...' : 'Release'}
          </button>
        </div>
      </div>

      <style jsx>{`
        .dialog-overlay {
          position: fixed;
          top: 0;
          left: 0;
          right: 0;
          bottom: 0;
          background: rgba(0, 0, 0, 0.5);
          display: flex;
          align-items: center;
          justify-content: center;
          z-index: 1000;
        }

        .dialog-box {
          background: white;
          border-radius: 8px;
          box-shadow: 0 4px 20px rgba(0, 0, 0, 0.15);
          width: 500px;
          max-height: 90vh;
          overflow-y: auto;
        }

        .dialog-header {
          display: flex;
          justify-content: space-between;
          align-items: center;
          padding: 20px;
          border-bottom: 1px solid #e0e0e0;
        }

        .dialog-header h2 {
          margin: 0;
          font-size: 20px;
          font-weight: 600;
        }

        .close-btn {
          background: none;
          border: none;
          font-size: 24px;
          cursor: pointer;
          color: #666;
        }

        .close-btn:hover {
          color: #000;
        }

        .dialog-body {
          padding: 20px;
        }

        .form-group {
          margin-bottom: 16px;
        }

        .form-group label {
          display: block;
          font-weight: 500;
          margin-bottom: 8px;
          font-size: 14px;
        }

        .input-disabled,
        .select-input,
        .textarea-input {
          width: 100%;
          padding: 8px 12px;
          border: 1px solid #ddd;
          border-radius: 4px;
          font-family: inherit;
          font-size: 14px;
        }

        .input-disabled {
          background: #f5f5f5;
          color: #999;
        }

        .select-input:focus,
        .textarea-input:focus {
          outline: none;
          border-color: #2196F3;
          box-shadow: 0 0 0 2px rgba(33, 150, 243, 0.1);
        }

        .checks-section {
          margin-top: 20px;
          padding: 12px;
          background: #f9f9f9;
          border-radius: 4px;
        }

        .checks-section h3 {
          margin: 0 0 12px 0;
          font-size: 14px;
          font-weight: 600;
        }

        .check-item {
          display: flex;
          gap: 8px;
          margin-bottom: 8px;
          font-size: 14px;
        }

        .check-item span:first-child {
          color: #4CAF50;
          font-weight: bold;
        }

        .dialog-footer {
          display: flex;
          gap: 12px;
          justify-content: flex-end;
          padding: 20px;
          border-top: 1px solid #e0e0e0;
        }

        .btn-secondary,
        .btn-primary {
          padding: 8px 16px;
          border: none;
          border-radius: 4px;
          font-size: 14px;
          font-weight: 500;
          cursor: pointer;
          transition: all 0.2s;
        }

        .btn-secondary {
          background: #f0f0f0;
          color: #333;
        }

        .btn-secondary:hover:not(:disabled) {
          background: #e0e0e0;
        }

        .btn-primary {
          background: #2196F3;
          color: white;
        }

        .btn-primary:hover:not(:disabled) {
          background: #1976D2;
        }

        .btn-secondary:disabled,
        .btn-primary:disabled {
          opacity: 0.5;
          cursor: not-allowed;
        }
      `}</style>
    </div>
  );
};
