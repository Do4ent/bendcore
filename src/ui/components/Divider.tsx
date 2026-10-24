/**
 * BendCore UI Component: Divider
 * Separator line (horizontal or vertical)
 *
 * @component
 * @example
 * <Divider orientation="horizontal" label="Section" />
 * <Divider orientation="vertical" />
 */

import React from 'react';
import './Divider.css';

export interface DividerProps {
  orientation?: 'horizontal' | 'vertical';
  label?: string;
  color?: string;
  thickness?: number;
  isDarkMode?: boolean;
  className?: string;
}

export const Divider: React.FC<DividerProps> = ({
  orientation = 'horizontal',
  label,
  color,
  thickness = 1,
  isDarkMode = false,
  className = '',
}) => {
  const baseClass = `divider divider--${orientation}`;
  const classes = `${baseClass} ${isDarkMode ? 'divider--dark' : ''} ${className}`;

  if (orientation === 'vertical') {
    return (
      <div
        className={classes}
        style={{
          borderLeftWidth: thickness,
          borderLeftColor: color,
        }}
        role="separator"
        aria-orientation="vertical"
      />
    );
  }

  return (
    <div
      className={classes}
      style={{
        borderTopWidth: thickness,
        borderTopColor: color,
      }}
      role="separator"
      aria-orientation="horizontal"
    >
      {label && (
        <div className="divider__label">
          <span className="divider__label-text">{label}</span>
        </div>
      )}
    </div>
  );
};

export default Divider;
