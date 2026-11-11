/**
 * Типы панели FEA (src/features/simulation).
 */

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
  isStub: boolean;
}
