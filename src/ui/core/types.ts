/**
 * Типы ответов ядра (команды core_api). Поля совпадают с JSON из src/core_api.cpp.
 */

export type PathItem =
  | { kind: 'straight'; length: number }
  | { kind: 'bend'; radius: number; angle: number; rotation?: number }
  | { kind: 'spiral'; radius: number; pitch: number; turns: number; direction?: 1 | -1 };

export type Feed = 'A' | 'B';
export type Vec3 = [number, number, number];
export type Vec2 = [number, number];

export interface Frame {
  origin?: Vec3;
  x?: Vec3;
  y?: Vec3;
  z?: Vec3;
}

export interface LraRow {
  bend_no: number;
  L: number;
  rotation: number;
  angle: number;
  radius: number;
  is_end: boolean;
}
export interface XyzRow {
  label: string;
  bend_no: number;
  p: Vec3;
  radius: number | null;
}
export interface BendDataResult<R> {
  rows: R[];
  csv: string;
}

/** Класс нарушения (4.5) в порядке убывания тяжести. */
export enum Severity {
  Geometric = 0,
  Technological = 1,
  Warning = 2,
  Mark = 3,
  Info = 4,
}

export interface Violation {
  tube: string;
  element: number;
  severity: Severity;
  class: string;
  rule: string;
  message: string;
  value: number | null;
  limit: number | null;
  other: string;
  at_s: number | null;
}

export interface TubeCheckSummary {
  state: 'passed' | 'violations';
  worst: Severity | null;
  blocks_output: boolean;
  fingerprint: string;
}

export interface CheckResult {
  violations: Violation[];
  tubes: Record<string, TubeCheckSummary>;
  counts: Record<string, number>;
  pairs_tested: number;
}

export interface CheckTubeInput {
  id: string;
  name?: string;
  path: PathItem[];
  od: number;
  B_min?: number;
  B_max?: number;
  L_min?: number;
  N_first?: number;
  N_last?: number;
  E_A?: number;
  E_B?: number;
  cut_allowed?: boolean;
  placement?: Frame;
}

export interface InstanceInfo {
  id: string;
  part: string;
  primary: boolean;
  adaptive: boolean;
  designation: string;
  editable: boolean;
}
export interface PositionInfo {
  number: number;
  designation: string;
  instances: string[];
  quantity: number;
  designations_differ: boolean;
}
export interface InstancesState {
  instances: InstanceInfo[];
  positions: PositionInfo[];
}

// ----- чертёж -----
export type LineStyle = 'contour' | 'thin' | 'axis' | 'cut' | 'hidden';
export interface Geometry {
  lines: Array<{ pts: Vec2[]; style: LineStyle; closed: boolean }>;
  circles: Array<{ c: Vec2; r: number; style: LineStyle }>;
  texts: Array<{ p: Vec2; text: string; h: number; anchor: 'start' | 'middle' | 'end'; frame: boolean; shift: Vec2 }>;
  dims: Array<{ a: Vec2; b: Vec2; offset: number; model_offset: number; text: string; reference: boolean }>;
  hatches: Array<{ outline: Vec2[] }>;
  bbox: { lo: Vec2; hi: Vec2 };
}
export interface Rect {
  x0: number;
  y0: number;
  x1: number;
  y1: number;
}
export interface DrawingTable {
  title: string;
  header: string[];
  rows: string[][];
  note: string;
}
/** Лист чертежа (ответ drawing.part → sheets[]). */
export interface DrawingData {
  number: number;
  count: number;
  standard: 'GOST' | 'ISO' | 'DIN' | 'ANSI';
  projection: 'first' | 'third';
  format: { name: string; w: number; h: number };
  border: Rect;
  title_block: Rect;
  views: Array<{ id: string; kind: string; label: string; scale: number; origin: Vec2; geom: Geometry }>;
  tables: Array<{ table: DrawingTable; top_left: Vec2; width: number }>;
  title: Record<string, string>;
  not_for_production: boolean;
  warnings: string[];
}

export interface DrawingResult {
  sheets: DrawingData[];
}

export interface ReleaseCheckResult {
  blocking: string[];
  warnings: string[];
  can_release: boolean;
  not_for_production: boolean;
  offer_update_and_release: boolean;
  can_unrelease: boolean;
}

// ----- ассоциативность (версионирование) -----
export type AssociativityStatus = 'fresh' | 'stale' | 'broken' | 'not_applicable';

export interface DrawingVersion {
  revision: number;
  fingerprint: string;
  timestamp: string;
  author: string;
  comment: string;
}

export interface AssociativeDrawingState {
  status: AssociativityStatus;
  fingerprint: string;
  last_update_time: string;
  last_updated_by: string;
  change_log: string[];
}

export interface AssociativeDrawing {
  versions: DrawingVersion[];
  state: AssociativeDrawingState;
}

export interface DrawingChanges {
  path_changed: boolean;
  od_changed: boolean;
  wall_changed: boolean;
  preset_changed: boolean;
  format_changed: boolean;
}

// ----- пакетный вывод (БОМ, спецификация) -----
export interface BOMRow {
  position: number;
  designation: string;
  od: number;
  wall: number;
  quantity: number;
  tube_ids: string[];
}

export interface SpecificationRow {
  position: number;
  designation: string;
  description: string;
  material: string;
  quantity: number;
  remark: string;
}

export interface AssemblyDrawingInput {
  tubes: Array<{ id: string; path: PathItem[]; od: number; wall: number; title: Record<string, string> }>;
  standard?: string;
  format?: string;
  tubes_per_unfold_sheet?: number;
  sort_by_position?: boolean;
  group_by_material?: boolean;
}

// ----- PDF экспорт -----
export interface PDFExportOptions {
  filename?: string;
  author?: string;
  subject?: string;
  title?: string;
  include_watermark?: boolean;
  watermark_text?: string;
  dpi?: number;
  landscape?: boolean;
}

export interface PDFExportResult {
  filename: string;
  title: string;
  author: string;
  page_count: number;
  svg_pages: string[];
  status: 'ready_for_client_render' | 'error';
}

// ---------- 2D Sketches (v7.9) ----------
export type SketchElementType = 'line' | 'arc' | 'text' | 'dimension';

export interface SketchElement {
  id: string;
  type: SketchElementType;
  p0_x: number;
  p0_y: number;
  p1_x: number;
  p1_y: number;
  text?: string;
  radius?: number;
  arc_large?: boolean;
  arc_sweep?: boolean;
  text_height?: number;
  is_construction?: boolean;
  style?: LineStyle;
}

export interface SketchDimension {
  id: string;
  p0_x: number;
  p0_y: number;
  p1_x: number;
  p1_y: number;
  offset_x?: number;
  offset_y?: number;
  text: string;
  reference?: boolean;
  driven?: boolean;
}

export interface Sketch2D {
  id: string;
  sheet_number: number;
  name: string;
  elements: SketchElement[];
  dimensions: SketchDimension[];
  visible: boolean;
  locked: boolean;
  created_by: string;
  created_at: string;
  modified_at: string;
}

export interface SketchValidationResult {
  valid: boolean;
  errors?: string[];
  warnings?: string[];
}

export interface AddSketchRequest {
  sheet_number?: number;
  name?: string;
  author?: string;
  elements?: SketchElement[];
  dimensions?: SketchDimension[];
}

export interface AddSketchResult extends SketchValidationResult {
  id: string;
  name: string;
  sheet_number: number;
  element_count: number;
  dimension_count: number;
  created_at: string;
}

export interface SketchBboxResult {
  x0: number;
  y0: number;
  x1: number;
  y1: number;
  width: number;
  height: number;
  empty: boolean;
}
