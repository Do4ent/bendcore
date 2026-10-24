/**
 * ЭТАП 3: СБОРКА И СИМУЛЯЦИЯ
 * Типы для сборки и FEA симуляции
 */

export interface AssemblyPart {
  id: string;
  name: string;
  partId: string;
  placement: {
    position: [number, number, number];
    rotation: [number, number, number];
  };
  visibility: boolean;
}

export interface AssemblyConstraint {
  id: string;
  type: 'coincident' | 'parallel' | 'perpendicular' | 'distance' | 'concentric' | 'coplanar';
  part1: string;
  face1: string;
  part2: string;
  face2: string;
  value?: number;  // Для distance constraints
  status: 'satisfied' | 'conflicting' | 'redundant';
}

export interface BendCoreAssembly {
  id: string;
  name: string;
  parts: AssemblyPart[];
  constraints: AssemblyConstraint[];
  explodedView?: {
    active: boolean;
    distance: number;
    direction: [number, number, number];
  };
}

export interface FEAResult {
  id: string;
  name: string;
  type: 'stress' | 'strain' | 'displacement' | 'thermal';
  maxValue: number;
  minValue: number;
  avgValue: number;
  unit: string;
  colorMap: 'jet' | 'hot' | 'cool' | 'grayscale';
  meshSize: number;
  nodeCount: number;
  elementCount: number;
  executionTime: number;
  status: 'pending' | 'running' | 'completed' | 'failed';
  timestamp: Date;
  isStub: boolean;  // Обозначаем, что это stub (заглушка)
}

export interface AssemblyState {
  assemblies: BendCoreAssembly[];
  activeAssemblyId: string | null;
}
