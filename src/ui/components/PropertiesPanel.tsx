/**
 * BendCore UI Component: PropertiesPanel
 * Property editors for selected objects with grouped sections
 *
 * @component
 * @example
 * <PropertiesPanel
 *   selectedNode={node}
 *   properties={properties}
 *   onPropertyChange={handleChange}
 *   isDarkMode={false}
 * />
 */

import React, { useState, useCallback, useMemo } from 'react';
import './PropertiesPanel.css';

// ============ Types ============

export type PropertyValue = string | number | boolean | Color;

export interface Color {
  r: number;
  g: number;
  b: number;
  a?: number;
}

export interface PropertyField {
  id: string;
  label: string;
  type: 'text' | 'number' | 'select' | 'color' | 'checkbox' | 'textarea';
  value: PropertyValue;
  category?: string;
  min?: number;
  max?: number;
  step?: number;
  options?: { label: string; value: string | number }[];
  placeholder?: string;
  readonly?: boolean;
  disabled?: boolean;
  unit?: string;
  onChange: (value: PropertyValue) => void;
  validation?: (value: PropertyValue) => string | null;
}

interface PropertiesPanelProps {
  selectedNode?: any;
  properties: PropertyField[];
  onPropertyChange?: (fieldId: string, value: PropertyValue) => void;
  onReset?: () => void;
  isDarkMode?: boolean;
  groupByCategory?: boolean;
}

interface ValidationError {
  [fieldId: string]: string | null;
}

// ============ Component ============

export const PropertiesPanel: React.FC<PropertiesPanelProps> = ({
  selectedNode,
  properties,
  onPropertyChange,
  onReset,
  isDarkMode = false,
  groupByCategory = true,
}) => {
  const [expandedCategories, setExpandedCategories] = useState<Set<string>>(new Set(['Основное']));
  const [errors, setErrors] = useState<ValidationError>({});

  // Group properties by category
  const groupedProperties = useMemo(() => {
    if (!groupByCategory) {
      return { 'Основное': properties };
    }

    return properties.reduce<{ [key: string]: PropertyField[] }>((acc, prop) => {
      const category = prop.category || 'Основное';
      if (!acc[category]) {
        acc[category] = [];
      }
      acc[category].push(prop);
      return acc;
    }, {});
  }, [properties, groupByCategory]);

  // Validate field
  const validateField = useCallback((field: PropertyField): string | null => {
    if (field.validation) {
      return field.validation(field.value);
    }

    if (field.type === 'number') {
      const num = Number(field.value);
      if (field.min !== undefined && num < field.min) {
        return `Минимум: ${field.min}`;
      }
      if (field.max !== undefined && num > field.max) {
        return `Максимум: ${field.max}`;
      }
    }

    return null;
  }, []);

  // Handle value change
  const handleChange = useCallback(
    (field: PropertyField, newValue: PropertyValue) => {
      // Validate
      const error = validateField({ ...field, value: newValue });
      setErrors((prev) => ({
        ...prev,
        [field.id]: error,
      }));

      // Call onChange if no error
      if (!error) {
        field.onChange(newValue);
        onPropertyChange?.(field.id, newValue);
      }
    },
    [validateField, onPropertyChange]
  );

  // Toggle category
  const toggleCategory = useCallback((category: string) => {
    setExpandedCategories((prev) => {
      const newSet = new Set(prev);
      if (newSet.has(category)) {
        newSet.delete(category);
      } else {
        newSet.add(category);
      }
      return newSet;
    });
  }, []);

  // Handle reset
  const handleReset = useCallback(() => {
    setErrors({});
    onReset?.();
  }, [onReset]);

  return (
    <div
      className={`properties-panel ${isDarkMode ? 'properties-panel--dark' : ''}`}
      role="region"
      aria-label="Панель свойств"
    >
      {/* === Header === */}
      {selectedNode && (
        <div className="properties-panel__header">
          <h3 className="properties-panel__title">{selectedNode.name}</h3>
          {onReset && (
            <button
              className="properties-panel__reset-btn"
              onClick={handleReset}
              title="Восстановить значения по умолчанию"
              aria-label="Восстановить значения"
            >
              ↻
            </button>
          )}
        </div>
      )}

      {/* === Properties List === */}
      <div className="properties-panel__content">
        {Object.entries(groupedProperties).map(([category, fields]) => (
          <PropertiesCategory
            key={category}
            category={category}
            fields={fields}
            isExpanded={expandedCategories.has(category)}
            onToggle={() => toggleCategory(category)}
            onFieldChange={handleChange}
            errors={errors}
          />
        ))}
      </div>
    </div>
  );
};

// ============ Category Component ============

interface PropertiesCategoryProps {
  category: string;
  fields: PropertyField[];
  isExpanded: boolean;
  onToggle: () => void;
  onFieldChange: (field: PropertyField, value: PropertyValue) => void;
  errors: ValidationError;
}

const PropertiesCategory: React.FC<PropertiesCategoryProps> = ({
  category,
  fields,
  isExpanded,
  onToggle,
  onFieldChange,
  errors,
}) => {
  return (
    <div className="properties-category">
      {/* === Category Header === */}
      <button
        className={`properties-category__header ${
          isExpanded ? 'properties-category__header--open' : ''
        }`}
        onClick={onToggle}
        aria-expanded={isExpanded}
        aria-controls={`properties-category-${category}`}
      >
        <span className="properties-category__toggle">▶</span>
        <span className="properties-category__title">{category}</span>
      </button>

      {/* === Category Fields === */}
      {isExpanded && (
        <div id={`properties-category-${category}`} className="properties-category__fields">
          {fields.map((field) => (
            <PropertyField
              key={field.id}
              field={field}
              error={errors[field.id]}
              onChange={(value) => onFieldChange(field, value)}
            />
          ))}
        </div>
      )}
    </div>
  );
};

// ============ Property Field Component ============

interface PropertyFieldProps {
  field: PropertyField;
  error?: string | null;
  onChange: (value: PropertyValue) => void;
}

const PropertyField: React.FC<PropertyFieldProps> = ({ field, error, onChange }) => {
  const [colorPickerOpen, setColorPickerOpen] = useState(false);

  return (
    <div
      className={`property-field ${field.type === 'checkbox' ? 'property-field--checkbox' : ''} ${
        error ? 'property-field--error' : ''
      }`}
    >
      {/* === Label === */}
      <label className="property-field__label" htmlFor={field.id}>
        {field.label}
        {field.unit && <span className="property-field__unit">({field.unit})</span>}
      </label>

      {/* === Input by Type === */}
      <div className="property-field__input-wrapper">
        {field.type === 'text' && (
          <input
            id={field.id}
            type="text"
            className="property-field__input property-field__input--text"
            value={field.value as string}
            onChange={(e) => onChange(e.target.value)}
            placeholder={field.placeholder}
            disabled={field.disabled || field.readonly}
            readOnly={field.readonly}
            aria-invalid={!!error}
            aria-describedby={error ? `${field.id}-error` : undefined}
          />
        )}

        {field.type === 'number' && (
          <input
            id={field.id}
            type="number"
            className="property-field__input property-field__input--number"
            value={field.value as number}
            onChange={(e) => onChange(e.target.value ? Number(e.target.value) : 0)}
            min={field.min}
            max={field.max}
            step={field.step || 1}
            disabled={field.disabled || field.readonly}
            readOnly={field.readonly}
            aria-invalid={!!error}
            aria-describedby={error ? `${field.id}-error` : undefined}
          />
        )}

        {field.type === 'select' && (
          <select
            id={field.id}
            className="property-field__input property-field__input--select"
            value={field.value as string}
            onChange={(e) => onChange(e.target.value)}
            disabled={field.disabled || field.readonly}
            aria-invalid={!!error}
            aria-describedby={error ? `${field.id}-error` : undefined}
          >
            <option value="">-- Выберите --</option>
            {field.options?.map((opt) => (
              <option key={opt.value} value={opt.value}>
                {opt.label}
              </option>
            ))}
          </select>
        )}

        {field.type === 'color' && (
          <div className="property-field__color-wrapper">
            <input
              id={field.id}
              type="color"
              className="property-field__input property-field__input--color"
              value={colorToHex(field.value as Color)}
              onChange={(e) => onChange(hexToColor(e.target.value))}
              disabled={field.disabled || field.readonly}
              aria-invalid={!!error}
            />
            <button
              className="property-field__color-button"
              onClick={() => setColorPickerOpen(!colorPickerOpen)}
              title="Открыть выбор цвета"
            >
              ▼
            </button>
          </div>
        )}

        {field.type === 'checkbox' && (
          <input
            id={field.id}
            type="checkbox"
            className="property-field__input property-field__input--checkbox"
            checked={field.value as boolean}
            onChange={(e) => onChange(e.target.checked)}
            disabled={field.disabled || field.readonly}
            aria-invalid={!!error}
          />
        )}

        {field.type === 'textarea' && (
          <textarea
            id={field.id}
            className="property-field__input property-field__input--textarea"
            value={field.value as string}
            onChange={(e) => onChange(e.target.value)}
            placeholder={field.placeholder}
            disabled={field.disabled || field.readonly}
            readOnly={field.readonly}
            rows={3}
            aria-invalid={!!error}
            aria-describedby={error ? `${field.id}-error` : undefined}
          />
        )}
      </div>

      {/* === Error Message === */}
      {error && (
        <div id={`${field.id}-error`} className="property-field__error">
          {error}
        </div>
      )}
    </div>
  );
};

// ============ Helper Functions ============

function colorToHex(color: PropertyValue): string {
  if (typeof color === 'string') return color;
  if (typeof color === 'object' && color && 'r' in color) {
    const c = color as Color;
    const toHex = (n: number) => Math.round(n).toString(16).padStart(2, '0');
    return `#${toHex(c.r)}${toHex(c.g)}${toHex(c.b)}`;
  }
  return '#000000';
}

function hexToColor(hex: string): Color {
  const result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
  return {
    r: result ? parseInt(result[1], 16) : 0,
    g: result ? parseInt(result[2], 16) : 0,
    b: result ? parseInt(result[3], 16) : 0,
  };
}

export default PropertiesPanel;
