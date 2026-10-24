/**
 * Assembly Module - PHASE 2 Production Ready
 *
 * Multi-level hierarchy management with advanced features:
 * - Unlimited nesting depth
 * - In-context editing
 * - Level of Detail (LOD)
 * - Assembly features (patterns, mirrors)
 * - Exploded views
 * - Constraint solving
 */

export { MultiLevelHierarchyManager } from './MultiLevelHierarchyManager';
export { HierarchyManager, AssemblyHierarchyManager } from './HierarchyManager';
export { AssociativeSyncEngine } from './AssociativeSyncEngine';
export { LargeAssemblyOptimizer } from './LargeAssemblyOptimizer';

// Re-export types
export type {
  Transform,
  Vector3,
  Plane,
  Component,
  AssemblyNode,
  Constraint,
  AssemblyFeature,
  GeometryLOD,
  InContextEditState,
  AssemblyEdit,
  ExplodedState
} from './MultiLevelHierarchyManager';
