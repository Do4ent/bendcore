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
    children: ReactNode;
}
declare const RibbonTab: React.FC<RibbonTabProps>;
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
export declare const RibbonTabBar: React.FC<RibbonTabBarProps>;
//# sourceMappingURL=RibbonTab.d.ts.map