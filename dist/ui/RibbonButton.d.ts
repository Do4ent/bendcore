/**
 * RibbonButton Component
 * Single button in CommandManager ribbon with icon, label, and color category
 */
import React, { ReactNode } from "react";
export interface RibbonButtonProps {
    id: string;
    label: string;
    icon?: ReactNode;
    colorCategory: string;
    onClick: (buttonId: string) => void;
    disabled?: boolean;
    tooltip?: string;
    showLabel?: boolean;
    size?: "small" | "medium" | "large";
}
declare const RibbonButton: React.FC<RibbonButtonProps>;
export default RibbonButton;
/**
 * RibbonButtonGroup - Container for organizing buttons
 */
export interface RibbonButtonGroupProps {
    title?: string;
    children: ReactNode;
    columns?: number;
}
export declare const RibbonButtonGroup: React.FC<RibbonButtonGroupProps>;
//# sourceMappingURL=RibbonButton.d.ts.map