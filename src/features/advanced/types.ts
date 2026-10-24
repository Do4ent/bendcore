/**
 * ЭТАП 2: ПРОДВИНУТОЕ МОДЕЛИРОВАНИЕ
 * Типы для продвинутых операций
 */

export interface LoftFeature {
  id: string;
  name: string;
  profiles: string[];
  surfaceType: 'smooth' | 'linear' | 'ruled';
  railCurves?: string[];
  startTangent?: { plane: 'xy' | 'xz' | 'yz'; angle: number };
  endTangent?: { plane: 'xy' | 'xz' | 'yz'; angle: number };
}

export interface SweepFeature {
  id: string;
  name: string;
  profile: string;
  path: string;
  twist?: number;
  scaleProfile?: boolean;
  keepNormal: boolean;
}

export interface SurfaceFeature {
  id: string;
  name: string;
  surfaceType: 'face' | 'surface' | 'blend' | 'fill';
  inputs: string[];
  tangentContinuity?: 'positional' | 'tangent' | 'curvature';
}

export type AdvancedFeature = LoftFeature | SweepFeature | SurfaceFeature;

export interface AdvancedState {
  features: AdvancedFeature[];
  activeFeatureId: string | null;
}
