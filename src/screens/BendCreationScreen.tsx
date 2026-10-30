import React, { useState } from 'react';
import {
  RibbonBar,
  PropertiesPanel,
  StatusBar,
  Badge,
  Icon,
  Divider,
  type RibbonTab,
  type PropertyField,
} from '../ui/components';
import './BendCreationScreen.css';

interface BendCreationScreenProps {
  tubeId: string;
}

interface Bend {
  id: string;
  name: string;
  angle: number;
  radius: number;
  position: number;
  radiusRatio?: number;
}

export const BendCreationScreen: React.FC<BendCreationScreenProps> = ({ tubeId }) => {
  const [activeTab, setActiveTab] = useState<string>('insert');
  const [selectedBendId, setSelectedBendId] = useState<string | null>(null);
  const [statusMessage, setStatusMessage] = useState('Bend creation ready');
  const [bends, setBends] = useState<Bend[]>([
    {
      id: 'bend-1',
      name: 'Bend_1',
      angle: 90,
      radius: 50,
      position: 100,
      radiusRatio: 2.0,
    },
    {
      id: 'bend-2',
      name: 'Bend_2',
      angle: 45,
      radius: 75,
      position: 250,
      radiusRatio: 3.0,
    },
  ]);

  const [bendProperties, setBendProperties] = useState<PropertyField[]>([
    {
      id: 'name',
      label: 'Name',
      type: 'text',
      value: 'Bend_1',
      section: 'Basic',
    },
    {
      id: 'angle',
      label: 'Angle (°)',
      type: 'number',
      value: 90,
      min: 5,
      max: 180,
      section: 'Geometry',
    },
    {
      id: 'radius',
      label: 'Radius (mm)',
      type: 'number',
      value: 50,
      min: 10,
      max: 500,
      section: 'Geometry',
    },
    {
      id: 'radius-ratio',
      label: 'Radius Ratio (R/D)',
      type: 'number',
      value: 2.0,
      min: 1.0,
      max: 5.0,
      section: 'Geometry',
    },
    {
      id: 'position',
      label: 'Position (mm)',
      type: 'number',
      value: 100,
      min: 0,
      max: 5000,
      section: 'Position',
    },
  ]);

  const predefinedAngles = [30, 45, 90, 135, 180];

  const handleAddBend = (angle: number) => {
    const newBend: Bend = {
      id: `bend-${bends.length + 1}`,
      name: `Bend_${bends.length + 1}`,
      angle,
      radius: 50,
      position: 200 + bends.length * 150,
      radiusRatio: 2.0,
    };
    setBends([...bends, newBend]);
    setStatusMessage(`✅ Added bend with ${angle}° angle`);
  };

  const handleSelectBend = (bendId: string) => {
    setSelectedBendId(bendId);
    const bend = bends.find((b) => b.id === bendId);
    if (bend) {
      setStatusMessage(`Selected: ${bend.name}`);
    }
  };

  const handlePropertyChange = (fieldId: string, value: any) => {
    setBendProperties((prev) =>
      prev.map((field) =>
        field.id === fieldId ? { ...field, value } : field
      )
    );

    if (selectedBendId) {
      setBends((prev) =>
        prev.map((bend) =>
          bend.id === selectedBendId
            ? { ...bend, [fieldId]: value }
            : bend
        )
      );
    }
  };

  const handleDeleteBend = (bendId: string) => {
    setBends((prev) => prev.filter((b) => b.id !== bendId));
    setStatusMessage(`Deleted bend`);
  };

  return (
    <div className="bend-creation-screen">
      {/* Ribbon */}
      <RibbonBar
        activeTab={activeTab}
        onTabChange={setActiveTab}
        onCommand={(cmd) => setStatusMessage(`Command: ${cmd}`)}
      />

      {/* Main Content */}
      <div className="bend-main">
        {/* Left Panel: Angle Selection */}
        <div className="bend-angles">
          <div className="angles-header">
            <h3>
              <Icon name="arc" size="sm" />
              Predefined Angles
            </h3>
          </div>

          <div className="angles-grid">
            {predefinedAngles.map((angle) => (
              <button
                key={angle}
                className="angle-btn"
                onClick={() => handleAddBend(angle)}
                title={`Add ${angle}° bend`}
              >
                <span className="angle-value">{angle}°</span>
              </button>
            ))}
          </div>

          <Divider orientation="horizontal" label="OR" />

          <div className="custom-angle">
            <label htmlFor="custom-angle">Custom Angle:</label>
            <input
              id="custom-angle"
              type="number"
              min="5"
              max="180"
              defaultValue="90"
              className="angle-input"
            />
            <button
              className="add-custom-btn"
              onClick={() => {
                const input = document.getElementById(
                  'custom-angle'
                ) as HTMLInputElement;
                const angle = parseInt(input.value);
                handleAddBend(angle);
              }}
            >
              Add Custom
            </button>
          </div>
        </div>

        {/* Center Panel: Bend List */}
        <div className="bend-list-container">
          <div className="list-header">
            <h3>
              <Icon name="tube" size="sm" />
              Bends List
            </h3>
            <Badge variant="info">{bends.length} bends</Badge>
          </div>

          <div className="bend-list">
            {bends.length > 0 ? (
              bends.map((bend) => (
                <div
                  key={bend.id}
                  className={`bend-item ${
                    selectedBendId === bend.id ? 'selected' : ''
                  }`}
                  onClick={() => handleSelectBend(bend.id)}
                >
                  <div className="bend-info">
                    <div className="bend-name">{bend.name}</div>
                    <div className="bend-details">
                      <span>{bend.angle}°</span>
                      <span>•</span>
                      <span>R{bend.radius}mm</span>
                    </div>
                  </div>
                  <button
                    className="bend-delete"
                    onClick={(e) => {
                      e.stopPropagation();
                      handleDeleteBend(bend.id);
                    }}
                    title="Delete"
                  >
                    ×
                  </button>
                </div>
              ))
            ) : (
              <div className="empty-state">No bends yet. Add one to start.</div>
            )}
          </div>
        </div>

        {/* Right Panel: Properties */}
        <div className="bend-properties">
          {selectedBendId ? (
            <PropertiesPanel
              title={selectedBendId}
              fields={bendProperties}
              onPropertyChange={handlePropertyChange}
            />
          ) : (
            <div className="no-selection">
              <Icon name="settings" size="lg" />
              <p>Select a bend to edit properties</p>
            </div>
          )}
        </div>
      </div>

      {/* Status Bar */}
      <StatusBar message={statusMessage} mode="Bend Design" />
    </div>
  );
};
