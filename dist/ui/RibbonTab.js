import { jsx as _jsx, jsxs as _jsxs } from "react/jsx-runtime";
/**
 * RibbonTab Component
 * Represents a single tab in the CommandManager ribbon (Home, Part, Assembly, Analysis, Drawing)
 */
import React from "react";
const RibbonTab = ({ id, name, isActive, onClick, children, }) => {
    return (_jsxs("div", { children: [_jsx("button", { id: `ribbon-tab-${id}`, onClick: () => onClick(id), className: `
          px-4 py-2 font-semibold text-sm
          border-b-2 transition-colors
          ${isActive
                    ? "border-blue-600 text-blue-600 bg-blue-50"
                    : "border-transparent text-gray-700 bg-gray-100 hover:bg-gray-200"}
        `, children: name }), isActive && (_jsx("div", { className: "bg-white border-b border-gray-300 flex overflow-x-auto", style: { minHeight: "80px" }, children: children }))] }));
};
export default RibbonTab;
export const RibbonTabBar = ({ activeTab, onTabChange, tabs, }) => {
    return (_jsxs("div", { className: "border-b border-gray-300 bg-gray-100", children: [_jsx("div", { className: "flex gap-0 border-b border-gray-300", children: tabs.map((tab) => (_jsx("button", { onClick: () => onTabChange(tab.id), className: `
              px-4 py-2 font-semibold text-sm
              border-b-2 transition-colors
              ${activeTab === tab.id
                        ? "border-blue-600 text-blue-600 bg-white"
                        : "border-transparent text-gray-700 bg-gray-100 hover:bg-gray-200"}
            `, children: tab.name }, tab.id))) }), tabs.map((tab) => (activeTab === tab.id && (_jsx("div", { className: "bg-white border-b border-gray-300 flex overflow-x-auto", style: { minHeight: "100px" }, children: tab.content }, `content-${tab.id}`))))] }));
};
//# sourceMappingURL=RibbonTab.js.map