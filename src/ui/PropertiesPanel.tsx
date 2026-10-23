/**
 * BendCore Properties Panel
 * Right sidebar showing properties of selected object
 * Phase 32: Professional UI
 */

import React, { useState, useCallback, useEffect, useRef } from 'react';
import './PropertiesPanel.css';
import PropertyGroups, { PropertyValue } from './PropertyGroups';

export interface PropertySchema {
  name: string;
  label: string;
  type: 'text' | 'number' | 'textarea' | 'select' | 'color' | 'toggle' | 'readonly';
  value?: any;
  options?: Array<{ label: string; value: any }>;
  unit?: string;
  min?: number;
  max?: number;
  step?: number;
  validation?: (value: any) => string | null;
  readonly?: boolean;
}

export interface PropertyGroup {
  id: string;
  title: string;
  collapsed?: boolean;
  properties: PropertySchema[];
}

export interface SelectedObject {
  id: string;
  type: 'tube' | 'sketch' | 'port' | 'vertex' | 'assembly' | 'document' | 'command';
  name?: string;
  groups: PropertyGroup[];
}

export interface PropertiesPanelProps {
  selectedObject?: SelectedObject;
  width?: number;
  onPropertyChange?: (objectId: string, propertyName: string, value: any) => void;
  onClose?: () => void;
  isPinned?: boolean;
  onPinChange?: (isPinned: boolean) => void;
}

export const PropertiesPanel: React.FC<PropertiesPanelProps> = ({
  selectedObject,
  width = 350,
  onPropertyChange,
  onClose,
  isPinned = false,
  onPinChange,
}) => {
  const [collapsedGroups, setCollapsedGroups] = useState<Set<string>>(new Set());
  const [searchText, setSearchText] = useState('');
  const [localChanges, setLocalChanges] = useState<Record<string, any>>({});
  const panelRef = useRef<HTMLDivElement>(null);

  // Handle property change with debouncing
  const handlePropertyChange = useCallback(
    (propertyName: string, value: any) => {
      if (!selectedObject) return;

      setLocalChanges((prev) => ({ ...prev, [propertyName]: value }));

      // Debounce real change
      const timeout = setTimeout(() => {
        if (onPropertyChange) {
          onPropertyChange(selectedObject.id, propertyName, value);
        }
      }, 300);

      return () => clearTimeout(timeout);
    },
    [selectedObject, onPropertyChange]
  );

  const toggleGroupCollapse = (groupId: string) => {
    setCollapsedGroups((prev) => {
      const newSet = new Set(prev);
      if (newSet.has(groupId)) {
        newSet.delete(groupId);
      } else {
        newSet.add(groupId);
      }
      return newSet;
    });
  };

  // Filter properties by search text
  const getFilteredGroups = (): PropertyGroup[] => {
    if (!selectedObject || !searchText.trim()) {
      return selectedObject?.groups || [];
    }

    const lowerSearch = searchText.toLowerCase();
    return (
      selectedObject.groups
        ?.map((group) => ({
          ...group,
          properties: group.properties.filter(
            (prop) =>
              prop.label.toLowerCase().includes(lowerSearch) ||
              prop.name.toLowerCase().includes(lowerSearch)
          ),
        }))
        .filter((group) => group.properties.length > 0) || []
    );
  };

  if (!selectedObject && !isPinned) {
    return null;
  }

  const filteredGroups = getFilteredGroups();
  const hasProperties =
    selectedObject && selectedObject.groups && selectedObject.groups.length > 0;

  return (
    <div
      ref={panelRef}
      className="bendcore-properties-panel"
      style={{ width: `${width}px` }}
    >
      {/* Header */}
      <div className="properties-header">
        <div className="properties-title">
          <span className="title-text">Properties</span>
          {selectedObject && (
            <span className="title-object-type">{selectedObject.type}</span>
          )}
        </div>

        <div className="properties-controls">
          <button
            className={`pin-button ${isPinned ? 'pinned' : ''}`}
            onClick={() => onPinChange?.(!isPinned)}
            title={isPinned ? 'Unpin panel' : 'Pin panel open'}
            aria-label="Pin panel"
          >
            📌
          </button>
          {onClose && (
            <button
              className="close-button"
              onClick={onClose}
              title="Close panel"
              aria-label="Close panel"
            >
              ✕
            </button>
          )}
        </div>
      </div>

      {/* Search */}
      <div className="properties-search">
        <input
          type="text"
          placeholder="Search properties..."
          value={searchText}
          onChange={(e) => setSearchText(e.target.value)}
          className="search-input"
        />
        {searchText && (
          <button
            className="search-clear"
            onClick={() => setSearchText('')}
            title="Clear search"
          >
            ✕
          </button>
        )}
      </div>

      {/* Content */}
      <div className="properties-content">
        {!selectedObject ? (
          <div className="properties-empty">
            <p>No object selected</p>
            <span className="empty-hint">Click an object to view its properties</span>
          </div>
        ) : !hasProperties ? (
          <div className="properties-empty">
            <p>No properties available</p>
          </div>
        ) : filteredGroups.length === 0 && searchText ? (
          <div className="properties-empty">
            <p>No matching properties</p>
          </div>
        ) : (
          filteredGroups.map((group) => (
            <PropertyGroup
              key={group.id}
              group={group}
              collapsed={collapsedGroups.has(group.id)}
              onToggleCollapse={() => toggleGroupCollapse(group.id)}
              onPropertyChange={handlePropertyChange}
              localChanges={localChanges}
            />
          ))
        )}
      </div>
    </div>
  );
};

interface PropertyGroupProps {
  group: PropertyGroup;
  collapsed: boolean;
  onToggleCollapse: () => void;
  onPropertyChange: (propertyName: string, value: any) => void;
  localChanges: Record<string, any>;
}

const PropertyGroup: React.FC<PropertyGroupProps> = ({
  group,
  collapsed,
  onToggleCollapse,
  onPropertyChange,
  localChanges,
}) => {
  return (
    <div className="property-group">
      <div
        className={`property-group-header ${collapsed ? 'collapsed' : ''}`}
        onClick={onToggleCollapse}
      >
        <span className="group-toggle">
          {collapsed ? '▶' : '▼'}
        </span>
        <h3 className="group-title">{group.title}</h3>
        <span className="group-count">
          {group.properties.length}
        </span>
      </div>

      {!collapsed && (
        <div className="property-group-content">
          {group.properties.map((prop) => (
            <PropertyField
              key={prop.name}
              property={prop}
              value={
                localChanges.hasOwnProperty(prop.name)
                  ? localChanges[prop.name]
                  : prop.value
              }
              onChange={(value) => onPropertyChange(prop.name, value)}
            />
          ))}
        </div>
      )}
    </div>
  );
};

interface PropertyFieldProps {
  property: PropertySchema;
  value: any;
  onChange: (value: any) => void;
}

const PropertyField: React.FC<PropertyFieldProps> = ({
  property,
  value,
  onChange,
}) => {
  const [validationError, setValidationError] = useState<string | null>(null);

  const handleChange = (newValue: any) => {
    if (property.validation) {
      const error = property.validation(newValue);
      setValidationError(error);
      if (error) return;
    }

    onChange(newValue);
  };

  return (
    <div className={`property-field ${property.readonly ? 'readonly' : ''}`}>
      <label className="property-label">{property.label}</label>

      <div className="property-input-wrapper">
        {property.type === 'readonly' ? (
          <span className="property-readonly-value">{value ?? '—'}</span>
        ) : property.type === 'text' ? (
          <input
            type="text"
            value={value ?? ''}
            onChange={(e) => handleChange(e.target.value)}
            className="property-input"
            disabled={property.readonly}
          />
        ) : property.type === 'number' ? (
          <input
            type="number"
            value={value ?? ''}
            onChange={(e) =>
              handleChange(e.target.value ? parseFloat(e.target.value) : null)
            }
            min={property.min}
            max={property.max}
            step={property.step}
            className="property-input"
            disabled={property.readonly}
          />
        ) : property.type === 'textarea' ? (
          <textarea
            value={value ?? ''}
            onChange={(e) => handleChange(e.target.value)}
            className="property-textarea"
            rows={3}
            disabled={property.readonly}
          />
        ) : property.type === 'select' ? (
          <select
            value={value ?? ''}
            onChange={(e) => handleChange(e.target.value)}
            className="property-select"
            disabled={property.readonly}
          >
            <option value="">—</option>
            {property.options?.map((opt) => (
              <option key={opt.value} value={opt.value}>
                {opt.label}
              </option>
            ))}
          </select>
        ) : property.type === 'color' ? (
          <input
            type="color"
            value={value ?? '#000000'}
            onChange={(e) => handleChange(e.target.value)}
            className="property-color"
            disabled={property.readonly}
          />
        ) : property.type === 'toggle' ? (
          <label className="property-toggle">
            <input
              type="checkbox"
              checked={value ?? false}
              onChange={(e) => handleChange(e.target.checked)}
              disabled={property.readonly}
            />
            <span className="toggle-label">{value ? 'On' : 'Off'}</span>
          </label>
        ) : null}

        {property.unit && property.type === 'number' && (
          <span className="property-unit">{property.unit}</span>
        )}
      </div>

      {validationError && (
        <div className="property-error">{validationError}</div>
      )}
    </div>
  );
};

export default PropertiesPanel;
