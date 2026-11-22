// Types of bendcore_kernel_api.mjs (docs/ui/KERNEL_VIEWPORT.md).

export type KernelErrorCode =
  | 'InvalidInput'
  | 'InvalidShape'
  | 'OperationFailed'
  | 'ParseError'
  | 'IoError'
  | 'Unsupported'
  // A hardware fault (SIGSEGV / SIGBUS / SIGFPE) inside OCCT caught by the kernel's fault guard
  // (native builds; WASM has no signals and traps instead — ModuleFault). The module state is
  // suspect: the UI treats it as a module fault (docs/ui/APP_SHELL.md, "Сбой модуля").
  | 'Fault'
  | 'Disposed'
  // Raised by the main-thread worker client (src/ui/worker/wasmClient.ts), not by the module:
  // the worker was terminated by cancel(), or failed; a call trapped inside the module
  // (WebAssembly.RuntimeError: the module state is unknown, docs/ui/APP_SHELL.md "Сбой модуля").
  | 'Cancelled'
  | 'WorkerFailed'
  | 'ModuleFault';

export type SurfaceType = 'Plane' | 'Cylinder' | 'Cone' | 'Sphere' | 'Torus' | 'BSpline' | 'Other';
export type CurveType = 'Line' | 'Circle' | 'Ellipse' | 'BSpline' | 'Other' | 'Degenerated';
export type Vec3 = [number, number, number];

export class KernelError extends Error {
  readonly code: KernelErrorCode;
  /** Kernel message without the code prefix. */
  readonly detail: string;
  constructor(code: KernelErrorCode, message: string);
}

export interface TopologyCounts {
  solids: number;
  shells: number;
  faces: number;
  edges: number;
  vertices: number;
}

export interface VolumeProperties {
  /** mm^3, signed (negative for an inside-out solid). */
  volume: number;
  centerOfMass: Vec3;
}

export interface SurfaceProperties {
  /** mm^2 */
  area: number;
  centroid: Vec3;
}

export interface BoundingBox {
  min: Vec3;
  max: Vec3;
}

export interface ValidityReport {
  valid: boolean;
  issues: string[];
}

/** Analytic parameters of a face surface in global coordinates; the fields depend on the type. */
export interface SurfaceParameters {
  origin?: Vec3; // Plane
  normal?: Vec3; // Plane, out of the material
  radius?: number; // Cylinder, Sphere
  center?: Vec3; // Sphere
  referenceRadius?: number; // Cone
  semiAngle?: number; // Cone, radians
  apex?: Vec3; // Cone
  majorRadius?: number; // Torus
  minorRadius?: number; // Torus
  axisOrigin?: Vec3; // Cylinder, Cone, Torus
  axisDirection?: Vec3; // Cylinder, Cone, Torus
  uDegree?: number; // BSpline
  vDegree?: number;
  uPoles?: number;
  vPoles?: number;
}

export interface FaceInfo {
  index: number;
  surfaceType: SurfaceType;
  orientation: 'Forward' | 'Reversed';
  area: number;
  centroid: Vec3;
  parameters: SurfaceParameters;
}

export interface TessellationParams {
  /** mm, or a fraction of the size when relative (default 0.1). */
  linearDeflection?: number;
  /** radians (default 0.5). */
  angularDeflection?: number;
  relative?: boolean;
  /** false: faces without triangulation are listed in missingFaces instead of failing. */
  requireAllFaces?: boolean;
}

export interface EdgePolyline {
  edgeIndex: number;
  /** xyz triples, mm */
  points: Float32Array;
}

export interface KernelMesh {
  positions: Float32Array;
  normals: Float32Array;
  indices: Uint32Array;
  /** Triples (faceIndex, firstIndex, indexCount), ordered and contiguous over indices. */
  faceSpans: Uint32Array;
  edges: EdgePolyline[];
  missingFaces: Uint32Array;
  missingEdges: Uint32Array;
}

export type ExportFormat = 'step' | 'iges' | 'stl' | '3mf';

/** Options of every export (docs/ui/DATA_EXCHANGE.md); fields of other formats are ignored. */
export interface ExportOptions {
  /** Part name written to STEP/IGES (PRODUCT name) and as the STL solid name. */
  name?: string;
  schema?: 'AP214' | 'AP242';
  unit?: 'mm' | 'm' | 'inch';
  igesMode?: 'brep' | 'faces';
  stlFormat?: 'binary' | 'ascii';
  /** Chordal deflection of STL/3MF meshes, mm (default 0.05). */
  linearDeflection?: number;
  /** Angular deflection of STL/3MF meshes, degrees (default 20). */
  angularDeflectionDeg?: number;
}

/** Units: kg, mm, kg·mm². Inertia about the centre of mass, axes parallel to the global axes. */
export interface MassProperties {
  mass: number;
  volume: number;
  area: number;
  centerOfMass: Vec3;
  inertiaAtCenter: [Vec3, Vec3, Vec3];
  /** Ascending. */
  principalMoments: Vec3;
  principalAxes: [Vec3, Vec3, Vec3];
  bodies: number;
}

export interface ImportStructure {
  /** Several placed parts or an assembly root. */
  assembly: boolean;
  parts: number;
  definitions: number;
  paths: string[];
  warnings: string[];
  name: string;
  fileUnitMm: number | null;
}

export type MeasureElement = { kind: 'face' | 'edge' | 'vertex'; index: number };
export type MeasureSpec =
  | { type: 'distance' | 'angle'; a: MeasureElement; b: MeasureElement }
  | { type: 'length' | 'area' | 'radius'; a: MeasureElement };
export interface Measurement {
  type: 'distance' | 'angle' | 'length' | 'area' | 'radius';
  unit: 'mm' | 'mm2' | 'deg';
  value: number;
  /** distance: nearest points; angle: vertex, point on the first, on the second; length: start, end, middle. */
  points: Vec3[];
  diameter?: number;
  /** distance with a reference mesh point: the point snapped to a mesh vertex */
  vertexA?: boolean;
  vertexB?: boolean;
}
/**
 * Point on a reference mesh (part and assembly modules): the point of triangle `triangle` nearest
 * to `point`, snapped to a corner within `snap` mm. `mesh` is the mesh id of the document.
 */
export type MeshPointElement = { kind: 'mesh'; mesh: string | number; triangle: number; point: Vec3; snap?: number };
/** Measure spec of the part and assembly modules: a distance may use points on reference meshes. */
export type MeasureSpecWithMesh<E = MeasureElement> =
  | { type: 'distance'; a: E | MeshPointElement; b: E | MeshPointElement }
  | { type: 'angle'; a: E; b: E }
  | { type: 'length' | 'area' | 'radius'; a: E };
/** Reference mesh for display: xyz positions and 3 indices per triangle (world/part mm). */
export interface MeshGeometry {
  positions: Float32Array;
  indices: Uint32Array;
}
export interface MeshProperties {
  vertices: number;
  triangles: number;
  min: Vec3;
  max: Vec3;
  size: Vec3;
  /** mm^2 */
  area: number;
  /** mm^3 by the divergence theorem; null unless the mesh is closed */
  volume: number | null;
  closed: boolean;
  openEdges: number;
}
export interface MeshReadReport {
  encoding: 'binary' | 'ascii' | '3mf';
  facetsRead: number;
  degenerateDropped: number;
  objects: number;
}
export interface MeshSectionResult {
  /** net enclosed area (closed, consistently oriented mesh), mm^2 */
  area: number;
  /** total contour length, mm */
  length: number;
  /** xyz triples; a closed contour repeats its first point */
  polylines: Float32Array[];
  closed: boolean[];
}
export type MeshUnit = 'mm' | 'cm' | 'm' | 'in';
export interface SectionPlane {
  origin: Vec3;
  normal: Vec3;
}
export interface SectionResult {
  area: number;
  polylines: Float32Array[];
}
export interface LibraryMaterial {
  id: string;
  name: string;
  category: string;
  /** kg/m^3 */
  density: number;
  color: [number, number, number];
  source: string;
  verified: boolean;
  youngModulus?: number;
  poissonRatio?: number;
  yieldStrength?: number;
}

export class KernelShape {
  readonly handle: number;
  readonly disposed: boolean;
  dispose(): void;
  counts(): TopologyCounts;
  volumeProperties(): VolumeProperties;
  surfaceProperties(): SurfaceProperties;
  boundingBox(): BoundingBox;
  checkValidity(): ValidityReport;
  faceTypes(): SurfaceType[];
  edgeTypes(): CurveType[];
  face(index: number): FaceInfo;
  tessellate(params?: TessellationParams): KernelMesh;
  exportFile(format: ExportFormat, options?: ExportOptions): Uint8Array;
  massProperties(density: number): MassProperties;
  /** Measurement between elements (kernel indices); distances in mm, angles in degrees. */
  measure(spec: MeasureSpec): Measurement;
  /** Section by a plane: area (mm^2) and contour polylines (xyz triples). */
  section(plane: SectionPlane, deflection?: number): SectionResult;
}

export interface KernelInfo {
  occtVersion: string;
  liveShapes: number;
}

export class Kernel {
  info(): KernelInfo;
  readStep(bytes: Uint8Array): KernelShape;
  importModel(bytes: Uint8Array, format: 'step' | 'iges'): { shape: KernelShape; structure: ImportStructure };
  exchangeInfo(): { zipMethod: 'deflate' | 'stored' };
  /** Built-in material library of the kernel. */
  materials(): LibraryMaterial[];
}

export type KernelModuleFactory = (options?: Record<string, unknown>) => Promise<unknown>;

export function loadKernel(factory: KernelModuleFactory, moduleOptions?: Record<string, unknown>): Promise<Kernel>;

export function faceOfTriangle(faceSpans: Uint32Array, triangle: number): number;
