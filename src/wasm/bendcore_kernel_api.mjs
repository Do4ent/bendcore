// JS API of the bendcore_kernel_wasm module (docs/ui/KERNEL_VIEWPORT.md).
//
//   import createBendcoreKernel from './bendcore_kernel.mjs';
//   import { loadKernel } from './bendcore_kernel_api.mjs';
//   const kernel = await loadKernel(createBendcoreKernel);
//   const shape = kernel.readStep(bytes);          // Uint8Array -> KernelShape
//   try { shape.volumeProperties().volume; shape.tessellate({ linearDeflection: 0.1 }); }
//   finally { shape.dispose(); }
//
// Every kernel failure is thrown as KernelError { code, message }; code is the kernel ErrorCode
// (InvalidInput, InvalidShape, OperationFailed, ParseError, IoError, Unsupported) or
// "Disposed" for a call on a shape that was already disposed.

export class KernelError extends Error {
  constructor(code, message) {
    super(`${code}: ${message}`);
    this.name = 'KernelError';
    this.code = code;
    this.detail = message;
  }
}

function unwrap(result) {
  if (!result.ok) throw new KernelError(result.code, result.message);
  return result.value;
}

export class KernelShape {
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
    if (this.#handle === 0) throw new KernelError('Disposed', `${name}() called on a disposed shape`);
    return unwrap(this.#module[name](this.#handle, ...args));
  }

  /** Releases the kernel shape. Further calls throw KernelError('Disposed'). Idempotent. */
  dispose() {
    if (this.#handle === 0) return;
    const handle = this.#handle;
    this.#handle = 0;
    unwrap(this.#module.disposeShape(handle));
  }

  counts() {
    return this.#call('counts');
  }

  volumeProperties() {
    return this.#call('volumeProperties');
  }

  surfaceProperties() {
    return this.#call('surfaceProperties');
  }

  boundingBox() {
    return this.#call('boundingBox');
  }

  checkValidity() {
    return this.#call('checkValidity');
  }

  faceTypes() {
    return this.#call('faceTypes');
  }

  edgeTypes() {
    return this.#call('edgeTypes');
  }

  face(index) {
    if (!Number.isInteger(index) || index < 0) {
      throw new KernelError('InvalidInput', `face index must be a non-negative integer, got ${index}`);
    }
    return this.#call('faceInfo', index);
  }

  tessellate(params = {}) {
    return this.#call('tessellate', params);
  }
}

export class Kernel {
  #module;

  constructor(module) {
    this.#module = module;
  }

  /** { occtVersion, liveShapes } */
  info() {
    return this.#module.kernelInfo();
  }

  readStep(bytes) {
    if (!(bytes instanceof Uint8Array)) {
      throw new KernelError('InvalidInput', 'readStep expects a Uint8Array');
    }
    return new KernelShape(this.#module, unwrap(this.#module.readStep(bytes)));
  }
}

/** Instantiates the Emscripten module (factory = default export of bendcore_kernel.mjs). */
export async function loadKernel(factory, moduleOptions = {}) {
  const module = await factory(moduleOptions);
  return new Kernel(module);
}

/**
 * Face of a triangle: binary search over faceSpans (Uint32Array of triples
 * faceIndex, firstIndex, indexCount, ordered by firstIndex and contiguous).
 * Returns the faceIndex, or -1 when the triangle is outside every span.
 */
export function faceOfTriangle(faceSpans, triangle) {
  const index = triangle * 3;
  let lo = 0;
  let hi = faceSpans.length / 3 - 1;
  while (lo <= hi) {
    const mid = (lo + hi) >> 1;
    const first = faceSpans[mid * 3 + 1];
    const count = faceSpans[mid * 3 + 2];
    if (index < first) hi = mid - 1;
    else if (index >= first + count) lo = mid + 1;
    else return faceSpans[mid * 3];
  }
  return -1;
}
