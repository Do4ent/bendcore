/**
 * BendCore UI Component: Badge
 * Status indicator with variants
 *
 * @component
 * @example
 * <Badge variant="success">Active</Badge>
 * <Badge variant="warning" dismissible onDismiss={handleDismiss}>
 *   Warning
 * </Badge>
 */

import React, { useState } from 'react';
import './Badge.css';

export interface BadgeProps {
  variant?: 'default' | 'success' | 'warning' | 'error' | 'info';
  children: React.ReactNode;
  dismissible?: boolean;
  onDismiss?: () => void;
  className?: string;
  isDarkMode?: boolean;
}

export const Badge: React.FC<BadgeProps> = ({
  variant = 'default',
  children,
  dismissible = false,
  onDismiss,
  className = '',
  isDarkMode = false,
}) => {
  const [isDismissed, setIsDismissed] = useState(false);

  const handleDismiss = () => {
    setIsDismissed(true);
    onDismiss?.();
  };

  if (isDismissed) {
    return null;
  }

  return (
    <span
      className={`badge badge--${variant} ${isDarkMode ? 'badge--dark' : ''} ${className}`}
      role="status"
    >
      <span className="badge__content">{children}</span>
      {dismissible && (
        <button
          className="badge__dismiss"
          onClick={handleDismiss}
          aria-label="Закрыть"
          title="Закрыть"
        >
          ✕
        </button>
      )}
    </span>
  );
};

export default Badge;
