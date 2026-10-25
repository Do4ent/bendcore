import React, { useState, useCallback, useMemo, useEffect } from 'react';

/**
 * CAD-CORE-027: PropertyPanel
 * Properties and parameters panel UI with real-time updates
 * ~600 LOC + tests
 */

export interface Property {
  /** Property unique identifier */
  id: string;
  /** Property display name */
  name: string;
  /** Property value */
  value: any;
  /** Property type */
  type: 'string' | 'number' | 'boolean' | 'color' | 'material' | 'dimension';
  /** Is read-only */
  readOnly?: boolean;
  /** Is hidden */
  hidden?: boolean;
  /** Section this property belongs to */
  section?: string;
  /** Property metadata */
  metadata?: Record<string, any>;
}

export interface PropertyPanelProps {
  /** Array of properties to display */
  properties: Property[];
  /** Callback on property value change */
  onPropertyChange?: (propertyId: string, value: any) => void;
  /** Selected object ID */
  selectedObjectId?: string;
  /** Object name */
  objectName?: string;
  /** Show section headers */
  showSections?: boolean;
  /** Enable inline editing */
  enableInlineEdit?: boolean;
  /** Custom property renderer */
  customRenderer?: (property: Property) => React.ReactNode;
  /** On property focus */
  onPropertyFocus?: (propertyId: string) => void;
  /** On property blur */
  onPropertyBlur?: (propertyId: string) => void;
  /** Searchable */
  searchable?: boolean;
}

/**
 * PropertyPanel Component
 * Displays and edits properties of selected CAD-CORE objects
 */
export const PropertyPanel: React.FC<PropertyPanelProps> = ({
  properties,
  onPropertyChange,
  selectedObjectId,
  objectName,
  showSections = true,
  enableInlineEdit = true,
  customRenderer,
  onPropertyFocus,
  onPropertyBlur,
  searchable = true,
}) => {
  const [searchQuery, setSearchQuery] = useState('');
  const [expandedSections, setExpandedSections] = useState<Set<string>>(
    new Set(properties.map((p) => p.section || 'General'))
  );
  const [editingId, setEditingId] = useState<string | null>(null);
  const [editingValue, setEditingValue] = useState<any>(null);

  /**
   * Group properties by section
   */
  const groupedProperties = useMemo(() => {
    const groups: Record<string, Property[]> = {};
    properties
      .filter((p) => !p.hidden)
      .forEach((p) => {
        const section = p.section || 'General';
        if (!groups[section]) groups[section] = [];
        groups[section].push(p);
      });
    return groups;
  }, [properties]);

  /**
   * Filter properties based on search
   */
  const filteredGroups = useMemo(() => {
    if (!searchQuery) return groupedProperties;

    const filtered: Record<string, Property[]> = {};
    Object.entries(groupedProperties).forEach(([section, props]) => {
      const sectionMatches = section.toLowerCase().includes(searchQuery.toLowerCase());
      const matchingProps = props.filter((p) =>
        p.name.toLowerCase().includes(searchQuery.toLowerCase()) ||
        String(p.value).toLowerCase().includes(searchQuery.toLowerCase())
      );

      if (sectionMatches || matchingProps.length > 0) {
        filtered[section] = matchingProps.length > 0 ? matchingProps : props;
      }
    });
    return filtered;
  }, [groupedProperties, searchQuery]);

  /**
   * Toggle section expansion
   */
  const toggleSection = useCallback((section: string) => {
    setExpandedSections((prev) => {
      const newSet = new Set(prev);
      if (newSet.has(section)) {
        newSet.delete(section);
      } else {
        newSet.add(section);
      }
      return newSet;
    });
  }, []);

  /**
   * Handle property value change
   */
  const handlePropertyChange = useCallback(
    (propertyId: string, value: any) => {
      onPropertyChange?.(propertyId, value);
      setEditingId(null);
    },
    [onPropertyChange]
  );

  /**
   * Start editing a property
   */
  const startEditing = useCallback((property: Property) => {
    if (!property.readOnly && enableInlineEdit) {
      setEditingId(property.id);
      setEditingValue(property.value);
      onPropertyFocus?.(property.id);
    }
  }, [enableInlineEdit, onPropertyFocus]);

  /**
   * Finish editing
   */
  const finishEditing = useCallback(() => {
    if (editingId && editingValue !== null) {
      const property = properties.find((p) => p.id === editingId);
      if (property) {
        handlePropertyChange(editingId, editingValue);
      }
    }
    onPropertyBlur?.(editingId || '');
    setEditingId(null);
    setEditingValue(null);
  }, [editingId, editingValue, properties, handlePropertyChange, onPropertyBlur]);

  /**
   * Render property value based on type
   */
  const renderPropertyValue = (property: Property) => {
    if (customRenderer) {
      return customRenderer(property);
    }

    const isEditing = editingId === property.id;
    const value = isEditing ? editingValue : property.value;

    const baseStyle: React.CSSProperties = {
      width: '100%',
      padding: '4px 6px',
      fontSize: '11px',
      border: isEditing ? '1px solid #0066cc' : '1px solid #444',
      borderRadius: '3px',
      backgroundColor: isEditing ? '#1a1a1a' : property.readOnly ? '#2a2a2a' : '#1a1a1a',
      color: property.readOnly ? '#888' : '#fff',
      fontFamily: property.type === 'material' ? 'monospace' : 'inherit',
      cursor: isEditing ? 'text' : property.readOnly ? 'default' : 'pointer',
    };

    switch (property.type) {
      case 'number':
        return (
          <input
            type="number"
            value={value}
            onChange={(e) => setEditingValue(parseFloat(e.target.value))}
            onBlur={finishEditing}
            disabled={property.readOnly}
            step={0.01}
            style={baseStyle}
            autoFocus={isEditing}
          />
        );

      case 'string':
        return (
          <input
            type="text"
            value={value}
            onChange={(e) => setEditingValue(e.target.value)}
            onBlur={finishEditing}
            disabled={property.readOnly}
            style={baseStyle}
            autoFocus={isEditing}
          />
        );

      case 'boolean':
        return (
          <input
            type="checkbox"
            checked={value}
            onChange={(e) => {
              setEditingValue(e.target.checked);
              handlePropertyChange(property.id, e.target.checked);
            }}
            disabled={property.readOnly}
            style={{ cursor: property.readOnly ? 'default' : 'pointer' }}
          />
        );

      case 'color':
        return (
          <input
            type="color"
            value={value}
            onChange={(e) => {
              setEditingValue(e.target.value);
              handlePropertyChange(property.id, e.target.value);
            }}
            disabled={property.readOnly}
            style={{ ...baseStyle, width: '100%', height: '28px', padding: '2px' }}
          />
        );

      case 'dimension':
        return (
          <div
            style={{
              display: 'flex',
              gap: '4px',
              alignItems: 'center',
            }}
          >
            <input
              type="number"
              value={value}
              onChange={(e) => setEditingValue(parseFloat(e.target.value))}
              onBlur={finishEditing}
              disabled={property.readOnly}
              style={{ ...baseStyle, flex: 1 }}
              autoFocus={isEditing}
            />
            <span
              style={{
                fontSize: '10px',
                opacity: 0.6,
                minWidth: '30px',
              }}
            >
              {property.metadata?.unit || 'mm'}
            </span>
          </div>
        );

      case 'material':
        return (
          <div
            onClick={() => startEditing(property)}
            style={{
              ...baseStyle,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'space-between',
              padding: '4px 8px',
              cursor: 'pointer',
            }}
          >
            <div
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: '6px',
              }}
            >
              <div
                style={{
                  width: '16px',
                  height: '16px',
                  backgroundColor: property.metadata?.color || '#666',
                  borderRadius: '2px',
                  border: '1px solid #444',
                }}
              />
              <span>{value}</span>
            </div>
            <span style={{ fontSize: '9px', opacity: 0.5 }}>▾</span>
          </div>
        );

      default:
        return (
          <div
            onClick={() => startEditing(property)}
            style={baseStyle}
          >
            {String(value)}
          </div>
        );
    }
  };

  /**
   * Get visibility of properties based on selection
   */
  const visibleProperties = useMemo(() => {
    if (!selectedObjectId) {
      return [];
    }
    return properties.filter((p) => !p.hidden);
  }, [properties, selectedObjectId]);

  if (visibleProperties.length === 0) {
    return (
      <div
        style={{
          padding: '16px',
          backgroundColor: '#222',
          color: '#888',
          fontSize: '12px',
          textAlign: 'center',
          borderRadius: '4px',
        }}
      >
        {selectedObjectId ? 'No properties available' : 'Select an object to view properties'}
      </div>
    );
  }

  return (
    <div
      style={{
        padding: '12px',
        backgroundColor: '#222',
        color: '#fff',
        fontSize: '12px',
        fontFamily: 'system-ui, -apple-system, sans-serif',
        borderRadius: '4px',
        overflow: 'auto',
        maxHeight: '600px',
      }}
    >
      {/* Header */}
      <div
        style={{
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          marginBottom: '12px',
          paddingBottom: '8px',
          borderBottom: '1px solid #444',
        }}
      >
        <div>
          <h3 style={{ margin: 0, fontSize: '14px', fontWeight: 600 }}>Properties</h3>
          {objectName && (
            <span style={{ fontSize: '10px', opacity: 0.6, marginTop: '2px', display: 'block' }}>
              {objectName}
            </span>
          )}
        </div>
      </div>

      {/* Search */}
      {searchable && (
        <div style={{ marginBottom: '12px' }}>
          <input
            type="text"
            placeholder="Search properties..."
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            style={{
              width: '100%',
              padding: '6px 8px',
              fontSize: '11px',
              border: '1px solid #444',
              borderRadius: '3px',
              backgroundColor: '#1a1a1a',
              color: '#fff',
            }}
          />
        </div>
      )}

      {/* Properties grouped by section */}
      {Object.entries(filteredGroups).map(([section, sectionProps]) => (
        <div key={section} style={{ marginBottom: '12px' }}>
          {/* Section header */}
          {showSections && (
            <div
              onClick={() => toggleSection(section)}
              style={{
                padding: '6px 8px',
                backgroundColor: '#333',
                cursor: 'pointer',
                borderRadius: '3px',
                marginBottom: '6px',
                display: 'flex',
                alignItems: 'center',
                gap: '6px',
                userSelect: 'none',
              }}
            >
              <span style={{ fontSize: '10px' }}>
                {expandedSections.has(section) ? '▼' : '▶'}
              </span>
              <span style={{ fontWeight: 500, fontSize: '12px' }}>{section}</span>
              <span
                style={{
                  fontSize: '9px',
                  opacity: 0.5,
                  marginLeft: 'auto',
                }}
              >
                {sectionProps.length}
              </span>
            </div>
          )}

          {/* Section properties */}
          {expandedSections.has(section) && (
            <div
              style={{
                marginLeft: showSections ? '8px' : '0',
                display: 'flex',
                flexDirection: 'column',
                gap: '8px',
              }}
            >
              {sectionProps.map((property) => (
                <div key={property.id}>
                  <div
                    style={{
                      display: 'flex',
                      justifyContent: 'space-between',
                      alignItems: 'center',
                      marginBottom: '4px',
                      fontSize: '11px',
                    }}
                  >
                    <label
                      style={{
                        fontWeight: 500,
                        opacity: property.readOnly ? 0.6 : 1,
                      }}
                    >
                      {property.name}
                    </label>
                    {property.readOnly && (
                      <span
                        style={{
                          fontSize: '8px',
                          opacity: 0.4,
                          textTransform: 'uppercase',
                        }}
                      >
                        Read-only
                      </span>
                    )}
                  </div>
                  <div
                    onClick={() => startEditing(property)}
                    style={{
                      cursor: property.readOnly || !enableInlineEdit ? 'default' : 'text',
                    }}
                  >
                    {renderPropertyValue(property)}
                  </div>
                </div>
              ))}
            </div>
          )}
        </div>
      ))}
    </div>
  );
};

export default PropertyPanel;
