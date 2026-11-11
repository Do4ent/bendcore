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
  type: 'line' | 'circle' | 'arc' | 'ellipse' | 'elliptical-arc' | 'bspline';
  start: number;
  end: number;
  center: number;
  radius: string;
  counterClockwise: boolean;
  construction: boolean;
  projection: number;
  /** ellipse, elliptical-arc: focus point (major axis direction); radius is the minor radius */
  focus?: number;
  /** bspline: pole point indices, degree, distinct knots, multiplicities, weights, periodic */
  poles?: number[];
  degree?: number;
  knots?: number[];
  multiplicities?: number[];
  weights?: number[];
  periodic?: boolean;
}
export interface SketchConstraintDef {
  type: string;
  a: number;
  b: number;
  c: number;
  value: string;
  /** reference (driven) dimension: measured, does not drive the sketch */
  driven?: boolean;
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
  /** 'fully-defined' | 'under-defined' | 'over-defined' */
  definition: string;
  /** redundant constraint numbers (1-based) */
  redundant: number[];
  /** per point index / per entity (id - 1): fully determined by the constraints */
  pointDefined: boolean[];
  entityDefined: boolean[];
  /** per constraint: dimension value (driven ones: measured) */
  constraintValues: number[];
  /** sketch frame in model space */
  placement: SketchPlacementInfo;
}

export interface SketchPlacementInfo {
  origin: Vec3;
  xDirection: Vec3;
  yDirection: Vec3;
  normal: Vec3;
}

/** Curve of a sketch entity at the solution (docs/ui/SKETCH_EDITOR.md). */
export interface SketchSolvedCurve {
  /** entity id (1-based) */
  entity: number;
  type: SketchGeometryDef['type'];
  construction: boolean;
  projection: number;
  defined: boolean;
  /** x0, y0, x1, y1, ... in sketch coordinates, sampled from the OCCT curve */
  polyline: Float64Array;
  /** middle of an open curve (parameter midpoint), null for closed ones */
  midpoint: [number, number] | null;
  center?: [number, number];
  radius?: number;
}

export interface SketchProfileIssue {
  kind: string;
  entities: number[];
  location: [number, number];
  detail: string;
}

export interface SketchSolvedRegion {
  index: number;
  /** entities of the outer loop, in traversal order */
  entities: number[];
  holes: number[][];
  area: number;
  depth: number;
  material: boolean;
  selected: boolean;
  /** entity id that selects exactly this region in SketchData.regions, or null */
  selector: number | null;
  /** triangulated face in sketch coordinates (x, y, 0) */
  positions: Float32Array;
  indices: Uint32Array;
}

export interface SketchSolution {
  converged: boolean;
  status: string;
  dof: number;
  /** 'fully-defined' | 'under-defined' | 'over-defined' */
  definition: string;
  /** constraint numbers (1-based, sketch data order) */
  conflicting: number[];
  redundant: number[];
  partiallyRedundant: number[];
  placement: SketchPlacementInfo;
  points: { x: number; y: number; fixed: boolean; defined: boolean }[];
  /** per constraint: dimension value (reference ones measured), other constraints their value */
  constraintValues: number[];
  /** per constraint: residual at the solution (0 for reference dimensions) */
  residuals: number[];
  curves: SketchSolvedCurve[];
  intersections: { x: number; y: number; a: number; b: number }[];
  /** null when the sketch did not solve exactly (conflict, no convergence) or with options.lite */
  profile: {
    hasProfile: boolean;
    issues: SketchProfileIssue[];
    regions: SketchSolvedRegion[];
    error: string | null;
  } | null;
  /** the input data with free points and initial radii at the solution */
  sketch: SketchData;
}

export type SketchTool =
  | { name: 'trim' | 'extend'; entity: number; pick: [number, number] }
  | { name: 'offset'; chain: number[]; distance: number; constrain?: boolean }
  | { name: 'mirror'; entities: number[]; axis: number; constrain?: boolean }
  | { name: 'linear-pattern'; entities: number[]; step: [number, number]; count: number; constrain?: boolean }
  | { name: 'circular-pattern'; entities: number[]; center: number; count: number; angle: number; constrain?: boolean }
  | { name: 'fillet'; a: number; b: number; radius: number }
  | { name: 'chamfer'; a: number; b: number; d1: number; d2: number };

export interface SketchEditOutcome {
  sketch: SketchData;
  /** entity ids in the new data */
  created: number[];
  modified: number[];
  /** entity ids of the input data */
  removed: number[];
  /** new point indices and constraint numbers in the new data */
  points: number[];
  constraints: number[];
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
  solveSketch(featureId: string, sketch: SketchData, options?: { drag?: { point: number; x: number; y: number }; lite?: boolean }): SketchSolution;
  editSketch(featureId: string, sketch: SketchData, tool: SketchTool): SketchEditOutcome;
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
