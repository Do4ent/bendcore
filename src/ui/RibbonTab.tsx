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
        role="tab"
        aria-selected={isActive}
        aria-controls={`ribbon-tabpanel-${id}`}
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
          id={`ribbon-tabpanel-${id}`}
          role="tabpanel"
          aria-labelledby={`ribbon-tab-${id}`}
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
  // WAI-ARIA tabs pattern: arrow keys / Home / End move between tabs.
  const handleTabKeyDown = (event: React.KeyboardEvent<HTMLButtonElement>, index: number) => {
    let nextIndex: number | null = null;
    if (event.key === "ArrowRight") nextIndex = (index + 1) % tabs.length;
    else if (event.key === "ArrowLeft") nextIndex = (index - 1 + tabs.length) % tabs.length;
    else if (event.key === "Home") nextIndex = 0;
    else if (event.key === "End") nextIndex = tabs.length - 1;
    if (nextIndex === null) return;

    event.preventDefault();
    const nextTab = tabs[nextIndex];
    onTabChange(nextTab.id);
    const tablist = event.currentTarget.parentElement;
    const nextButton = tablist?.querySelectorAll<HTMLButtonElement>('[role="tab"]')[nextIndex];
    nextButton?.focus();
  };

  return (
    <div className="border-b border-gray-300 bg-gray-100">
      {/* Tab Buttons Row */}
      <div className="flex gap-0 border-b border-gray-300" role="tablist" aria-label="Ribbon">
        {tabs.map((tab, index) => (
          <button
            key={tab.id}
            id={`ribbon-tab-${tab.id}`}
            role="tab"
            aria-selected={activeTab === tab.id}
            aria-controls={`ribbon-tabpanel-${tab.id}`}
            tabIndex={activeTab === tab.id ? 0 : -1}
            onClick={() => onTabChange(tab.id)}
            onKeyDown={(event) => handleTabKeyDown(event, index)}
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
            id={`ribbon-tabpanel-${tab.id}`}
            role="tabpanel"
            aria-labelledby={`ribbon-tab-${tab.id}`}
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
