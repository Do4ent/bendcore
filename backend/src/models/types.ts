/**
 * BendCore Type Definitions
 * Core data structures for the backend
 */

// ============ PROFILE MANAGER TYPES ============

export interface ProfileDiameter {
  id: string;
  OD: number;                    // Outer diameter (inches)
  R: number;                     // Bend radius (inches)
  S_min: number;                 // Minimum straight length (inches)
  N: number;                     // Number of dies/stations
  B_min: number;                 // Minimum bend angle (degrees)
  B_max: number;                 // Maximum bend angle (degrees)
  discrete_type: 'continuous' | 'step';
  discrete_values?: number[];    // For step type: allowed bend angles
}

export interface BendingProfile {
  id: string;
  name: string;
  category: string;
  manufacturer: string;
  type: string;
  description: string;
  capacity_max_OD?: number;
  capacity_max_wall?: number;
  diameters: ProfileDiameter[];
}

export interface TubeSpecification {
  standard: string;
  material: string;
  type: string;
  description?: string;
  profile_id: string;
  diameter: number;
  wall_thickness: number;
  first_segment_allowance: number;
  last_segment_allowance: number;
  calculate_allowance: boolean;
}

export interface ValidationResult {
  valid: boolean;
  errors: string[];
  warnings: string[];
}

// ============ SKETCH SOLVER TYPES ============

export interface Vector3 {
  x: number;
  y: number;
  z: number;
}

export interface Vertex {
  id: string;
  position: Vector3;
  is_fixed: boolean;
}

export interface Segment {
  id: string;
  from_vertex: string;
  to_vertex: string;
  dimension_L: number;
  dimension_S: number;
  bend_angle: number;
  plane_rotation: number;
}

export type ConstraintType =
  | 'COINCIDENT'
  | 'PARALLEL'
  | 'PERPENDICULAR'
  | 'EQUALITY'
  | 'FIXED'
  | 'DISTANCE'
  | 'ANGLE';

export interface ConstraintDef {
  id: string;
  type: ConstraintType;
  entity_ids: string[];
  value: number;
  is_satisfied: boolean;
  error: number;
}

export interface SolveResult {
  converged: boolean;
  iterations: number;
  residual: number;
  conflicts: string[];
  over_constrained: string[];
  vertices: Vertex[];
  constraints: ConstraintDef[];
}

// ============ TUBE BUILDER TYPES ============

export interface Cylinder {
  center: Vector3;
  axis: Vector3;
  length: number;
  outer_radius: number;
  inner_radius: number;
  segment_id: string;
}

export interface Torus {
  center: Vector3;
  axis: Vector3;
  major_radius: number;
  minor_radius: number;
  start_angle: number;
  end_angle: number;
  segment_id: string;
}

export interface TubeBody {
  tube_id: string;
  straight_sections: Cylinder[];
  bend_sections: Torus[];
  total_length: number;
  total_volume: number;
  total_mass: number;
  valid: boolean;
  validation_errors: string[];
  validation_warnings: string[];
  feature_ids: string[];
}

export interface TubeProperties {
  total_length: number;
  total_volume: number;
  total_mass: number;
  surface_area?: number;
  center_of_mass?: Vector3;
}

// ============ PROJECT & PERSISTENCE TYPES ============

export interface Project {
  id: string;
  name: string;
  description?: string;
  created_at: string;
  updated_at: string;
  tubes: Tube[];
  metadata?: Record<string, unknown>;
}

export interface Tube {
  id: string;
  name: string;
  specification: TubeSpecification;
  sketch?: PathSketch;
  geometry?: TubeBody;
  properties?: TubeProperties;
  created_at: string;
  updated_at: string;
}

export interface PathSketch {
  id: string;
  vertices: Vertex[];
  segments: Segment[];
  constraints: ConstraintDef[];
  profile_r: number;
  profile_s_min: number;
  profile_b_min: number;
  profile_b_max: number;
}

// ============ API RESPONSE TYPES ============

export interface ApiResponse<T> {
  success: boolean;
  data?: T;
  error?: string;
  errors?: string[];
  timestamp: string;
}

export interface ApiError {
  error: true;
  message: string;
  code?: string;
  details?: Record<string, unknown>;
}

// ============ GEOMETRY TYPES ============

export interface BoundingBox {
  min: Vector3;
  max: Vector3;
  center: Vector3;
  size: Vector3;
}

export interface AnalysisReport {
  tube_id: string;
  valid: boolean;
  checks: ValidationCheck[];
  summary: string;
}

export interface ValidationCheck {
  name: string;
  passed: boolean;
  message: string;
  severity: 'error' | 'warning' | 'info';
}
