/**
 * BendCore UI Components
 * Export all 14 core UI components (10 existing + 4 Phase G)
 *
 * Core Components (1-10):
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
 *
 * Phase G Components (11-14):
 * 11. ThreeJsRenderer - 3D viewport with Three.js and LOD support (CAD-CORE-025)
 * 12. ParametricEditor - Parametric editor UI with dependency resolution (CAD-CORE-026)
 * 13. PropertyPanel - Properties and parameters panel with real-time updates (CAD-CORE-027)
 * 14. ViewportControls - 3D viewport camera and interaction controls (CAD-CORE-028)
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

// Phase G Components

// 11. ThreeJsRenderer (CAD-CORE-025)
export { ThreeJsRenderer } from './ThreeJsRenderer';
export type { ThreeJsRendererProps } from './ThreeJsRenderer';

// 12. ParametricEditor (CAD-CORE-026)
export { ParametricEditor } from './ParametricEditor';
export type {
  ParameterDefinition,
  ParametricEditorProps,
} from './ParametricEditor';

// 13. PropertyPanel (CAD-CORE-027)
export { PropertyPanel } from './PropertyPanel';
export type { Property, PropertyPanelProps } from './PropertyPanel';

// 14. ViewportControls (CAD-CORE-028)
export { ViewportControls } from './ViewportControls';
export type { ViewportControlsConfig, CameraState } from './ViewportControls';
