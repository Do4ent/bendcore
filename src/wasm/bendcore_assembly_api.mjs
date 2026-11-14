// JS API of the bendcore_assembly_wasm module: the assembly document in the browser and in node
// (docs/ui/ASSEMBLY_UI.md).
//
//   import createBendcoreAssembly from './bendcore_assembly.mjs';
//   import { loadAssembly } from './bendcore_assembly_api.mjs';
//   const assembly = await loadAssembly(createBendcoreAssembly);
//   const doc = assembly.createDocument('Unit');
//   try {
//     const block = doc.insertPart({ name: 'Block', format: 'part-json', data: blockJson, fixed: true });
//     const shaft = doc.insertPart({ name: 'Shaft', format: 'part-json', data: shaftJson });
//     doc.addMate({ type: 'concentric', a: { path: [shaft.occurrence], kind: 'face', index: 0 },
//                   b: { path: [block.occurrence], kind: 'face', index: 6 } });
//     doc.state().solve.totalDof;
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

export class AssemblyDocumentHandle {
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
    if (this.#handle === 0) throw new KernelError('Disposed', `${name}() called on a disposed assembly`);
    return unwrap(this.#module[name](this.#handle, ...args));
  }

  /** Releases the document, its history and its cached meshes. Idempotent. */
  dispose() {
    if (this.#handle === 0) return;
    const handle = this.#handle;
    this.#handle = 0;
    unwrap(this.#module.asmDispose(handle));
  }

  /** Definitions, occurrence tree with DOF, mates with status, patterns, views, solve report, history. */
  state() {
    return this.#call('asmState');
  }

  /** JSON "bendcore-assembly"; embedGeometry stores part BReps in the file (default true). */
  save(options = {}) {
    return this.#call('asmSave', options);
  }

  /** STEP text (AP214 by default, { schema: 'AP242' }): structure, names, colours, placements. */
  exportStep(options = {}) {
    return this.#call('asmExportStep', options);
  }

  /** File bytes: format 'step' | 'iges' | 'stl' | '3mf' (docs/ui/DATA_EXCHANGE.md). */
  exportFile(format, options = {}) {
    return this.#call('asmExport', format, options);
  }

  /** { volume, area, parts, partsWithoutDensity, mass: MassProperties | null }. */
  massProperties() {
    return this.#call('asmMassProperties');
  }

  /** { name, format: 'part-json' | 'step', data, placement, properties, fixed, parent } → { definition, occurrence } */
  insertPart(spec) {
    return this.#call('asmInsertPart', spec);
  }

  /** Another occurrence of an existing definition; returns the occurrence id. */
  insertOccurrence(spec) {
    return this.#call('asmInsertOccurrence', spec);
  }

  /** External subassembly from a source registered with AssemblyKernel.registerSource. */
  insertSubassembly(spec) {
    return this.#call('asmInsertSubassembly', spec);
  }

  removeOccurrence(id) {
    this.#call('asmRemoveOccurrence', id);
  }

  /** { fixed, suppressed, name, designation, placement, color } — one history step. */
  setOccurrence(id, changes) {
    this.#call('asmSetOccurrence', id, changes);
  }

  /** { designation, name, material, density (kg/m^3, null clears) } of a definition. */
  setProperties(definition, properties) {
    this.#call('asmSetProperties', definition, properties);
  }

  /** { type, a, b, value (mm; angle in degrees), alignment, internal, name } → mate id */
  addMate(spec) {
    return this.#call('asmAddMate', spec);
  }

  editMate(id, spec) {
    this.#call('asmEditMate', id, spec);
  }

  removeMate(id) {
    this.#call('asmRemoveMate', id);
  }

  setMateSuppressed(id, suppressed) {
    this.#call('asmSetMateSuppressed', id, Boolean(suppressed));
  }

  /** Solves the mates as a history step; returns the solve report. */
  solve() {
    return this.#call('asmSolve');
  }

  undo() {
    this.#call('asmUndo');
  }

  redo() {
    this.#call('asmRedo');
  }

  /** Grabs a top-level occurrence at a world point; returns { localPoint }. */
  dragBegin(occurrence, point) {
    return this.#call('asmDragBegin', occurrence, point);
  }

  /** Re-solves with the grabbed point pulled to target: { solve, placement, grabbed, placements }. */
  dragMove(target) {
    return this.#call('asmDragMove', target);
  }

  /** Commits the drag as one history step. */
  dragEnd() {
    this.#call('asmDragEnd');
  }

  dragCancel() {
    this.#call('asmDragCancel');
  }

  /** { kind: 'linear' | 'circular' | 'feature', seed, ... } → pattern id */
  addPattern(spec) {
    return this.#call('asmAddPattern', spec);
  }

  removePattern(id) {
    this.#call('asmRemovePattern', id);
  }

  /** { name, steps: [{ occurrences, direction, distance }] } → view index */
  addExplodedView(view) {
    return this.#call('asmAddExplodedView', view);
  }

  removeExplodedView(index) {
    this.#call('asmRemoveExplodedView', index);
  }

  /** { meshes: { key: KernelMesh }, occurrences } — meshes in part coordinates, world matrices per occurrence. */
  tessellate(options = {}) {
    return this.#call('asmTessellate', options);
  }

  /** World placements of all placed parts ({ view, progress } for an exploded view). */
  placements(options = {}) {
    return this.#call('asmPlacements', options);
  }

  /** { path, kind, index, geometry, name, entity } of an element of a placed part. */
  element(ref) {
    return this.#call('asmElement', ref);
  }

  /** { parts, pairs: [{ a, b, kind, volume, distance }], totalPairs, candidatePairs, totalVolume } */
  interference(options = {}) {
    return this.#call('asmInterference', options);
  }

  /** 'flat' | 'structured' → { rows, totalMass, totalVolume, totalParts } */
  bom(mode = 'flat') {
    return this.#call('asmBom', mode);
  }

  /** Holes of a part for a fastener: { part, insertion, radius } → sites. */
  findHoles(spec) {
    return this.#call('asmFindHoles', spec);
  }

  /** Bolt/screw stack in holes: { part, insertion, radius, holes, screw, headWashers, nutWashers, nut }. */
  insertFasteners(spec) {
    return this.#call('asmInsertFasteners', spec);
  }
}

export class AssemblyKernel {
  #module;

  constructor(module) {
    this.#module = module;
  }

  /** { occtVersion, liveDocuments, cachedMeshes, sources, formatVersion } */
  info() {
    return this.#module.assemblyInfo();
  }

  /** Fastener standards: { id, designation, kind, gost, counterpart, sizes, verified, source, verification } */
  fastenerCatalog() {
    return unwrap(this.#module.fastenerCatalog());
  }

  /** Model data of one fastener { standard, size, length }: designation, volume, mass, thread, dimensions. */
  describeFastener(spec) {
    return unwrap(this.#module.fastenerDescribe(spec));
  }

  /** Text of an external subassembly document by its source name (insertSubassembly, loadDocument). */
  registerSource(name, text) {
    if (typeof name !== 'string' || typeof text !== 'string') {
      throw new KernelError('InvalidInput', 'registerSource expects (name, text) strings');
    }
    unwrap(this.#module.asmRegisterSource(name, text));
  }

  createDocument(name = 'Assembly') {
    return new AssemblyDocumentHandle(this.#module, unwrap(this.#module.asmCreate(name)));
  }

  /** Loads an assembly file; returns { document, report }. */
  loadDocument(json) {
    if (typeof json !== 'string') throw new KernelError('InvalidInput', 'loadDocument expects a JSON string');
    const loaded = unwrap(this.#module.asmLoad(json));
    return { document: new AssemblyDocumentHandle(this.#module, loaded.handle), report: loaded.report };
  }

  /** New document from a STEP assembly (structure, names, colours, placements). */
  importStep(text) {
    if (typeof text !== 'string') throw new KernelError('InvalidInput', 'importStep expects STEP text');
    return new AssemblyDocumentHandle(this.#module, unwrap(this.#module.asmImportStep(text)));
  }

  /** New document from IGES (XCAF): one part per solid, names and colours of the file. */
  importIges(text) {
    if (typeof text !== 'string') throw new KernelError('InvalidInput', 'importIges expects IGES text');
    return new AssemblyDocumentHandle(this.#module, unwrap(this.#module.asmImportIges(text)));
  }
}

/** Instantiates the Emscripten module (factory = default export of bendcore_assembly.mjs). */
export async function loadAssembly(factory, moduleOptions = {}) {
  const module = await factory(moduleOptions);
  return new AssemblyKernel(module);
}
