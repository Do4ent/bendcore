/**
 * BendCore Plugin SDK v1.0
 * Developer toolkit for extending BendCore with custom features
 *
 * Usage:
 *   import { BendCorePlugin, BendCoreAPI } from '@bendcore/sdk';
 *
 *   export class MyPlugin extends BendCorePlugin {
 *     async onLoad(context) { ... }
 *     async onUnload() { ... }
 *   }
 */

/**
 * Plugin Manifest - describes the plugin
 */
export interface PluginManifest {
  id: string; // unique identifier: 'bendcore.plugin-name'
  name: string; // display name
  version: string; // semver
  author: string; // plugin author
  description: string; // short description
  homepage?: string; // plugin website
  repository?: string; // git repo
  license?: string; // license type
  permissions: PluginPermission[]; // required permissions
  hooks?: PluginHook[]; // hooks this plugin provides/uses
  uiComponents?: PluginUIComponent[]; // UI elements
}

/**
 * Plugin permissions
 */
export type PluginPermission =
  | 'read:projects'
  | 'write:projects'
  | 'read:geometry'
  | 'write:geometry'
  | 'read:materials'
  | 'write:materials'
  | 'read:fea'
  | 'write:fea'
  | 'read:users'
  | 'network:http'
  | 'filesystem:read'
  | 'filesystem:write'
  | 'clipboard:read'
  | 'clipboard:write';

/**
 * Plugin lifecycle hooks
 */
export type PluginHook =
  | 'on:load'
  | 'on:unload'
  | 'on:beforeExport'
  | 'on:afterExport'
  | 'on:beforeAnalysis'
  | 'on:afterAnalysis'
  | 'on:fileOpen'
  | 'on:fileSave'
  | 'on:menuClick'
  | 'on:commandExecute';

/**
 * UI Component registration
 */
export interface PluginUIComponent {
  id: string;
  type: 'panel' | 'toolbar' | 'menu' | 'modal' | 'contextMenu';
  label: string;
  icon?: string;
  path: string; // React component path
  position?: 'left' | 'right' | 'top' | 'bottom';
}

/**
 * API for plugin to interact with BendCore
 */
export interface BendCoreAPI {
  // Project management
  projects: {
    list(): Promise<Project[]>;
    get(id: string): Promise<Project>;
    create(data: CreateProjectData): Promise<Project>;
    update(id: string, data: UpdateProjectData): Promise<Project>;
    delete(id: string): Promise<void>;
    export(id: string, format: ExportFormat): Promise<Blob>;
    import(file: File): Promise<Project>;
  };

  // Feature management
  features: {
    list(projectId: string): Promise<Feature[]>;
    get(projectId: string, featureId: string): Promise<Feature>;
    create(projectId: string, data: CreateFeatureData): Promise<Feature>;
    update(featureId: string, data: UpdateFeatureData): Promise<Feature>;
    delete(featureId: string): Promise<void>;
  };

  // FEA Analysis
  fea: {
    analyze(projectId: string, config: FEAConfig): Promise<FEAAnalysisResult>;
    getMaterials(): Promise<Material[]>;
    addMaterial(material: Material): Promise<void>;
  };

  // Geometry operations
  geometry: {
    createMesh(geometry: GeometryData, config: MeshConfig): Promise<Mesh>;
    export(projectId: string, format: ExportFormat): Promise<Blob>;
    import(data: ArrayBuffer, format: string): Promise<GeometryData>;
    getBoundingBox(geometry: GeometryData): Promise<BoundingBox>;
  };

  // User and workspace
  user: {
    getProfile(): Promise<UserProfile>;
    getSettings(): Promise<UserSettings>;
    updateSettings(settings: Partial<UserSettings>): Promise<void>;
  };

  // Storage
  storage: {
    get(key: string): Promise<any>;
    set(key: string, value: any): Promise<void>;
    delete(key: string): Promise<void>;
    list(prefix?: string): Promise<string[]>;
  };
}

/**
 * UI Context - UI operations for plugins
 */
export interface UIContext {
  // Notifications
  showNotification(
    message: string,
    type: 'info' | 'warning' | 'error' | 'success',
    duration?: number
  ): void;

  // Dialogs
  showDialog(options: DialogOptions): Promise<DialogResult>;
  showInputDialog(prompt: string, defaultValue?: string): Promise<string | null>;
  showConfirmDialog(message: string): Promise<boolean>;
  showFileDialog(options: FileDialogOptions): Promise<File[]>;

  // Panels and menus
  addToolbarButton(button: ToolbarButton): void;
  removeToolbarButton(id: string): void;
  addMenuItem(item: MenuItem): void;
  removeMenuItem(id: string): void;

  // Panels
  openPanel(options: PanelOptions): void;
  closePanel(id: string): void;
  showSidebar(title: string, component: React.ReactNode): void;
  closeSidebar(): void;

  // Progress
  showProgress(title: string, currentValue?: number, maxValue?: number): void;
  updateProgress(currentValue: number): void;
  hideProgress(): void;
}

/**
 * Event system
 */
export interface EventBus {
  on(event: string, callback: EventCallback): void;
  off(event: string, callback: EventCallback): void;
  once(event: string, callback: EventCallback): void;
  emit(event: string, data?: any): void;
}

export type EventCallback = (data: any) => void | Promise<void>;

/**
 * Plugin context - everything provided to plugin
 */
export interface PluginContext {
  bendcore: BendCoreAPI;
  ui: UIContext;
  events: EventBus;
  logger: Logger;
  version: string;
  isDevelopment: boolean;
}

/**
 * Logger interface
 */
export interface Logger {
  log(message: string, ...args: any[]): void;
  warn(message: string, ...args: any[]): void;
  error(message: string, ...args: any[]): void;
  debug(message: string, ...args: any[]): void;
}

/**
 * Data type definitions
 */

export interface Project {
  id: string;
  name: string;
  description?: string;
  createdAt: Date;
  updatedAt: Date;
  ownerId: string;
  collaborators?: string[];
  features: Feature[];
  metadata?: Record<string, any>;
}

export interface Feature {
  id: string;
  projectId: string;
  name: string;
  type: string; // 'pipe', 'bend', 'feature', etc.
  geometry?: GeometryData;
  properties?: Record<string, any>;
  createdAt: Date;
  updatedAt: Date;
}

export interface Mesh {
  nodes: MeshNode[];
  elements: MeshElement[];
  metadata?: Record<string, any>;
}

export interface MeshNode {
  id: number;
  x: number;
  y: number;
  z: number;
}

export interface MeshElement {
  id: number;
  nodes: number[];
  type: string; // 'tet4', 'tri3', etc.
}

export interface FEAAnalysisResult {
  displacement: number[];
  stress: number[];
  strain: number[];
  maxStress: number;
  maxDisplacement: number;
  safetyFactor: number;
  convergence: boolean;
  executionTime: number;
}

export interface Material {
  id: string;
  name: string;
  elasticModulus: number; // Pa
  poissonsRatio: number;
  density: number; // kg/m³
  yieldStrength: number; // Pa
  color?: string;
}

export interface BoundingBox {
  min: { x: number; y: number; z: number };
  max: { x: number; y: number; z: number };
}

export interface UserProfile {
  id: string;
  email: string;
  name: string;
  avatar?: string;
}

export interface UserSettings {
  theme: 'light' | 'dark';
  units: 'mm' | 'cm' | 'inches';
  language: string;
  autoSave: boolean;
}

// Type definitions for complex parameters

export type ExportFormat = 'step' | 'iges' | 'stl' | 'dxf' | 'pdf' | '3dpdf';

export interface CreateProjectData {
  name: string;
  description?: string;
}

export interface UpdateProjectData {
  name?: string;
  description?: string;
}

export interface CreateFeatureData {
  name: string;
  type: string;
  geometry?: GeometryData;
  properties?: Record<string, any>;
}

export interface UpdateFeatureData {
  name?: string;
  geometry?: GeometryData;
  properties?: Record<string, any>;
}

export interface FEAConfig {
  material: string; // material ID or name
  meshSize: number; // mm
  solver: 'direct' | 'iterative';
}

export interface GeometryData {
  type: 'tube' | 'custom' | 'imported';
  dimensions?: Record<string, number>;
  mesh?: Mesh;
}

export interface MeshConfig {
  size: number; // mm
  quality: 'coarse' | 'medium' | 'fine';
}

export interface DialogOptions {
  title: string;
  message: string;
  buttons: { label: string; value: any }[];
}

export interface DialogResult {
  button: number;
  value: any;
}

export interface FileDialogOptions {
  title?: string;
  filters?: { name: string; extensions: string[] }[];
  multiple?: boolean;
}

export interface ToolbarButton {
  id: string;
  label: string;
  icon?: string;
  tooltip?: string;
  onClick: () => void | Promise<void>;
}

export interface MenuItem {
  id: string;
  label: string;
  menu: string; // 'File', 'Edit', 'View', etc.
  icon?: string;
  shortcut?: string;
  onClick: () => void | Promise<void>;
  submenu?: MenuItem[];
}

export interface PanelOptions {
  id: string;
  title: string;
  component: React.ReactNode;
  position?: 'left' | 'right' | 'bottom';
  width?: number;
  height?: number;
  closeable?: boolean;
}

/**
 * Base Plugin class - extend this to create a plugin
 */
export abstract class BendCorePlugin {
  protected manifest: PluginManifest;
  protected context: PluginContext | null = null;

  constructor(manifest: PluginManifest) {
    this.manifest = manifest;
  }

  /**
   * Called when plugin is loaded
   */
  abstract onLoad(context: PluginContext): Promise<void>;

  /**
   * Called when plugin is unloaded
   */
  abstract onUnload(): Promise<void>;

  /**
   * Get plugin manifest
   */
  getManifest(): PluginManifest {
    return this.manifest;
  }

  /**
   * Get plugin context (after onLoad is called)
   */
  protected getContext(): PluginContext {
    if (!this.context) {
      throw new Error('Plugin context not available. Make sure onLoad was called.');
    }
    return this.context;
  }

  /**
   * Logging helper
   */
  protected log(message: string, ...args: any[]): void {
    const ctx = this.context;
    if (ctx) {
      ctx.logger.log(`[${this.manifest.name}] ${message}`, ...args);
    }
  }

  protected logError(message: string, ...args: any[]): void {
    const ctx = this.context;
    if (ctx) {
      ctx.logger.error(`[${this.manifest.name}] ${message}`, ...args);
    }
  }

  protected logWarn(message: string, ...args: any[]): void {
    const ctx = this.context;
    if (ctx) {
      ctx.logger.warn(`[${this.manifest.name}] ${message}`, ...args);
    }
  }
}

/**
 * Plugin registry and loader (internal, used by BendCore)
 */
export class PluginRegistry {
  private plugins: Map<string, BendCorePlugin> = new Map();

  register(plugin: BendCorePlugin): void {
    const manifest = plugin.getManifest();
    this.plugins.set(manifest.id, plugin);
  }

  get(id: string): BendCorePlugin | undefined {
    return this.plugins.get(id);
  }

  getAll(): BendCorePlugin[] {
    return Array.from(this.plugins.values());
  }

  unregister(id: string): void {
    this.plugins.delete(id);
  }
}

/**
 * Export SDK version
 */
export const SDK_VERSION = '1.0.0';
