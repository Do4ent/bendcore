/**
 * RibbonTab Component
 * Represents a single tab in the CommandManager ribbon (Home, Part, Assembly, Analysis, Drawing)
 */

import React, { ReactNode } from "react";

export interface RibbonTabProps {
  id: string;
  name: string;
  isActive: boolean;
  onClick: (tabId: string) => void;
  children: ReactNode; // RibbonButtonGroups
}

const RibbonTab: React.FC<RibbonTabProps> = ({
  id,
  name,
  isActive,
  onClick,
  children,
}) => {
  return (
    <div>
      {/* Tab Button */}
      <button
        id={`ribbon-tab-${id}`}
        onClick={() => onClick(id)}
        className={`
          px-4 py-2 font-semibold text-sm
          border-b-2 transition-colors
          ${
            isActive
              ? "border-blue-600 text-blue-600 bg-blue-50"
              : "border-transparent text-gray-700 bg-gray-100 hover:bg-gray-200"
          }
        `}
      >
        {name}
      </button>

      {/* Tab Content */}
      {isActive && (
        <div
          className="bg-white border-b border-gray-300 flex overflow-x-auto"
          style={{ minHeight: "80px" }}
        >
          {children}
        </div>
      )}
    </div>
  );
};

export default RibbonTab;

/**
 * RibbonTabBar - Container for all ribbon tabs
 */
export interface RibbonTabBarProps {
  activeTab: string;
  onTabChange: (tabId: string) => void;
  tabs: {
    id: string;
    name: string;
    content: ReactNode;
  }[];
}

export const RibbonTabBar: React.FC<RibbonTabBarProps> = ({
  activeTab,
  onTabChange,
  tabs,
}) => {
  return (
    <div className="border-b border-gray-300 bg-gray-100">
      {/* Tab Buttons Row */}
      <div className="flex gap-0 border-b border-gray-300">
        {tabs.map((tab) => (
          <button
            key={tab.id}
            onClick={() => onTabChange(tab.id)}
            className={`
              px-4 py-2 font-semibold text-sm
              border-b-2 transition-colors
              ${
                activeTab === tab.id
                  ? "border-blue-600 text-blue-600 bg-white"
                  : "border-transparent text-gray-700 bg-gray-100 hover:bg-gray-200"
              }
            `}
          >
            {tab.name}
          </button>
        ))}
      </div>

      {/* Content Row */}
      {tabs.map((tab) => (
        activeTab === tab.id && (
          <div
            key={`content-${tab.id}`}
            className="bg-white border-b border-gray-300 flex overflow-x-auto"
            style={{ minHeight: "100px" }}
          >
            {tab.content}
          </div>
        )
      ))}
    </div>
  );
};
