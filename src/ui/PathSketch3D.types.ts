/**
 * PathSketch3D.types.ts
 * Type-only exports from PathSketch3D — no React, no three.js dependency.
 * Import types from here in non-UI code and tests to avoid loading heavy deps.
 */

export interface Vector3 {
  x: number;
  y: number;
  z: number;
}

export interface PathVertex {
  id: string;
  position: Vector3;
  is_fixed: boolean;
  is_selected: boolean;
}

export type DimensionType = 'L' | 'S' | 'B' | 'A';

export interface PathSegment {
  id: string;
  from_vertex: string;
  to_vertex: string;
  dimension_L?: number;
  dimension_S?: number;
  bend_angle?: number;
  plane_rotation?: number;
  selected_dimension: DimensionType;
}

export type ConstraintType =
  | 'COINCIDENT'
  | 'PARALLEL'
  | 'PERPENDICULAR'
  | 'EQUALITY'
  | 'FIXED'
  | 'DISTANCE'
  | 'ANGLE';

export interface Constraint {
  id: string;
  type: ConstraintType;
  entity_ids: string[];
  value?: number;
  is_satisfied: boolean;
  error: number;
}

export interface PathSketchData {
  sketch_id: string;
  profile_id: string;
  profile_r: number;
  profile_s_min: number;
  profile_b_min: number;
  profile_b_max: number;
  vertices: PathVertex[];
  segments: PathSegment[];
  constraints: Constraint[];
  solver_state: {
    converged: boolean;
    iterations: number;
    residual: number;
  };
}
