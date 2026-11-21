// JS API of the bendcore_modeling_wasm module: the part document in the browser and in node
// (docs/ui/PART_MODELING_UI.md).
//
//   import createBendcoreModeling from './bendcore_modeling.mjs';
//   import { loadModeling } from './bendcore_modeling_api.mjs';
//   const modeling = await loadModeling(createBendcoreModeling);
//   const doc = modeling.createDocument();
//   try {
//     doc.setVariable('W', '40');
//     doc.addFeature({ type: 'Sketch', id: 'Profile', options: { plane: 'XY' },
//                      sketch: modeling.sketchTemplate({ profiles: [{ kind: 'rectangle', x: 0, y: 0, width: 'W', height: 20 }] }) });
//     doc.addFeature({ type: 'Extrude', id: 'Body', parameters: { distance: 10 }, links: { sketch: 'Profile' } });
//     doc.rebuild();
//     doc.properties('final').volume;   // 8000
//   } finally { doc.dispose(); }
//
// Every failure is thrown as KernelError { code, detail } (bendcore_kernel_api.mjs); code is the
// kernel ErrorCode (InvalidInput, InvalidShape, OperationFailed, ParseError, IoError,
// Unsupported, Fault) or "Disposed" for a call on a disposed document.

import { KernelError, faceOfTriangle } from './bendcore_kernel_api.mjs';

export { KernelError, faceOfTriangle };

function unwrap(result) {
  if (!result.ok) throw new KernelError(result.code, result.message);
  return result.value;
}

export class PartDocumentHandle {
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

  #call(name, ...args) {
    if (this.#handle === 0) throw new KernelError('Disposed', `${name}() called on a disposed document`);
    return unwrap(this.#module[name](this.#handle, ...args));
  }

  /** Releases the document and its shapes. Idempotent. */
  dispose() {
    if (this.#handle === 0) return;
    const handle = this.#handle;
    this.#handle = 0;
    unwrap(this.#module.docDispose(handle));
  }

  /**
   * JSON "bendcore-part" v3; includeCache stores BRep results keyed by input hashes.
   * options.material = { id, density } is written as the top-level field "material" (the part's
   * material from the kernel library, docs/ui/PART_MODELING_UI.md); the v3 reader ignores it.
   */
  save(options = {}) {
    const { material, ...kernelOptions } = options;
    const text = this.#call('docSave', kernelOptions);
    if (material === undefined || material === null) return text;
    if (typeof material.id !== 'string' || !(Number.isFinite(material.density) && material.density > 0)) {
      throw new KernelError('InvalidInput', 'save: material must be { id: string, density: kg/m3 > 0 }');
    }
    const doc = JSON.parse(text);
    doc.material = { id: material.id, density: material.density };
    return JSON.stringify(doc);
  }

  /**
   * Configuration table (docs/modeling/PART_DOCUMENT.md, "Таблица параметров"): { columns: [{ kind:
   * 'variable' | 'suppression' | 'material', target, title }], rows: [{ designation, fullDesignation,
   * name, cells }], active, material }.
   */
  configurations() {
    return this.#call('docConfigurations');
  }

  /** Replaces the table { columns, rows, active } (checked against the document; one undo entry). */
  setConfigurations(table) {
    this.#call('docSetConfigurations', table);
  }

  setConfigurationCell(row, column, value) {
    this.#call('docSetConfigurationCell', row, column, value);
  }

  /** Adds a column { kind, target }; every row takes the current base value. */
  addConfigurationColumn(column) {
    this.#call('docAddConfigurationColumn', column);
  }

  /** Makes a configuration active ('' — base values); call rebuild() afterwards. */
  activateConfiguration(designation) {
    this.#call('docActivateConfiguration', designation);
  }

  /** Builds every configuration: [{ designation, fullDesignation, ok, problems, volume }]. */
  checkConfigurations() {
    return this.#call('docCheckConfigurations');
  }

  /** CSV text of the table; options { decimal: '.' | ',', bom: boolean }. */
  exportConfigurationsCsv(options = {}) {
    return this.#call('docExportConfigurationsCsv', options);
  }

  /** Replaces the table by a CSV file (';' separator, decimal point or comma). */
  importConfigurationsCsv(text) {
    this.#call('docImportConfigurationsCsv', text);
  }

  variables() {
    return this.#call('docVariables');
  }

  setVariable(name, expression) {
    this.#call('docSetVariable', name, expression);
  }

  removeVariable(name) {
    this.#call('docRemoveVariable', name);
  }

  /** Nodes in tree order with state, messages, expressions, values, references, links. */
  features() {
    return this.#call('docFeatures');
  }

  feature(id) {
    const f = this.features().find((x) => x.id === id);
    if (!f) throw new KernelError('InvalidInput', `no feature '${id}'`);
    return f;
  }

  /** { size, rollback, canUndo, canRedo, inTransaction, undoHistory, redoHistory } */
  state() {
    return this.#call('docState');
  }

  /** Adds a node (takes effect at rebuild()); returns its id. position: index or null. */
  addFeature(spec, position = null) {
    return this.#call('docAddFeature', spec, position);
  }

  /** Several edits of one node as one undo entry: { label, parameters, options, links, references, sketch, suppressed }. */
  editFeature(id, edits) {
    this.#call('docEditFeature', id, edits);
  }

  removeFeature(id) {
    this.#call('docRemoveFeature', id);
  }

  moveFeature(id, index) {
    this.#call('docMoveFeature', id, index);
  }

  setSuppressed(id, suppressed) {
    this.#call('docSetSuppressed', id, suppressed);
  }

  /** Features at index >= position are rolled back; null removes the marker. */
  setRollback(position) {
    this.#call('docSetRollback', position);
  }

  beginTransaction(name) {
    this.#call('docBeginTransaction', name);
  }

  commitTransaction() {
    this.#call('docCommitTransaction');
  }

  /** Restores the state at beginTransaction (call rebuild() afterwards). */
  abortTransaction() {
    this.#call('docAbortTransaction');
  }

  undo() {
    return this.#call('docUndo');
  }

  redo() {
    return this.#call('docRedo');
  }

  /** { recomputed, reused, fromCache, failed } */
  rebuild() {
    return this.#call('docRebuild');
  }

  /** Reference { name, kind, signature } to element `index` of `kind` in the target shape. */
  makeReference(target, kind, index) {
    return this.#call('docMakeReference', target, kind, index);
  }

  /** Element record { index, kind, name, origin, feature, aliases, signature }. */
  element(target, kind, index) {
    return this.#call('docElement', target, kind, index);
  }

  /** Mesh of the target shape (same layout as KernelShape.tessellate). */
  tessellate(target = 'final', params = {}) {
    return this.#call('docTessellate', target, params);
  }

  /** { counts, volume, centerOfMass, mass, area, boundingBox }; density in kg/m^3, mass in kg. */
  properties(target = 'final', density = null) {
    return this.#call('docProperties', target, density);
  }

  /** File bytes of a target body: format 'step' | 'iges' | 'stl' | '3mf' (docs/ui/DATA_EXCHANGE.md). */
  exportFile(format, options = {}, target = 'final') {
    return this.#call('docExport', target, format, options);
  }

  /** Mass, centre of mass, inertia tensor and principal axes for a density in kg/m³. */
  massProperties(density, target = 'final') {
    return this.#call('docMassProperties', target, density);
  }

  /**
   * Measurement between elements of the target (kernel indices): spec { type: 'distance' | 'angle'
   * | 'length' | 'area' | 'radius', a: { kind, index }, b: { kind, index } } -> { type, unit, value,
   * points } (docs/kernel/ANALYSIS.md, section 4). Distances in mm, angles in degrees.
   */
  measure(spec, target = 'final') {
    return this.#call('docMeasure', target, spec);
  }

  /** Section of the target by the plane { origin, normal }: { area, polylines: Float32Array[] }. */
  section(plane, target = 'final', deflection = 0.05) {
    return this.#call('docSection', target, plane, deflection);
  }

  /**
   * Solves sketch data in the context of sketch node `featureId` (variables, placement, projected
   * edges) without changing the document (docs/ui/SKETCH_EDITOR.md): solved points and curves,
   * intersections, dof, definition, conflicting/redundant constraints, profile regions and the
   * data moved to the solution. options.drag = { point, x, y } pulls a free point.
   */
  solveSketch(featureId, sketch, options = {}) {
    return this.#call('docSketchSolve', featureId, sketch, options);
  }

  /**
   * Applies a sketch editing tool (trim, extend, offset, mirror, linear-pattern,
   * circular-pattern, fillet, chamfer) to sketch data; returns { sketch, created, modified,
   * removed, points, constraints }. The document is not changed.
   */
  editSketch(featureId, sketch, tool) {
    return this.#call('docSketchEdit', featureId, sketch, tool);
  }
}

/** The part's material saved next to the v3 document, or null (absent or malformed). */
function savedMaterial(json) {
  try {
    const m = JSON.parse(json).material;
    if (m && typeof m.id === 'string' && Number.isFinite(m.density) && m.density > 0) return { id: m.id, density: m.density };
  } catch {
    // The kernel has already accepted the text as a document; a missing field is no error.
  }
  return null;
}

export class ModelingKernel {
  #module;

  constructor(module) {
    this.#module = module;
  }

  /** { occtVersion, liveDocuments, formatVersion } */
  info() {
    return this.#module.modelingInfo();
  }

  /** Feature types with their parameters, reference slots, options and link slots. */
  catalog() {
    return unwrap(this.#module.featureCatalog());
  }

  /** Sketch data from profile templates (rectangle, circle, polygon, point). */
  sketchTemplate(spec) {
    return unwrap(this.#module.sketchTemplate(spec));
  }

  /**
   * DXF file (Uint8Array or string) as sketch data (docs/ui/SKETCH_EDITOR.md, "Импорт DXF"):
   * lines, arcs, circles and polylines with bulges of the chosen layers, in millimetres by
   * $INSUNITS (or options.units), end points within options.tolerance (mm, default 0.001) merged.
   * options: { layers?, units?, defaultUnits?, tolerance?, fixed?, paperSpace?, into? } - into: sketch data the
   * imported geometry is appended to. Returns the sketch, the layers, counts and diagnostics.
   */
  dxfSketch(bytes, options = {}) {
    return unwrap(this.#module.dxfSketch(bytes, options));
  }

  createDocument() {
    return new PartDocumentHandle(this.#module, unwrap(this.#module.docCreate()));
  }

  /** Built-in material library: [{ id, name, category, density (kg/m3), color, source, verified, ... }]. */
  materials() {
    return unwrap(this.#module.materialLibrary());
  }

  /**
   * Loads (and migrates) a document file; returns { document, report, material } where material is
   * the { id, density } saved with save({ material }) or null.
   */
  loadDocument(json) {
    if (typeof json !== 'string') throw new KernelError('InvalidInput', 'loadDocument expects a JSON string');
    const loaded = unwrap(this.#module.docLoad(json));
    return { document: new PartDocumentHandle(this.#module, loaded.handle), report: loaded.report, material: savedMaterial(json) };
  }
}

/** Instantiates the Emscripten module (factory = default export of bendcore_modeling.mjs). */
export async function loadModeling(factory, moduleOptions = {}) {
  const module = await factory(moduleOptions);
  return new ModelingKernel(module);
}
