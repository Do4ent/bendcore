// Types of bendcore_assembly_api.mjs (docs/ui/ASSEMBLY_UI.md).

import { KernelError, faceOfTriangle } from './bendcore_kernel_api.mjs';
import type { KernelMesh, Vec3 } from './bendcore_kernel_api.mjs';

export { KernelError, faceOfTriangle };
export type { KernelMesh, Vec3 };

export type ElementKind = 'face' | 'edge' | 'vertex';
export type EntityUse = 'auto' | 'axis' | 'point';
export type MateType =
  | 'coincident'
  | 'concentric'
  | 'distance'
  | 'angle'
  | 'parallel'
  | 'perpendicular'
  | 'tangent'
  | 'rigid';
export type Alignment = 'closest' | 'aligned' | 'anti-aligned';
export type MateStatus = 'solved' | 'redundant' | 'conflicting' | 'unsolved' | 'broken' | 'suppressed';

/** Rigid placement; rotation is a unit quaternion [x, y, z, w]. matrix: column-major 4×4. */
export interface Placement {
  position: Vec3;
  rotation: [number, number, number, number];
  matrix: number[];
}

/** Input placement: { position, rotation } or { position, axis, angle (degrees) }. */
export interface PlacementInput {
  position?: Vec3;
  rotation?: [number, number, number, number];
  axis?: Vec3;
  angle?: number;
}

export interface GeometryRef {
  /** Occurrence ids from the top level down to the part. */
  path: number[];
  kind?: ElementKind;
  /** TopologyIndex index of the part shape (= faceIndex / edgeIndex of its mesh). */
  index?: number;
  name?: string;
  use?: EntityUse;
}

export interface MateSpec {
  type: MateType;
  a: GeometryRef;
  b: GeometryRef;
  /** Distance in mm; angle in degrees. */
  value?: number;
  alignment?: Alignment;
  internal?: boolean;
  name?: string;
}

export interface PartProperties {
  designation: string;
  name: string;
  material: string;
  /** kg/m^3; null — no mass in the BOM. */
  density: number | null;
  /** Drawing sections (GOST 2.305 §9): true — always cut, false — kept whole in a longitudinal
   *  section, null — default rule (fasteners, shafts, axles, balls are not cut). */
  sectionCut?: boolean | null;
}

export type Color = [number, number, number, number];

export interface DefinitionInfo {
  id: number;
  kind: 'part' | 'assembly';
  name: string;
  properties: PartProperties;
  source: string;
  external: boolean;
  readOnly: boolean;
  loadError: string;
  color: Color | null;
  faces?: number;
  edges?: number;
}

export interface OccurrenceInfo {
  id: number;
  name: string;
  definition: number;
  kind: 'part' | 'assembly';
  placement: Placement;
  fixed: boolean;
  suppressed: boolean;
  designation: string;
  color: Color | null;
  pattern: number | null;
  /** Top level only: degrees of freedom of the subsystem holding the occurrence (0 when fixed). */
  dof?: number;
  /** The DOF belong to a subsystem of several occurrences. */
  dofShared?: boolean;
  children?: OccurrenceInfo[];
}

export interface MateInfo {
  id: number;
  type: MateType;
  name: string;
  a: GeometryRef;
  b: GeometryRef;
  value: number;
  alignment: Alignment;
  internal: boolean;
  suppressed: boolean;
  broken: string;
  status: MateStatus;
}

export interface SubsystemInfo {
  occurrences: number[];
  mates: number[];
  status: 'Solved' | 'Conflicting' | 'NotConverged';
  dof: number;
  rank: number;
  redundantMates: number[];
  conflictingMates: number[];
}

export interface SolveReport {
  success: boolean;
  totalDof: number;
  residualNorm: number;
  subsystems: SubsystemInfo[];
  brokenMates: number[];
  suppressedMates: number[];
}

export interface PatternInfo {
  id: number;
  kind: 'Linear' | 'Circular' | 'Feature';
  seed: number;
  instances: number[];
}

export interface ExplodeStep {
  occurrences: number[];
  direction: Vec3;
  /** mm */
  distance: number;
}

export interface ExplodedView {
  name: string;
  steps: ExplodeStep[];
}

export interface AssemblyState {
  name: string;
  definitions: DefinitionInfo[];
  occurrences: OccurrenceInfo[];
  mates: MateInfo[];
  patterns: PatternInfo[];
  explodedViews: ExplodedView[];
  solve: SolveReport;
  history: { canUndo: boolean; canRedo: boolean; undoLabel: string; redoLabel: string; depth: number };
  /** Occurrence being dragged, or null. */
  dragging: number | null;
}

export interface PlacedPart {
  path: number[];
  pathName: string;
  definition: number;
  /** Key of the definition mesh (part coordinates); null for a part without geometry. */
  mesh: string | null;
  world: Placement;
  color: Color | null;
}

export interface AssemblyTessellation {
  meshes: Record<string, KernelMesh>;
  occurrences: PlacedPart[];
}

export interface ExplodeOptions {
  /** Index of the exploded view. */
  view?: number;
  /** Step s runs on clamp(progress − s, 0, 1). */
  progress?: number;
}

export interface TessellateOptions extends ExplodeOptions {
  params?: { linearDeflection?: number; angularDeflection?: number; relative?: boolean };
  /** Mesh keys the caller already holds: not sent again. */
  skip?: string[];
}

export interface EntityInfo {
  type: 'point' | 'axis' | 'plane' | 'cylinder' | 'sphere';
  point: Vec3;
  direction?: Vec3;
  radius?: number;
}

export interface ElementInfo {
  path: number[];
  kind: ElementKind;
  index: number;
  geometry: string;
  name: string | null;
  /** World coordinates; null when the element has no mate entity (entityError says why). */
  entity: EntityInfo | null;
  entityError?: string;
}

export interface InterferenceReport {
  parts: { path: number[]; pathName: string; definition: number }[];
  pairs: { a: number; b: number; kind: 'interference' | 'touching'; volume: number; distance: number }[];
  totalPairs: number;
  candidatePairs: number;
  totalVolume: number;
}

export interface BomRow {
  position: number;
  positionPath: string;
  level: number;
  definition: number;
  kind: 'part' | 'assembly';
  designation: string;
  name: string;
  material: string;
  quantity: number;
  /** mm^3 */
  unitVolume: number;
  /** kg */
  unitMass: number | null;
  totalMass: number | null;
  children: BomRow[];
}

export interface Bom {
  rows: BomRow[];
  totalMass: number | null;
  totalVolume: number;
  totalParts: number;
}

export interface FastenerSpec {
  /** Standard id from fastenerCatalog(), e.g. 'iso-4017'. */
  standard: string;
  size: number;
  /** Bolt, screw: shank length; pin: length. */
  length?: number;
}

export interface HoleSite {
  hole: GeometryRef;
  entryFace: GeometryRef;
  exitFace: GeometryRef | null;
  radius: number;
  depth: number;
}

export interface FastenerInsertSpec {
  part: number[];
  /** World direction from the head to the tip. */
  insertion: Vec3;
  radius: number;
  tolerance?: number;
  /** Hole face indices to use (default: every hole of this radius along the direction). */
  holes?: number[];
  screw: FastenerSpec;
  headWashers?: FastenerSpec[];
  nutWashers?: FastenerSpec[];
  nut?: FastenerSpec | null;
}

export interface StackPlacement {
  site: HoleSite;
  screw: number;
  headWashers: number[];
  nutWashers: number[];
  nut: number | null;
  mates: number[];
  grip: number;
  protrusion: number;
}

export interface FastenerStandardInfo {
  id: string;
  designation: string;
  kind: 'hex-bolt' | 'socket-head-screw' | 'hex-nut' | 'plain-washer' | 'spring-washer' | 'parallel-pin';
  gost: boolean;
  counterpart: string | null;
  sizes: number[];
  verified: boolean;
  source: string;
  verification: string;
}

export interface FastenerDescription {
  kind: FastenerStandardInfo['kind'];
  name: string;
  designation: string;
  equivalentDesignation: string;
  material: string;
  density: number;
  volume: number;
  mass: number;
  thread: { present: boolean; designation: string; pitch: number; length: number };
  dimensions: Record<string, number>;
}

export interface LoadReport {
  fileVersion: number;
  migrated: boolean;
  sources: { definition: number; name: string; source: string; status: string; message: string }[];
  mates: { mate: number; name: string; outcome: string; message: string }[];
  warnings: string[];
}

export interface InsertPartSpec {
  name?: string;
  format?: 'part-json' | 'step';
  data: string;
  placement?: PlacementInput;
  properties?: Partial<PartProperties>;
  fixed?: boolean;
  parent?: number;
  /** Top level: move the new component next to the assembly along +X (20 mm gap). */
  beside?: boolean;
}

export class AssemblyDocumentHandle {
  readonly handle: number;
  readonly disposed: boolean;
  dispose(): void;
  state(): AssemblyState;
  save(options?: { embedGeometry?: boolean; indent?: number }): string;
  exportStep(options?: { schema?: 'AP214' | 'AP242' }): string;
  insertPart(spec: InsertPartSpec): { definition: number; occurrence: number };
  insertOccurrence(spec: {
    definition: number;
    placement?: PlacementInput;
    name?: string;
    parent?: number;
    fixed?: boolean;
    beside?: boolean;
  }): number;
  insertSubassembly(spec: { source: string; name?: string; placement?: PlacementInput; fixed?: boolean; beside?: boolean }): {
    definition: number;
    occurrence: number;
  };
  removeOccurrence(id: number): void;
  setOccurrence(
    id: number,
    changes: {
      fixed?: boolean;
      suppressed?: boolean;
      name?: string;
      designation?: string;
      placement?: PlacementInput;
      color?: number[] | null;
    },
  ): void;
  setProperties(definition: number, properties: Partial<PartProperties>): void;
  addMate(spec: MateSpec): number;
  editMate(id: number, spec: MateSpec): void;
  removeMate(id: number): void;
  setMateSuppressed(id: number, suppressed: boolean): void;
  solve(): SolveReport;
  undo(): void;
  redo(): void;
  dragBegin(occurrence: number, point: Vec3): { localPoint: Vec3 };
  dragMove(target: Vec3): { solve: SolveReport; placement: Placement; grabbed: Vec3; placements: PlacedPart[] };
  dragEnd(): void;
  dragCancel(): void;
  addPattern(spec: Record<string, unknown> & { kind: 'linear' | 'circular' | 'feature'; seed: number }): number;
  removePattern(id: number): void;
  addExplodedView(view: ExplodedView): number;
  removeExplodedView(index: number): void;
  tessellate(options?: TessellateOptions): AssemblyTessellation;
  placements(options?: ExplodeOptions): PlacedPart[];
  element(ref: GeometryRef): ElementInfo;
  interference(options?: {
    reportTouching?: boolean;
    includeSameTopLevel?: boolean;
    volumeTolerance?: number;
    contactTolerance?: number;
  }): InterferenceReport;
  bom(mode?: 'flat' | 'structured'): Bom;
  findHoles(spec: { part: number[]; insertion: Vec3; radius: number; tolerance?: number }): HoleSite[];
  insertFasteners(spec: FastenerInsertSpec): StackPlacement[];
}

export interface AssemblyInfo {
  occtVersion: string;
  liveDocuments: number;
  cachedMeshes: number;
  sources: string[];
  formatVersion: number;
}

export class AssemblyKernel {
  info(): AssemblyInfo;
  fastenerCatalog(): FastenerStandardInfo[];
  describeFastener(spec: FastenerSpec): FastenerDescription;
  registerSource(name: string, text: string): void;
  createDocument(name?: string): AssemblyDocumentHandle;
  loadDocument(json: string): { document: AssemblyDocumentHandle; report: LoadReport };
  importStep(text: string): AssemblyDocumentHandle;
}

export type AssemblyModuleFactory = (options?: Record<string, unknown>) => Promise<unknown>;

export function loadAssembly(factory: AssemblyModuleFactory, moduleOptions?: Record<string, unknown>): Promise<AssemblyKernel>;
