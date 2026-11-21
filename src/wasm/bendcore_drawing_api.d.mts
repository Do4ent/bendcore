// Types of bendcore_drawing_api.mjs (docs/ui/DRAWING_UI.md). Sheet coordinates are millimetres
// with the origin in the lower left corner and y up; the SVG text of sheetPicture() has y down.

import { KernelError } from './bendcore_kernel_api.mjs';

export { KernelError };

export type GostFormat = 'A0' | 'A1' | 'A2' | 'A3' | 'A4';
export type Orientation = 'portrait' | 'landscape';
export type ViewKind = 'base' | 'projected' | 'axonometric' | 'section' | 'detail' | 'auxiliary';
/** Projection method (GOST 2.305 / ISO 128-30: first angle = method E, third angle = method A). */
export type ProjectionMethod = 'firstAngle' | 'thirdAngle';
/** Sheet style: ESKD (GOST 2.104 title block) or ISO (ISO 5457 frame, ISO 7200 title block). */
export type SheetStyle = 'eskd' | 'iso7200';
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
  /** ISO 7200 fields (legal owner = organization, creator = developer, approver = approver). */
  documentType: string;
  documentStatus: string;
  revision: string;
  /** YYYY-MM-DD (ISO 8601). */
  issueDate: string;
  /** ISO 639 code; empty - ru. */
  language: string;
  department: string;
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
  /** Line of the position shelves (ГОСТ 2.109): auto — right, else left, above, below. */
  balloonLine: 'auto' | 'right' | 'left' | 'above' | 'below';
  /** Rolling bearings in a section through the axis — conventionally (ГОСТ 2.420). */
  bearings: boolean;
  bearingSymbol: 'diagonals' | 'rollingElement';
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
  /** Stepped section: further plane points (ГОСТ 2.305). */
  stepPoints?: [number, number, number][];
  /** Local sections of the view: boundary in view coordinates (model mm), depth from the front. */
  localSections: LocalSectionInfo[];
  /** Auxiliary view (GOST 2.305, 5.4-5.6): turned so that the parent's up is up on the sheet. */
  rotated?: boolean;
  /** Auxiliary view: designation letter, rotation from the arrow orientation (0 - not rotated). */
  letter?: string;
  rotationDeg?: number;
  /** Auxiliary view: arrow on the parent view, sheet mm. */
  arrow?: { tip: Point; tail: Point; direction: Point; letterPosition: Point };
}
export interface LocalSectionInfo {
  points: Point[];
  spline: boolean;
  depth: number;
}
export interface ThreadNoteInfo {
  id: string;
  view: string;
  toleranceField: string;
  offset: number;
  leaderAngle: number;
  state: 'ok' | 'dangling' | null;
  /** "M10×1,25LH-6H" */
  text?: string;
  reason?: string;
  labelPosition?: Point;
}
export interface HistoryInfo {
  canUndo: boolean;
  canRedo: boolean;
  undoLabel: string;
  redoLabel: string;
  undoDepth: number;
  redoDepth: number;
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
  /** First dimension line segment on the sheet (mm). */
  dimensionLine?: [Point, Point];
  /** Chain / baseline group (ГОСТ 2.307) the dimension belongs to; null — a single dimension. */
  group: string | null;
  groupMode?: 'chain' | 'baseline';
  groupSpacing?: number;
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
  /** Positions on the sheet: automatic ones of the views (id null) and the manual ones (id). */
  balloons: {
    id: string | null;
    /** View of the position (null only for files described before an update). */
    view: string | null;
    /** Automatic position whose shelf was moved by hand (setAutoBalloon). */
    moved?: boolean;
    definition: number;
    text: string;
    at: Point;
    target: Point;
  }[];
  /** Manual positions (addBalloon); state null before the first update. */
  positions: {
    id: string;
    view: string;
    /** Shelf point given by the user; null — the default place. */
    at: Point | null;
    /** The shelf is placed by arrangeBalloons together with the automatic positions of the view. */
    arranged: boolean;
    state: 'ok' | 'dangling' | null;
    definition?: number;
    /** Number from the specification. */
    text?: string;
    target?: Point;
    shelf?: Point;
  }[];
  threadNotes: ThreadNoteInfo[];
  name?: string;
  width?: number;
  height?: number;
  computedScale?: ScaleInfo;
  /** Title block as printed: fields from the model where the sheet leaves them empty. */
  titleShown?: { designation: string; name: string; material: string; mass: string; scale: string; sheet: number; sheets: number };
  /** ISO 7200 title block as printed (sheet style iso7200). */
  isoTitleShown?: {
    legalOwner: string; identification: string; title: string; documentType: string; documentStatus: string;
    revision: string; issueDate: string; sheet: number; sheets: number; language: string; creator: string;
    approver: string; department: string; scale: string; projectionSymbol: ProjectionMethod;
  };
}
export type TextFont = 'gost2304' | 'truetype';

export interface DrawingSettingsChanges {
  textFont?: TextFont;
  /** Projected views move to the opposite side of their parent; each keeps its content. */
  projection?: ProjectionMethod;
  sheetStyle?: SheetStyle;
  dissolvedAssemblies?: string[];
}

export interface DrawingDescription {
  model: { kind: 'part' | 'assembly'; path: string; resultHash: string; revision: number; properties: ModelProperties };
  projection: ProjectionMethod;
  sheetStyle: SheetStyle;
  /** Text font of the sheets: single-stroke GOST 2.304 type B or the TrueType font of setFont. */
  textFont: TextFont;
  /** Sub-assemblies listed by their parts in the specification and positions (assembly only). */
  dissolvedAssemblies: string[];
  sheets: SheetInfo[];
  history: HistoryInfo;
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
  balloons: {
    sheet: string;
    view: string;
    placed: { position: number; definition: number }[];
    withoutBalloon: number[];
    /** Parts hidden on this view whose positions are on the nearest section of the sheet. */
    movedTo: { definition: number; view: string }[];
    line: 'auto' | 'right' | 'left' | 'above' | 'below';
    ordered: boolean;
    crossings: number;
    /** Shelves touching dimensions of the sheet (0 — the line of shelves went round them). */
    shelfHits: number;
  }[];
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
  stepPoints?: [number, number, number][];
  /** auxiliary: arrow direction on the parent view, degrees from its x axis (or toward). */
  arrowAngle?: number;
  rotated?: boolean;
  scale?: ScaleRatio | null;
  position?: Point | null;
  hidden?: boolean;
  centerLines?: boolean;
  autoBalloons?: boolean;
  balloonLine?: 'auto' | 'right' | 'left' | 'above' | 'below';
  bearings?: boolean;
  bearingSymbol?: 'diagonals' | 'rollingElement';
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
  setSettings(changes: DrawingSettingsChanges): UpdateReport;
  setSheet(sheet: string, changes: SheetChanges): EditResult;
  addView(sheet: string, spec: ViewSpec): string;
  removeView(sheet: string, view: string): string[];
  moveView(sheet: string, view: string, x: number, y: number): EditResult & { moved: string; group: boolean };
  setView(sheet: string, view: string, changes: Partial<Pick<ViewSpec, 'scale' | 'position' | 'hidden' | 'centerLines' | 'autoBalloons' | 'detailRadius' | 'balloonLine' | 'bearings' | 'bearingSymbol' | 'rotated'>>): EditResult;
  addDimension(sheet: string, view: string, picks: number[], options?: { id?: string; offset?: number; tolerance?: string; leaderAngle?: number }): string;
  setDimension(sheet: string, id: string, changes: { offset?: number; textShift?: number; decimals?: number; tolerance?: string | null; toleranceDisplay?: DimensionInfo['toleranceDisplay']; textOverride?: string | null; prefix?: string; suffix?: string }): EditResult;
  addRoughness(sheet: string, view: string, pick: number, spec?: RoughnessSpec & { id?: string; at?: number }): string;
  addTolerance(sheet: string, view: string, pick: number, spec: { characteristic: GeometricCharacteristic; tolerance: string; datums?: string[]; diameterZone?: boolean; id?: string; at?: number }): string;
  addDatum(sheet: string, view: string, pick: number, letter: string, spec?: { id?: string; at?: number }): string;
  addBalloon(sheet: string, view: string, pick: number, spec?: { id?: string; at?: Point | null }): string;
  setBalloon(sheet: string, id: string, changes: { at: Point | null }): EditResult;
  setAutoBalloon(sheet: string, view: string, definition: number, changes: { at: Point | null }): EditResult;
  removeAnnotation(sheet: string, id: string): EditResult;
  addLocalSection(sheet: string, view: string, spec: { points: Point[]; spline?: boolean; depth: number }): EditResult & { view: string; index: number };
  removeLocalSection(sheet: string, view: string, index: number): EditResult;
  moveDimension(sheet: string, id: string, x: number, y: number): EditResult & { offset: number; textShift: number; leaderAngle: number };
  /** Arranges the dimensions of a view (ГОСТ 2.307); counts of moved dimensions and of what is left to fix by hand. */
  arrangeDimensions(sheet: string, view: string): EditResult & { arranged: number; crossings: number; collisions: number };
  /** Arranges the positions of a sheet (or one view, ГОСТ 2.109); manual positions joined and the result per view. */
  arrangeBalloons(sheet: string, view?: string): EditResult & { arranged: number; views: { view: string; line: string; ordered: boolean; crossings: number; shelfHits: number }[] };
  addDimensionGroup(sheet: string, view: string, picks: number[], options?: { mode?: 'chain' | 'baseline'; direction?: 'horizontal' | 'vertical'; offset?: number; spacing?: number }): string[];
  addThreadNote(sheet: string, view: string, spec: { at: Point; toleranceField?: string; offset?: number; leaderAngle?: number; textShift?: number; id?: string }): EditResult & { id: string; thread: string };
  setThreadNote(sheet: string, id: string, changes: { toleranceField?: string; offset?: number; leaderAngle?: number; textShift?: number }): EditResult;
  undo(): EditResult & { label: string; history: HistoryInfo };
  redo(): EditResult & { label: string; history: HistoryInfo };
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
