/**
 * BendCore UI Component: Icon
 * Reusable icon component with size variants
 *
 * @component
 * @example
 * <Icon name="save" size="md" color="#0078d4" />
 */

import React from 'react';

interface IconProps {
  name: string;
  size?: 'xs' | 'sm' | 'md' | 'lg' | 'xl';
  color?: string;
  rotation?: number;
  className?: string;
  title?: string;
  alt?: string;
}

const ICON_MAP: Record<string, string> = {
  save: '💾',
  open: '📁',
  new: '📄',
  edit: '✏️',
  delete: '🗑️',
  undo: '↶',
  redo: '↷',
  copy: '📋',
  paste: '📌',
  cut: '✂️',
  search: '🔍',
  zoom_in: '🔍+',
  zoom_out: '🔍-',
  home: '🏠',
  settings: '⚙️',
  close: '✕',
  check: '✓',
  warning: '⚠️',
  error: '✕',
  info: 'ℹ️',
  menu: '☰',
  arrow_left: '←',
  arrow_right: '→',
  arrow_up: '↑',
  arrow_down: '↓',
  expand: '▶',
  collapse: '◀',
  pin: '📌',
  star: '⭐',
  heart: '❤️',
  fire: '🔥',
};

const SIZE_MAP: Record<string, string> = {
  xs: '12px',
  sm: '16px',
  md: '20px',
  lg: '24px',
  xl: '32px',
};

export const Icon: React.FC<IconProps> = ({
  name,
  size = 'md',
  color,
  rotation = 0,
  className = '',
  title,
  alt,
}) => {
  const iconChar = ICON_MAP[name] || name;
  const fontSize = SIZE_MAP[size];

  return (
    <span
      className={`icon ${className}`}
      style={{
        display: 'inline-flex',
        alignItems: 'center',
        justifyContent: 'center',
        fontSize: fontSize,
        color: color,
        transform: rotation ? `rotate(${rotation}deg)` : undefined,
        lineHeight: 1,
        userSelect: 'none',
      }}
      title={title}
      aria-label={alt || name}
      role="img"
    >
      {iconChar}
    </span>
  );
};

export default Icon;
