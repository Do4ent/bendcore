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

  /** JSON "bendcore-part" v3; includeCache stores BRep results keyed by input hashes. */
  save(options = {}) {
    return this.#call('docSave', options);
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

  createDocument() {
    return new PartDocumentHandle(this.#module, unwrap(this.#module.docCreate()));
  }

  /** Loads (and migrates) a document file; returns { document, report }. */
  loadDocument(json) {
    if (typeof json !== 'string') throw new KernelError('InvalidInput', 'loadDocument expects a JSON string');
    const loaded = unwrap(this.#module.docLoad(json));
    return { document: new PartDocumentHandle(this.#module, loaded.handle), report: loaded.report };
  }
}

/** Instantiates the Emscripten module (factory = default export of bendcore_modeling.mjs). */
export async function loadModeling(factory, moduleOptions = {}) {
  const module = await factory(moduleOptions);
  return new ModelingKernel(module);
}
