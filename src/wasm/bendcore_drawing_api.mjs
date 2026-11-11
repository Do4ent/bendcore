// JS API of the bendcore_drawing_wasm module: the drawing document in the browser and in node
// (docs/ui/DRAWING_UI.md).
//
//   import createBendcoreDrawing from './bendcore_drawing.mjs';
//   import { loadDrawingKernel } from './bendcore_drawing_api.mjs';
//   const kernel = await loadDrawingKernel(createBendcoreDrawing);
//   kernel.setFont(fontBytes, 'DejaVuSans.ttf');        // Uint8Array of a TrueType font
//   const drawing = kernel.createFromPart(partJson, { properties: { designation: 'РД.10.001' } });
//   try {
//     drawing.addView('S1', { kind: 'base', standard: 'front' });
//     drawing.addView('S1', { kind: 'projected', standard: 'top' });
//     const picture = drawing.sheetPicture('S1');       // SVG text + view edges for picking
//     const pdf = drawing.exportPdf();                   // Uint8Array, starts with %PDF
//   } finally { drawing.dispose(); }
//
// The module exchanges JSON text; this wrapper parses it. Every failure is thrown as
// KernelError { code, detail } (bendcore_kernel_api.mjs): code is the kernel ErrorCode
// (InvalidInput, InvalidShape, OperationFailed, ParseError, IoError, Unsupported) or "Disposed"
// for a call on a disposed drawing.

import { KernelError } from './bendcore_kernel_api.mjs';

export { KernelError };

function parse(text) {
  const result = JSON.parse(text);
  if (!result.ok) throw new KernelError(result.code, result.message);
  return result.value;
}

function bytes(result) {
  if (!result.ok) throw new KernelError(result.code, result.message);
  return result.value;
}

const text = (value) => (value === undefined ? '' : JSON.stringify(value));

export class DrawingHandle {
  #module;
  #handle;

  constructor(module, handle) {
    this.#module = module;
    this.#handle = handle;
  }

  /** Registry handle inside the module; 0 after dispose(). */
  get handle() {
    return this.#handle;
  }

  get disposed() {
    return this.#handle === 0;
  }

  #check(name) {
    if (this.#handle === 0) throw new KernelError('Disposed', `${name}() called on a disposed drawing`);
  }

  #call(name, ...args) {
    this.#check(name);
    return parse(this.#module[name](this.#handle, ...args));
  }

  /** Releases the drawing, its model and caches. Idempotent. */
  dispose() {
    if (this.#handle === 0) return;
    const handle = this.#handle;
    this.#handle = 0;
    parse(this.#module.dispose(handle));
  }

  /** Model link, sheets with their views, annotations and computed values. */
  describe() {
    return this.#call('describe');
  }

  /** Drawing document JSON ("bendcore-drawing", version 2). */
  save() {
    return this.#call('save');
  }

  /** JSON of the model document the drawing is made from (with the current variable values). */
  saveModel() {
    return this.#call('modelSave');
  }

  /** Part variables (a part model only). */
  variables() {
    return this.#call('modelVariables');
  }

  /** Sets a part variable and rebuilds the part; returns what an update would change. */
  setVariable(name, expression) {
    return this.#call('modelSetVariable', name, String(expression));
  }

  /** Part properties for the title block: designation, name, material, density (kg/m³). */
  setModelProperties(properties) {
    return this.#call('setModelProperties', text(properties));
  }

  /** Whether the model changed since the last update; views and dimensions to recompute. */
  checkModel() {
    return this.#call('checkModel');
  }

  /** Updates the drawing from the current model: report of changed and dangling dimensions. */
  updateFromModel() {
    return this.#call('updateFromModel');
  }

  addSheet(options = {}) {
    return this.#call('addSheet', text(options)).id;
  }

  removeSheet(sheet) {
    return this.#call('removeSheet', sheet);
  }

  /** Format, orientation, scale, title fields, technical requirements, roughness remainder, parts list. */
  setSheet(sheet, changes) {
    return this.#call('setSheet', sheet, text(changes));
  }

  /** Adds a view; returns its id. */
  addView(sheet, spec) {
    return this.#call('addView', sheet, text(spec)).id;
  }

  /** Removes the view with the views and annotations that depend on it; returns removed ids. */
  removeView(sheet, view) {
    return this.#call('removeView', sheet, view).removed;
  }

  /** Puts the centre of the view at (x, y) sheet mm; a view in projection relation moves its group. */
  moveView(sheet, view, x, y) {
    return this.#call('moveView', sheet, view, x, y);
  }

  setView(sheet, view, changes) {
    return this.#call('setView', sheet, view, text(changes));
  }

  /** Dimension from one or two picked view edges (indices of sheetPicture().views[].edges). */
  addDimension(sheet, view, picks, options = {}) {
    return this.#call('addDimension', sheet, view, text(picks), text(options)).id;
  }

  setDimension(sheet, id, changes) {
    return this.#call('setDimension', sheet, id, text(changes));
  }

  /** Roughness sign on a picked straight edge. */
  addRoughness(sheet, view, pick, spec = {}) {
    return this.#call('addSymbol', sheet, view, 'roughness', text(pick), text(spec)).id;
  }

  /** Form or location tolerance frame on a picked straight edge. */
  addTolerance(sheet, view, pick, spec) {
    return this.#call('addSymbol', sheet, view, 'tolerance', text(pick), text(spec)).id;
  }

  /** Datum symbol on a picked straight edge. */
  addDatum(sheet, view, pick, letter, spec = {}) {
    return this.#call('addSymbol', sheet, view, 'datum', text(pick), text({ ...spec, letter })).id;
  }

  removeAnnotation(sheet, id) {
    return this.#call('removeAnnotation', sheet, id);
  }

  /** Sheet as SVG text (1 unit = 1 mm, y down) and the view edges in sheet mm (y up) for picking. */
  sheetPicture(sheet) {
    return this.#call('sheetPicture', sheet);
  }

  /** Specification (GOST 2.106) of an assembly drawing. */
  specification() {
    return this.#call('specification');
  }

  /** PDF of all sheets (and the specification sheets of an assembly). */
  exportPdf(options = {}) {
    this.#check('exportPdf');
    return bytes(this.#module.exportPdf(this.#handle, text(options)));
  }

  /** One sheet as 'pdf', 'dxf' or 'svg' bytes. */
  exportSheet(sheet, format) {
    this.#check('exportSheet');
    return bytes(this.#module.exportSheet(this.#handle, sheet, format));
  }
}

export class DrawingKernel {
  #module;

  constructor(module) {
    this.#module = module;
  }

  info() {
    return parse(this.#module.drawingInfo());
  }

  /** TrueType font for all text (metrics, PDF embedding); bytes of the .ttf file. */
  setFont(fontBytes, source = 'font.ttf') {
    if (!(fontBytes instanceof Uint8Array)) throw new KernelError('InvalidInput', 'setFont() expects a Uint8Array');
    return parse(this.#module.setFont(fontBytes, source));
  }

  #create(kind, modelJson, options) {
    return new DrawingHandle(this.#module, parse(this.#module.createDrawing(kind, modelJson, text(options))).handle);
  }

  /** New drawing (one empty sheet) of a part document JSON. */
  createFromPart(partJson, options = {}) {
    return this.#create('part', partJson, options);
  }

  /** New drawing of an assembly document JSON with embedded part geometry. */
  createFromAssembly(assemblyJson, options = {}) {
    return this.#create('assembly', assemblyJson, options);
  }

  /** Opens a saved drawing with the JSON of its model; returns the drawing and the update report. */
  load(drawingJson, modelJson) {
    const value = parse(this.#module.loadDrawing(drawingJson, modelJson));
    return { drawing: new DrawingHandle(this.#module, value.handle), report: value.report };
  }
}

export async function loadDrawingKernel(factory, moduleOptions = {}) {
  const module = await factory(moduleOptions);
  return new DrawingKernel(module);
}
