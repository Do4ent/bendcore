/**
 * BendCore Phase 35C: Tube Properties Panel
 * React component for displaying and managing tube properties
 */

import React, { useState, useEffect } from 'react';
import './TubePropertiesPanel.css';

interface Vector3 {
  x: number;
  y: number;
  z: number;
}

interface Cylinder {
  center: Vector3;
  axis: Vector3;
  length: number;
  outer_radius: number;
  inner_radius: number;
  segment_id: string;
}

interface Torus {
  center: Vector3;
  axis: Vector3;
  major_radius: number;
  minor_radius: number;
  start_angle: number;
  end_angle: number;
  segment_id: string;
}

interface TubeBody {
  tube_id: string;
  total_length: number;
  total_volume: number;
  total_mass: number;
  valid: boolean;
  straight_sections: Cylinder[];
  bend_sections: Torus[];
  errors: string[];
  warnings: string[];
}

interface BendingProfile {
  profile_id: string;
  name: string;
  radius: number;
  min_bend_angle: number;
  max_bend_angle: number;
  min_straight: number;
}

interface TubeSpecification {
  tube_id: string;
  outer_diameter: number;
  inner_diameter: number;
  material_id: string;
  density: number;
  designation: string;
}

interface TubePropertiesPanelProps {
  tube: TubeBody;
  profile: BendingProfile;
  spec: TubeSpecification;
  onClose?: () => void;
  onExport?: (format: string) => void;
}

export const TubePropertiesPanel: React.FC<TubePropertiesPanelProps> = ({
  tube,
  profile,
  spec,
  onClose,
  onExport,
}) => {
  const [activeTab, setActiveTab] = useState<'summary' | 'geometry' | 'bends' | 'validation'>('summary');
  const [expandedBends, setExpandedBends] = useState<Set<string>>(new Set());

  const toggleBendExpanded = (bendId: string) => {
    const newSet = new Set(expandedBends);
    if (newSet.has(bendId)) {
      newSet.delete(bendId);
    } else {
      newSet.add(bendId);
    }
    setExpandedBends(newSet);
  };

  const calculateWallThickness = (): number => {
    return (spec.outer_diameter - spec.inner_diameter) / 2;
  };

  const formatNumber = (num: number, decimals: number = 2): string => {
    return num.toFixed(decimals);
  };

  // ========== SUMMARY TAB ==========

  const renderSummary = () => (
    <div className="tube-summary">
      <div className="summary-header">
        <h3>{spec.designation || tube.tube_id}</h3>
        <span className={`status-badge ${tube.valid ? 'valid' : 'invalid'}`}>
          {tube.valid ? '✓ Valid' : '✗ Invalid'}
        </span>
      </div>

      <div className="summary-grid">
        <div className="summary-item">
          <span className="label">Total Length</span>
          <span className="value">{formatNumber(tube.total_length)} mm</span>
        </div>

        <div className="summary-item">
          <span className="label">Total Mass</span>
          <span className="value">{formatNumber(tube.total_mass, 1)} g</span>
        </div>

        <div className="summary-item">
          <span className="label">Outer Diameter</span>
          <span className="value">{formatNumber(spec.outer_diameter, 1)} mm</span>
        </div>

        <div className="summary-item">
          <span className="label">Wall Thickness</span>
          <span className="value">{formatNumber(calculateWallThickness(), 2)} mm</span>
        </div>

        <div className="summary-item">
          <span className="label">Material</span>
          <span className="value">{spec.material_id}</span>
        </div>

        <div className="summary-item">
          <span className="label">Density</span>
          <span className="value">{formatNumber(spec.density, 2)} g/cm³</span>
        </div>

        <div className="summary-item">
          <span className="label">Number of Bends</span>
          <span className="value">{tube.bend_sections.length}</span>
        </div>

        <div className="summary-item">
          <span className="label">Profile</span>
          <span className="value">{profile.name}</span>
        </div>
      </div>
    </div>
  );

  // ========== GEOMETRY TAB ==========

  const renderGeometry = () => (
    <div className="tube-geometry">
      <div className="geometry-section">
        <h4>Volume & Mass</h4>
        <div className="info-grid">
          <div className="info-item">
            <span className="label">Total Volume</span>
            <span className="value">{formatNumber(tube.total_volume)} mm³</span>
          </div>
          <div className="info-item">
            <span className="label">Volume (cm³)</span>
            <span className="value">{formatNumber(tube.total_volume / 1000, 3)} cm³</span>
          </div>
          <div className="info-item">
            <span className="label">Total Mass</span>
            <span className="value">{formatNumber(tube.total_mass, 2)} g</span>
          </div>
          <div className="info-item">
            <span className="label">Total Mass (kg)</span>
            <span className="value">{formatNumber(tube.total_mass / 1000, 4)} kg</span>
          </div>
        </div>
      </div>

      <div className="geometry-section">
        <h4>Straight Sections ({tube.straight_sections.length})</h4>
        <div className="sections-list">
          {tube.straight_sections.map((cyl, idx) => (
            <div key={`straight-${idx}`} className="section-item">
              <span className="section-id">{cyl.segment_id}</span>
              <span className="section-length">{formatNumber(cyl.length)} mm</span>
              <span className="section-detail">
                {formatNumber(spec.outer_diameter)} × {formatNumber(spec.inner_diameter)} mm
              </span>
            </div>
          ))}
        </div>
      </div>

      <div className="geometry-section">
        <h4>Bend Sections ({tube.bend_sections.length})</h4>
        <div className="sections-list">
          {tube.bend_sections.map((torus, idx) => (
            <div key={`bend-${idx}`} className="section-item">
              <span className="section-id">{torus.segment_id}</span>
              <span className="section-angle">
                R {formatNumber(torus.major_radius)} mm
              </span>
              <span className="section-detail">
                Sweep: {formatNumber((torus.end_angle - torus.start_angle) * 180 / Math.PI, 1)}°
              </span>
            </div>
          ))}
        </div>
      </div>
    </div>
  );

  // ========== BENDS TAB ==========

  const renderBends = () => (
    <div className="tube-bends">
      <div className="bends-header">
        <h4>Bending Details</h4>
        <div className="bends-stats">
          <span>Total Bends: {tube.bend_sections.length}</span>
          <span>Bend Radius: {formatNumber(profile.radius)} mm</span>
          <span>Min Straight: {formatNumber(profile.min_straight)} mm</span>
        </div>
      </div>

      <div className="bends-list">
        {tube.bend_sections.map((bend, idx) => {
          const isExpanded = expandedBends.has(bend.segment_id);
          const sweepAngle = bend.end_angle - bend.start_angle;
          const sweepDegrees = (sweepAngle * 180) / Math.PI;

          return (
            <div key={bend.segment_id} className="bend-item">
              <div
                className="bend-header"
                onClick={() => toggleBendExpanded(bend.segment_id)}
              >
                <span className="bend-toggle">
                  {isExpanded ? '▼' : '▶'}
                </span>
                <span className="bend-id">Bend {idx + 1}</span>
                <span className="bend-angle">{formatNumber(sweepDegrees, 1)}°</span>
                <span className="bend-segment">{bend.segment_id}</span>
              </div>

              {isExpanded && (
                <div className="bend-details">
                  <div className="detail-row">
                    <span className="detail-label">Sweep Angle</span>
                    <span className="detail-value">{formatNumber(sweepDegrees, 2)}°</span>
                  </div>
                  <div className="detail-row">
                    <span className="detail-label">Major Radius</span>
                    <span className="detail-value">{formatNumber(bend.major_radius, 2)} mm</span>
                  </div>
                  <div className="detail-row">
                    <span className="detail-label">Minor Radius</span>
                    <span className="detail-value">{formatNumber(bend.minor_radius, 2)} mm</span>
                  </div>
                  <div className="detail-row">
                    <span className="detail-label">Arc Length</span>
                    <span className="detail-value">
                      {formatNumber(bend.major_radius * sweepAngle, 2)} mm
                    </span>
                  </div>
                  <div className="detail-row">
                    <span className="detail-label">Center</span>
                    <span className="detail-value">
                      ({formatNumber(bend.center.x, 1)}, {formatNumber(bend.center.y, 1)}, {formatNumber(bend.center.z, 1)})
                    </span>
                  </div>
                </div>
              )}
            </div>
          );
        })}
      </div>
    </div>
  );

  // ========== VALIDATION TAB ==========

  const renderValidation = () => (
    <div className="tube-validation">
      <div className="validation-status">
        <div className={`status-indicator ${tube.valid ? 'valid' : 'invalid'}`}>
          {tube.valid ? '✓' : '✗'}
        </div>
        <span className="status-text">
          {tube.valid ? 'All validations passed' : 'Validation failed'}
        </span>
      </div>

      {tube.errors.length > 0 && (
        <div className="validation-section errors">
          <h4>Errors ({tube.errors.length})</h4>
          <div className="error-list">
            {tube.errors.map((error, idx) => (
              <div key={`error-${idx}`} className="error-item">
                <span className="error-icon">⚠</span>
                <span className="error-text">{error}</span>
              </div>
            ))}
          </div>
        </div>
      )}

      {tube.warnings.length > 0 && (
        <div className="validation-section warnings">
          <h4>Warnings ({tube.warnings.length})</h4>
          <div className="warning-list">
            {tube.warnings.map((warning, idx) => (
              <div key={`warning-${idx}`} className="warning-item">
                <span className="warning-icon">ℹ</span>
                <span className="warning-text">{warning}</span>
              </div>
            ))}
          </div>
        </div>
      )}

      {tube.errors.length === 0 && tube.warnings.length === 0 && (
        <div className="no-issues">
          <p>✓ No issues found</p>
        </div>
      )}
    </div>
  );

  // ========== RENDER ==========

  return (
    <div className="tube-properties-panel">
      <div className="panel-header">
        <h2>Tube Properties</h2>
        {onClose && (
          <button className="close-button" onClick={onClose}>
            ✕
          </button>
        )}
      </div>

      <div className="panel-tabs">
        <button
          className={`tab ${activeTab === 'summary' ? 'active' : ''}`}
          onClick={() => setActiveTab('summary')}
        >
          Summary
        </button>
        <button
          className={`tab ${activeTab === 'geometry' ? 'active' : ''}`}
          onClick={() => setActiveTab('geometry')}
        >
          Geometry
        </button>
        <button
          className={`tab ${activeTab === 'bends' ? 'active' : ''}`}
          onClick={() => setActiveTab('bends')}
        >
          Bends
        </button>
        <button
          className={`tab ${activeTab === 'validation' ? 'active' : ''}`}
          onClick={() => setActiveTab('validation')}
        >
          Validation
        </button>
      </div>

      <div className="panel-content">
        {activeTab === 'summary' && renderSummary()}
        {activeTab === 'geometry' && renderGeometry()}
        {activeTab === 'bends' && renderBends()}
        {activeTab === 'validation' && renderValidation()}
      </div>

      <div className="panel-footer">
        {onExport && (
          <div className="export-buttons">
            <button
              className="export-btn"
              onClick={() => onExport('STEP')}
            >
              Export STEP
            </button>
            <button
              className="export-btn"
              onClick={() => onExport('JSON')}
            >
              Export JSON
            </button>
          </div>
        )}
      </div>
    </div>
  );
};

export default TubePropertiesPanel;
