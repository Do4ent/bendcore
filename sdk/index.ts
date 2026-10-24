/**
 * BendCore Plugin SDK
 * Main export file for plugin developers
 */

export {
  // Main SDK exports
  BendCorePlugin,
  SDK_VERSION,
  PluginRegistry,

  // Types and interfaces
  type PluginManifest,
  type PluginPermission,
  type PluginHook,
  type PluginUIComponent,
  type BendCoreAPI,
  type UIContext,
  type EventBus,
  type EventCallback,
  type PluginContext,
  type Logger,

  // Data types
  type Project,
  type Feature,
  type Mesh,
  type MeshNode,
  type MeshElement,
  type FEAAnalysisResult,
  type Material,
  type BoundingBox,
  type UserProfile,
  type UserSettings,

  // API types
  type ExportFormat,
  type CreateProjectData,
  type UpdateProjectData,
  type CreateFeatureData,
  type UpdateFeatureData,
  type FEAConfig,
  type GeometryData,
  type MeshConfig,
  type DialogOptions,
  type DialogResult,
  type FileDialogOptions,
  type ToolbarButton,
  type MenuItem,
  type PanelOptions,
} from './plugin-sdk';

// Re-export everything from plugin-sdk
export * from './plugin-sdk';
