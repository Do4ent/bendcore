/**
 * Shared Types - используются и на фронтенде и в бэкенде
 * Гарантирует совместимость API контрактов
 */

// ============ BASIC TYPES ============

export type Vector3 = [number, number, number];
export type Vector2 = [number, number];

export interface Bend {
  angle: number;
  radius: number;
}

export interface MeshData {
  vertices: number[];
  indices: number[];
}

// ============ TUBE ANALYSIS TYPES ============

export interface TubeAnalysis {
  od: number;
  wall: number;
  length: number;
  bends?: Bend[];
  confidence?: number;
  inner_diameter?: number;
  shape_type?: string;
  notes?: string;
  valid?: boolean;
}

export interface AnalysisResult {
  ok: boolean;
  tube: TubeAnalysis;
  mesh: MeshData;
  viewportMesh?: ViewportMesh;
}

export interface ViewportMesh {
  id: string;
  name: string;
  mesh: MeshData;
  color?: number;
}

// ============ FEA TYPES ============

export interface FEARequest {
  mesh: MeshData;
  material: 'steel' | 'aluminum' | 'titanium';
  pressure: number;
}

export interface FEAResult {
  max_stress: number;
  max_displacement: number;
  element_count: number;
  converged: boolean;
}

export interface FEAResponse {
  ok: boolean;
  fea: FEAResult;
}

// ============ TOPOLOGY OPTIMIZATION TYPES ============

export interface TopologyOptimizationRequest {
  stress: number;
  displacement: number;
  od: number;
  wall: number;
  length: number;
  material: 'steel' | 'aluminum' | 'titanium';
  target?: number;
}

export interface TopologyOptimizationResult {
  original_wall: number;
  optimal_wall: number;
  weight_reduction_percent: number;
  safety_factor: number;
  recommendation: string;
}

export interface TopologyOptimizationResponse {
  ok: boolean;
  topology: TopologyOptimizationResult;
}

// ============ DRAWING EXPORT TYPES ============

export interface DrawingExportRequest {
  od?: number;
  wall?: number;
  length?: number;
  bends?: Bend[];
  confidence?: number;
}

export interface DrawingExportResponse {
  svg: string;
  filename: string;
}

// ============ API RESPONSE ENVELOPE ============

export interface ApiResponse<T> {
  success: boolean;
  data?: T;
  error?: string;
  errors?: string[];
  timestamp: string;
}

// ============ IMPORT PIPELINE TYPES ============

export interface ImportedTube {
  id: string;
  centerline: Vector3[];
  diameter: number;
  material?: string;
  profile?: string;
  confidence: number;
}

export interface ImportGroup {
  id: string;
  tube_count: number;
  tubes: ImportedTube[];
}

export interface ImportFilterResponse {
  total_tubes: number;
  group_count: number;
  groups: ImportGroup[];
}

// ============ TYPE GUARDS ============

export function isMeshData(obj: unknown): obj is MeshData {
  return (
    typeof obj === 'object' &&
    obj !== null &&
    'vertices' in obj &&
    'indices' in obj &&
    Array.isArray((obj as any).vertices) &&
    Array.isArray((obj as any).indices)
  );
}

export function isAnalysisResult(obj: unknown): obj is AnalysisResult {
  return (
    typeof obj === 'object' &&
    obj !== null &&
    'ok' in obj &&
    'tube' in obj &&
    'mesh' in obj
  );
}

export function isFEAResult(obj: unknown): obj is FEAResult {
  return (
    typeof obj === 'object' &&
    obj !== null &&
    'max_stress' in obj &&
    'max_displacement' in obj &&
    'element_count' in obj
  );
}

export function isTopologyResult(obj: unknown): obj is TopologyOptimizationResult {
  return (
    typeof obj === 'object' &&
    obj !== null &&
    'original_wall' in obj &&
    'optimal_wall' in obj &&
    'weight_reduction_percent' in obj
  );
}
