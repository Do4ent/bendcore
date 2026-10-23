/**
 * CommandManager RibbonBar
 * Main ribbon interface for BendCore CAD with 5 context-sensitive tabs
 * Reference: BENDCORE_SPECIFICATION_FINAL_LOCKED.md Section 6
 */
import React from "react";
export interface CommandHandler {
    (commandId: string, args?: Record<string, any>): Promise<void> | void;
}
export interface RibbonBarProps {
    onCommand: CommandHandler;
    activeDocument?: {
        type: "part" | "assembly" | "drawing" | null;
    };
}
declare const RibbonBar: React.FC<RibbonBarProps>;
export default RibbonBar;
//# sourceMappingURL=RibbonBar.d.ts.map