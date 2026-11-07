// Types of bendcore_kernel_api.mjs (docs/ui/KERNEL_VIEWPORT.md).

export type KernelErrorCode =
  | 'InvalidInput'
  | 'InvalidShape'
  | 'OperationFailed'
  | 'ParseError'
  | 'IoError'
  | 'Unsupported'
  | 'Disposed';

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
}

export interface KernelInfo {
  occtVersion: string;
  liveShapes: number;
}

export class Kernel {
  info(): KernelInfo;
  readStep(bytes: Uint8Array): KernelShape;
}

export type KernelModuleFactory = (options?: Record<string, unknown>) => Promise<unknown>;

export function loadKernel(factory: KernelModuleFactory, moduleOptions?: Record<string, unknown>): Promise<Kernel>;

export function faceOfTriangle(faceSpans: Uint32Array, triangle: number): number;
