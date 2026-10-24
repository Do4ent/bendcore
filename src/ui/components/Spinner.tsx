/**
 * BendCore UI Component: Spinner
 * Loading animation
 *
 * @component
 * @example
 * <Spinner size="md" color="#0078d4" />
 */

import React from 'react';
import './Spinner.css';

interface SpinnerProps {
  size?: 'sm' | 'md' | 'lg';
  color?: string;
  isDarkMode?: boolean;
  label?: string;
}

const SIZE_MAP: Record<string, string> = {
  sm: '16px',
  md: '24px',
  lg: '32px',
};

export const Spinner: React.FC<SpinnerProps> = ({
  size = 'md',
  color = '#0078d4',
  isDarkMode = false,
  label = 'Загрузка...',
}) => {
  const dimension = SIZE_MAP[size];

  return (
    <div
      className={`spinner ${isDarkMode ? 'spinner--dark' : ''}`}
      role="status"
      aria-label={label}
    >
      <svg
        className="spinner__svg"
        width={dimension}
        height={dimension}
        viewBox="0 0 24 24"
        fill="none"
        xmlns="http://www.w3.org/2000/svg"
      >
        <circle
          className="spinner__circle-bg"
          cx="12"
          cy="12"
          r="10"
          stroke="currentColor"
          strokeWidth="2"
          opacity="0.2"
        />
        <circle
          className="spinner__circle-progress"
          cx="12"
          cy="12"
          r="10"
          stroke={color}
          strokeWidth="2"
          strokeDasharray="15.7"
          strokeDashoffset="0"
          strokeLinecap="round"
        />
      </svg>
    </div>
  );
};

export default Spinner;
