/**
 * ЭТАП 1: БАЗОВОЕ МОДЕЛИРОВАНИЕ
 * Типы для базовых операций моделирования
 */

export interface PadFeature {
  id: string;
  name: string;
  sketchId: string;
  length: number;
  direction: 'normal' | 'reverse' | 'symmetric';
  offset: number;
  draftAngle?: number;
  taper?: number;
}

export interface PocketFeature {
  id: string;
  name: string;
  sketchId: string;
  depth: number;
  depthType: 'blind' | 'through' | 'twoSided';
  offset: number;
  draftAngle?: number;
}

export interface FilletFeature {
  id: string;
  name: string;
  radius: number;
  edges: string[];
  preserveEdges: boolean;
}

export interface ChamferFeature {
  id: string;
  name: string;
  size: number;
  edges: string[];
  chamferType: 'distance' | 'angle';
  angle?: number;
}

export interface MirrorFeature {
  id: string;
  name: string;
  plane: 'xy' | 'xz' | 'yz' | 'custom';
  features: string[];
  customPlaneNormal?: [number, number, number];
}

export interface PatternFeature {
  id: string;
  name: string;
  patternType: 'linear' | 'circular';
  feature: string;

  // Linear
  direction?: 'x' | 'y' | 'z';
  spacing?: number;
  count?: number;

  // Circular
  axis?: 'x' | 'y' | 'z';
  angleStep?: number;
  instances?: number;
}

export type ModelingFeature =
  | PadFeature
  | PocketFeature
  | FilletFeature
  | ChamferFeature
  | MirrorFeature
  | PatternFeature;

export interface ModelingState {
  features: ModelingFeature[];
  activeFeatureId: string | null;
  history: ModelingFeature[][];
  historyIndex: number;
}
