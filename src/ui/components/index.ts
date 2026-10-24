/**
 * BendCore UI Components
 * Export all 10 core UI components
 *
 * Components:
 * 1. RibbonBar - Top toolbar with tabs and commands
 * 2. TreePanel - Feature tree with expand/collapse
 * 3. PropertiesPanel - Object properties editor
 * 4. StatusBar - Status information display
 * 5. ContextMenu - Right-click context menu
 * 6. Tooltip - Hover tooltips
 * 7. Icon - Icon component
 * 8. Badge - Status badges
 * 9. Spinner - Loading spinner
 * 10. Divider - Visual dividers
 */

// 1. RibbonBar
export { RibbonBar } from './RibbonBar';
export type { RibbonCommand, RibbonTab, RibbonBarProps } from './RibbonBar';

// 2. TreePanel
export { TreePanel } from './TreePanel';
export type { TreeNode, TreePanelProps } from './TreePanel';

// 3. PropertiesPanel
export { PropertiesPanel } from './PropertiesPanel';
export type { PropertyField, PropertyValue, PropertiesPanelProps } from './PropertiesPanel';

// 4. StatusBar
export { StatusBar } from './StatusBar';
export type { StatusBarProps } from './StatusBar';

// 5. ContextMenu
export { ContextMenu } from './ContextMenu';
export type { ContextMenuItem, ContextMenuProps } from './ContextMenu';

// 6. Tooltip
export { Tooltip } from './Tooltip';
export type { TooltipProps } from './Tooltip';

// 7. Icon
export { Icon } from './Icon';
export type { IconProps } from './Icon';

// 8. Badge
export { Badge } from './Badge';
export type { BadgeProps } from './Badge';

// 9. Spinner
export { Spinner } from './Spinner';
export type { SpinnerProps } from './Spinner';

// 10. Divider
export { Divider } from './Divider';
export type { DividerProps } from './Divider';
