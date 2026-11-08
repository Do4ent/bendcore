// Types of bendcore_modeling_api.mjs (docs/ui/PART_MODELING_UI.md).

import { KernelError, KernelMesh, TessellationParams, Vec3, faceOfTriangle } from './bendcore_kernel_api.mjs';

export { KernelError, faceOfTriangle };

export type ElementKind = 'face' | 'edge' | 'vertex';
export type FeatureState = 'ok' | 'warning' | 'error' | 'suppressed' | 'rolled-back';
/** "final" | "before:<id>" | "after:<id>" | "sketch:<id>" */
export type ShapeTarget = string;

export interface ParameterSpec {
  name: string;
  defaultExpression: string;
}
export interface ReferenceSlotSpec {
  name: string;
  kind: ElementKind;
  multiple: boolean;
  optional: boolean;
}
export interface OptionSpec {
  name: string;
  /** First value is the default. */
  allowed: string[];
}
export interface LinkSlotSpec {
  name: string;
  /** Accepted feature types; empty = any. */
  types: string[];
  optional: boolean;
}
export interface FeatureTypeSpec {
  type: string;
  parameters: ParameterSpec[];
  references: ReferenceSlotSpec[];
  options: OptionSpec[];
  links: LinkSlotSpec[];
  hasSketch: boolean;
}

export interface SketchPointDef {
  x: string;
  y: string;
  fixed: boolean;
}
export interface SketchGeometryDef {
  type: 'line' | 'circle' | 'arc';
  start: number;
  end: number;
  center: number;
  radius: string;
  counterClockwise: boolean;
  construction: boolean;
  projection: number;
}
export interface SketchConstraintDef {
  type: string;
  a: number;
  b: number;
  c: number;
  value: string;
}
export interface SketchData {
  points: SketchPointDef[];
  geometry: SketchGeometryDef[];
  constraints: SketchConstraintDef[];
  regions: number[];
}

export type Expr = string | number;
export type SketchProfileTemplate =
  | { kind: 'rectangle'; x: Expr; y: Expr; width: Expr; height: Expr }
  | { kind: 'circle'; cx: Expr; cy: Expr; radius: Expr }
  | { kind: 'polygon'; points: [Expr, Expr][] }
  | { kind: 'point'; x: Expr; y: Expr };

export interface GeometricSignature {
  valid: boolean;
  kind: ElementKind;
  geometry: string;
  center: Vec3;
  hasDirection: boolean;
  direction: Vec3;
  radius: number;
  size: number;
}
export interface TopoReference {
  name: string;
  kind: ElementKind;
  signature: GeometricSignature;
}

export interface ElementRecord {
  index: number;
  kind: ElementKind;
  name: string;
  origin: string;
  feature: string;
  aliases: string[];
  signature: GeometricSignature;
}

export interface ReferenceReport {
  slot: string;
  name: string;
  kind: ElementKind;
  method: 'exact' | 'history' | 'signature' | 'broken' | string;
  names: string[];
  detail: string;
}

export interface SketchResultInfo {
  dof: number;
  solverStatus: string;
  curves: number;
  regions: number;
  standalonePoints: number;
  hasFaces: boolean;
}

export interface FeatureView {
  index: number;
  id: string;
  type: string;
  label: string;
  suppressed: boolean;
  state: FeatureState;
  messages: string[];
  parameters: Record<string, string>;
  values: Record<string, number>;
  options: Record<string, string>;
  links: Record<string, string>;
  references: Record<string, TopoReference[]>;
  referenceReports: ReferenceReport[];
  hasBody: boolean;
  sketch?: SketchData;
  sketchResult?: SketchResultInfo;
  datum?: { planeOrigin?: Vec3; planeNormal?: Vec3; axisOrigin?: Vec3; axisDirection?: Vec3 };
}

export interface FeatureSpecInput {
  type: string;
  id?: string;
  label?: string;
  parameters?: Record<string, Expr>;
  options?: Record<string, string>;
  links?: Record<string, string>;
  references?: Record<string, TopoReference[]>;
  sketch?: SketchData;
  suppressed?: boolean;
}

export interface FeatureEdits {
  label?: string;
  parameters?: Record<string, Expr>;
  options?: Record<string, string>;
  links?: Record<string, string>;
  references?: Record<string, TopoReference[]>;
  sketch?: SketchData;
  suppressed?: boolean;
}

export interface RebuildReport {
  recomputed: string[];
  reused: string[];
  fromCache: string[];
  failed: string[];
}

export interface TransactionInfo {
  name: string;
  edits: number;
}

export interface DocumentState {
  size: number;
  rollback: number | null;
  canUndo: boolean;
  canRedo: boolean;
  inTransaction: boolean;
  undoHistory: TransactionInfo[];
  redoHistory: TransactionInfo[];
}

export interface VariableView {
  name: string;
  expression: string;
  value: number | null;
  error: string | null;
}

export interface PartProperties {
  counts: { solids: number; faces: number; edges: number; vertices: number };
  volume: number | null;
  centerOfMass: Vec3 | null;
  /** kg, when a density was given and the target has solids. */
  mass: number | null;
  area: number | null;
  boundingBox: { min: Vec3; max: Vec3 } | null;
}

export class PartDocumentHandle {
  readonly handle: number;
  readonly disposed: boolean;
  dispose(): void;
  save(options?: { includeCache?: boolean }): string;
  variables(): VariableView[];
  setVariable(name: string, expression: Expr): void;
  removeVariable(name: string): void;
  features(): FeatureView[];
  feature(id: string): FeatureView;
  state(): DocumentState;
  addFeature(spec: FeatureSpecInput, position?: number | null): string;
  editFeature(id: string, edits: FeatureEdits): void;
  removeFeature(id: string): void;
  moveFeature(id: string, index: number): void;
  setSuppressed(id: string, suppressed: boolean): void;
  setRollback(position: number | null): void;
  beginTransaction(name: string): void;
  commitTransaction(): void;
  abortTransaction(): void;
  undo(): RebuildReport;
  redo(): RebuildReport;
  rebuild(): RebuildReport;
  makeReference(target: ShapeTarget, kind: ElementKind, index: number): TopoReference;
  element(target: ShapeTarget, kind: ElementKind, index: number): ElementRecord;
  tessellate(target?: ShapeTarget, params?: TessellationParams): KernelMesh;
  properties(target?: ShapeTarget, density?: number | null): PartProperties;
}

export interface ModelingInfo {
  occtVersion: string;
  liveDocuments: number;
  formatVersion: number;
}

export class ModelingKernel {
  info(): ModelingInfo;
  catalog(): FeatureTypeSpec[];
  sketchTemplate(spec: { profiles: SketchProfileTemplate[] }): SketchData;
  createDocument(): PartDocumentHandle;
  loadDocument(json: string): { document: PartDocumentHandle; report: RebuildReport };
}

export type ModelingModuleFactory = (options?: Record<string, unknown>) => Promise<unknown>;

export function loadModeling(factory: ModelingModuleFactory, moduleOptions?: Record<string, unknown>): Promise<ModelingKernel>;
