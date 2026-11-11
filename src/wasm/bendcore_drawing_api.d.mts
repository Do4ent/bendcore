// Types of bendcore_drawing_api.mjs (docs/ui/DRAWING_UI.md). Sheet coordinates are millimetres
// with the origin in the lower left corner and y up; the SVG text of sheetPicture() has y down.

import { KernelError } from './bendcore_kernel_api.mjs';

export { KernelError };

export type GostFormat = 'A0' | 'A1' | 'A2' | 'A3' | 'A4';
export type Orientation = 'portrait' | 'landscape';
export type ViewKind = 'base' | 'projected' | 'axonometric' | 'section' | 'detail';
export type SheetSide = 'above' | 'below' | 'leftOf' | 'rightOf';
export type StandardView = 'front' | 'top' | 'left' | 'right' | 'bottom' | 'rear';
export type DimensionType = 'horizontal' | 'vertical' | 'aligned' | 'radius' | 'diameter' | 'angle';
export type GeometricCharacteristic =
  | 'straightness' | 'flatness' | 'roundness' | 'cylindricity' | 'lineProfile' | 'surfaceProfile'
  | 'parallelism' | 'perpendicularity' | 'angularity' | 'position' | 'coaxiality' | 'symmetry'
  | 'circularRunout' | 'totalRunout';
export type RoughnessMethod = 'unspecified' | 'materialRemoval' | 'noMaterialRemoval';
/** [num, den] from the GOST 2.302 series, e.g. [1, 2] or [2, 1]. */
export type ScaleRatio = [number, number];
export type Point = [number, number];

export interface ScaleInfo {
  ratio: ScaleRatio;
  label: string;
}
export interface Bounds {
  minX: number;
  minY: number;
  maxX: number;
  maxY: number;
}
export interface SignatureRow {
  surname: string;
  date: string;
}
export interface TitleFields {
  designation: string;
  name: string;
  material: string;
  mass: string;
  litera: string;
  organization: string;
  developer: SignatureRow;
  checker: SignatureRow;
  techControl: SignatureRow;
  normControl: SignatureRow;
  approver: SignatureRow;
}
export type TitleFieldsInput = Partial<Omit<TitleFields, 'developer' | 'checker' | 'techControl' | 'normControl' | 'approver'>> & {
  developer?: Partial<SignatureRow>;
  checker?: Partial<SignatureRow>;
  techControl?: Partial<SignatureRow>;
  normControl?: Partial<SignatureRow>;
  approver?: Partial<SignatureRow>;
};
export interface ModelProperties {
  designation: string;
  name: string;
  material: string;
  /** kg/m³; null - the mass is not computed */
  density: number | null;
}
export interface RoughnessSpec {
  method?: RoughnessMethod;
  parameter?: string;
  value?: string;
  process?: string;
}

export interface ViewInfo {
  id: string;
  kind: ViewKind;
  parent: string;
  side: SheetSide | null;
  toward: [number, number, number];
  right: [number, number, number];
  scale: ScaleInfo | null;
  position: Point | null;
  hidden: boolean;
  centerLines: boolean;
  autoBalloons: boolean;
  inGroup: boolean;
  computedScale?: ScaleInfo;
  label?: string;
  bounds?: Bounds;
  fromCache?: boolean;
  axonometry?: 'isometric' | 'dimetric';
  planePoint?: [number, number, number];
  sectionKind?: 'cut' | 'slice';
  markView?: string;
  detailCenter?: Point;
  detailRadius?: number;
}
export interface DimensionInfo {
  id: string;
  view: string;
  type: DimensionType;
  offset: number;
  tolerance: string | null;
  toleranceDisplay: 'classOnly' | 'deviations' | 'classAndDeviations';
  textOverride: string | null;
  decimals: number;
  lastValue: number | null;
  state?: 'ok' | 'dangling';
  /** mm of the model, degrees for angles */
  value?: number;
  text?: string;
  reason?: string;
  labelPosition?: Point;
}
export interface SymbolInfo {
  id: string;
  view: string;
  at: number;
  state: 'ok' | 'dangling' | null;
}
export interface SheetInfo {
  id: string;
  format: GostFormat;
  multiplier: number;
  orientation: Orientation;
  scale: ScaleInfo | null;
  title: TitleFields;
  views: ViewInfo[];
  dimensions: DimensionInfo[];
  roughness: (SymbolInfo & { spec: RoughnessSpec })[];
  tolerances: (SymbolInfo & { characteristic: GeometricCharacteristic; tolerance: string; diameterZone: boolean; datums: string[] })[];
  datums: (SymbolInfo & { letter: string })[];
  roughnessRemainder: RoughnessSpec | null;
  technicalRequirements: string[];
  partsList: boolean;
  balloons: { definition: number; text: string; at: Point; target: Point }[];
  name?: string;
  width?: number;
  height?: number;
  computedScale?: ScaleInfo;
  /** Title block as printed: fields from the model where the sheet leaves them empty. */
  titleShown?: { designation: string; name: string; material: string; mass: string; scale: string; sheet: number; sheets: number };
}
export interface DrawingDescription {
  model: { kind: 'part' | 'assembly'; path: string; resultHash: string; revision: number; properties: ModelProperties };
  projection: 'firstAngle' | 'thirdAngle';
  sheets: SheetInfo[];
}

export interface DimensionReport {
  sheet: string;
  id: string;
  state: 'ok' | 'dangling';
  oldValue: number | null;
  newValue: number;
  changed: boolean;
  method: string;
  reason: string;
}
export interface UpdateReport {
  modelChanged: boolean;
  previousHash: string;
  currentHash: string;
  revision: number;
  viewsComputed: number;
  viewsFromCache: number;
  dimensions: DimensionReport[];
  /** "S1/D1: 60 → 80" */
  stale: string[];
  /** "S1/D2: reason" */
  dangling: string[];
  balloons: { sheet: string; view: string; placed: { position: number; definition: number }[]; withoutBalloon: number[] }[];
  warnings: string[];
}
export interface StalenessReport {
  modelChanged: boolean;
  storedHash: string;
  currentHash: string;
  staleViews: string[];
  dimensions: string[];
}
export interface EditResult {
  warnings: string[];
  dangling: string[];
}

export type PictureCurve =
  | { type: 'line'; points: [Point, Point] }
  | { type: 'polyline'; points: Point[]; closed: boolean }
  | { type: 'circle'; center: Point; radius: number }
  | { type: 'arc'; center: Point; radius: number; startDeg: number; endDeg: number };
export type PictureEdge = PictureCurve & {
  index: number;
  visible: boolean;
  class: 'sharp' | 'smooth' | 'silhouette' | 'sectionCut';
  edgeIndex: number | null;
  faceIndex: number | null;
};
export interface PictureView {
  id: string;
  kind: ViewKind;
  inGroup: boolean;
  label: string;
  scale: string;
  bounds: Bounds;
  /** sheet point = anchor + factor · view point; view point of a model point P = (P·right, P·up) */
  anchor: Point;
  factor: number;
  frame: { toward: [number, number, number]; right: [number, number, number]; up: [number, number, number] };
  edges: PictureEdge[];
}
export interface SheetPicture {
  sheet: string;
  width: number;
  height: number;
  svg: string;
  views: PictureView[];
}

export interface ViewSpec {
  kind: ViewKind;
  id?: string;
  /** base: orientation by standard view (default front); projected: standard view placed by GOST 2.305 */
  standard?: StandardView;
  toward?: [number, number, number];
  right?: [number, number, number];
  parent?: string;
  side?: SheetSide;
  axonometry?: 'isometric' | 'dimetric';
  planePoint?: [number, number, number];
  sectionKind?: 'cut' | 'slice';
  markView?: string;
  detailCenter?: Point;
  detailRadius?: number;
  scale?: ScaleRatio | null;
  position?: Point | null;
  hidden?: boolean;
  centerLines?: boolean;
  autoBalloons?: boolean;
}
export interface SheetOptions {
  id?: string;
  format?: GostFormat;
  multiplier?: number;
  orientation?: Orientation;
  scale?: ScaleRatio | null;
  title?: TitleFieldsInput;
}
export interface SheetChanges extends Omit<SheetOptions, 'id'> {
  technicalRequirements?: string[];
  roughnessRemainder?: RoughnessSpec | null;
  partsList?: boolean;
}
export interface CreateOptions {
  path?: string;
  properties?: Partial<ModelProperties>;
  sheet?: SheetOptions;
}
export interface SpecificationTable {
  designation: string;
  name: string;
  sections: {
    section: string;
    title: string;
    items: { format: string; zone: string; position: number; designation: string; name: string; quantity: number | null; note: string }[];
  }[];
}

export class DrawingHandle {
  readonly handle: number;
  readonly disposed: boolean;
  dispose(): void;
  describe(): DrawingDescription;
  save(): string;
  saveModel(): string;
  variables(): { name: string; expression: string; value: number | null; error: string | null }[];
  setVariable(name: string, expression: string | number): StalenessReport;
  setModelProperties(properties: Partial<ModelProperties>): UpdateReport;
  checkModel(): StalenessReport;
  updateFromModel(): UpdateReport;
  addSheet(options?: SheetOptions): string;
  removeSheet(sheet: string): EditResult;
  setSheet(sheet: string, changes: SheetChanges): EditResult;
  addView(sheet: string, spec: ViewSpec): string;
  removeView(sheet: string, view: string): string[];
  moveView(sheet: string, view: string, x: number, y: number): EditResult & { moved: string; group: boolean };
  setView(sheet: string, view: string, changes: Partial<Pick<ViewSpec, 'scale' | 'position' | 'hidden' | 'centerLines' | 'autoBalloons' | 'detailRadius'>>): EditResult;
  addDimension(sheet: string, view: string, picks: number[], options?: { id?: string; offset?: number; tolerance?: string; leaderAngle?: number }): string;
  setDimension(sheet: string, id: string, changes: { offset?: number; textShift?: number; decimals?: number; tolerance?: string | null; toleranceDisplay?: DimensionInfo['toleranceDisplay']; textOverride?: string | null; prefix?: string; suffix?: string }): EditResult;
  addRoughness(sheet: string, view: string, pick: number, spec?: RoughnessSpec & { id?: string; at?: number }): string;
  addTolerance(sheet: string, view: string, pick: number, spec: { characteristic: GeometricCharacteristic; tolerance: string; datums?: string[]; diameterZone?: boolean; id?: string; at?: number }): string;
  addDatum(sheet: string, view: string, pick: number, letter: string, spec?: { id?: string; at?: number }): string;
  removeAnnotation(sheet: string, id: string): EditResult;
  sheetPicture(sheet: string): SheetPicture;
  specification(): SpecificationTable;
  exportPdf(options?: { specification?: boolean }): Uint8Array;
  exportSheet(sheet: string, format: 'pdf' | 'dxf' | 'svg'): Uint8Array;
}

export class DrawingKernel {
  info(): { occtVersion: string; liveDrawings: number; formatVersion: number; font: { postScriptName: string; source: string } | null };
  setFont(fontBytes: Uint8Array, source?: string): { postScriptName: string; glyphs: number };
  createFromPart(partJson: string, options?: CreateOptions): DrawingHandle;
  createFromAssembly(assemblyJson: string, options?: CreateOptions): DrawingHandle;
  load(drawingJson: string, modelJson: string): { drawing: DrawingHandle; report: UpdateReport };
}

export type DrawingModuleFactory = (options?: Record<string, unknown>) => Promise<unknown>;

export function loadDrawingKernel(factory: DrawingModuleFactory, moduleOptions?: Record<string, unknown>): Promise<DrawingKernel>;
