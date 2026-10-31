/**
 * RibbonButton Component
 * Single button in CommandManager ribbon with icon, label, and color category
 */

import React, { ReactNode } from "react";
import { getColorHex } from "./ColorScheme";
import { getShortcutForCommand, formatShortcutForDisplay } from "./KeyboardShortcuts";

export interface RibbonButtonProps {
  id: string;
  label: string;
  icon?: ReactNode; // SVG or React component
  colorCategory: string; // Key from ColorScheme
  onClick: (buttonId: string) => void;
  disabled?: boolean;
  tooltip?: string;
  showLabel?: boolean;
  size?: "small" | "medium" | "large";
}

const RibbonButton: React.FC<RibbonButtonProps> = ({
  id,
  label,
  icon,
  colorCategory,
  onClick,
  disabled = false,
  tooltip,
  showLabel = true,
  size = "medium",
}) => {
  const bgColor = getColorHex(colorCategory);
  const shortcut = formatShortcutForDisplay(id);
  // Only set a native tooltip when it adds information beyond the visible label:
  // an explicit tooltip, a keyboard shortcut, or the label itself when hidden.
  const tooltipText = tooltip || (shortcut || !showLabel ? label : undefined);
  const fullTooltip =
    tooltipText && shortcut ? `${tooltipText} (${shortcut})` : tooltipText;

  const sizeStyles = {
    small: "w-12 h-12",
    medium: "w-16 h-16",
    large: "w-20 h-20",
  };

  return (
    <button
      id={`ribbon-btn-${id}`}
      onClick={() => !disabled && onClick(id)}
      disabled={disabled}
      title={fullTooltip}
      aria-label={showLabel ? undefined : label}
      className={`
        ${sizeStyles[size]}
        flex flex-col items-center justify-center
        rounded border border-gray-300
        hover:bg-opacity-90 transition-all
        disabled:opacity-50 disabled:cursor-not-allowed
        focus:outline-2 focus:outline-offset-2 focus:outline-blue-500
      `}
      style={{
        backgroundColor: disabled ? "#e0e0e0" : bgColor + "20", // Light tint
        borderColor: bgColor,
      }}
    >
      {icon && <div className="mb-1">{icon}</div>}
      {showLabel && (
        <span className="text-xs font-semibold text-gray-700 text-center truncate px-1">
          {label}
        </span>
      )}
    </button>
  );
};

export default RibbonButton;

/**
 * RibbonButtonGroup - Container for organizing buttons
 */
export interface RibbonButtonGroupProps {
  title?: string;
  children: ReactNode;
  columns?: number;
}

export const RibbonButtonGroup: React.FC<RibbonButtonGroupProps> = ({
  title,
  children,
  columns = 4,
}) => (
  <div className="border-r border-gray-400 px-2 py-1">
    {title && (
      <p className="text-xs font-bold text-gray-600 mb-1 text-center">
        {title}
      </p>
    )}
    <div
      className="flex flex-wrap gap-1"
      style={{
        gridTemplateColumns: `repeat(${columns}, minmax(0, 1fr))`,
      }}
    >
      {children}
    </div>
  </div>
);
