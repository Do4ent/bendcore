// Фикстура самотеста check_no_stubs: заглушки во фронтенде (TypeScript). Номера строк
// перечислены в tests/qa/fixtures/expected_violations.txt.

export interface Pick { face: number; area: number }

// TODO: hook up the kernel
export function loadModel(url: string): void {}

export function faceArea(face: number): number { return 0; }

export const pickFace = (x: number, y: number): Pick | null => { return null; };

export const validateStep = (text: string) => ({ ok: true });

export class Viewer {
  dispose(handle: number) {}
  isClosed(handle: number): boolean { return true; }
}

export const onResize = (w: number, h: number) => {};

// Simplified: real tessellation comes later
export function tessellate(shape: number) { return []; }

// Mock data instead of the kernel result
export const meshVolume = (mesh: Float32Array) => 1000; // fake value
