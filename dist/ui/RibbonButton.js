import { jsx as _jsx, jsxs as _jsxs } from "react/jsx-runtime";
/**
 * RibbonButton Component
 * Single button in CommandManager ribbon with icon, label, and color category
 */
import React from "react";
import { getColorHex } from "./ColorScheme";
const RibbonButton = ({ id, label, icon, colorCategory, onClick, disabled = false, tooltip, showLabel = true, size = "medium", }) => {
    const bgColor = getColorHex(colorCategory);
    const sizeStyles = {
        small: "w-12 h-12",
        medium: "w-16 h-16",
        large: "w-20 h-20",
    };
    return (_jsxs("button", { id: `ribbon-btn-${id}`, onClick: () => !disabled && onClick(id), disabled: disabled, title: tooltip || label, className: `
        ${sizeStyles[size]}
        flex flex-col items-center justify-center
        rounded border border-gray-300
        hover:bg-opacity-90 transition-all
        disabled:opacity-50 disabled:cursor-not-allowed
        focus:outline-2 focus:outline-offset-2 focus:outline-blue-500
      `, style: {
            backgroundColor: disabled ? "#e0e0e0" : bgColor + "20", // Light tint
            borderColor: bgColor,
        }, children: [icon && _jsx("div", { className: "mb-1", children: icon }), showLabel && (_jsx("span", { className: "text-xs font-semibold text-gray-700 text-center truncate px-1", children: label }))] }));
};
export default RibbonButton;
export const RibbonButtonGroup = ({ title, children, columns = 4, }) => (_jsxs("div", { className: "border-r border-gray-400 px-2 py-1", children: [title && (_jsx("p", { className: "text-xs font-bold text-gray-600 mb-1 text-center", children: title })), _jsx("div", { className: "flex flex-wrap gap-1", style: {
                gridTemplateColumns: `repeat(${columns}, minmax(0, 1fr))`,
            }, children: children })] }));
//# sourceMappingURL=RibbonButton.js.map