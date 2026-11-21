/**
 * Viewport appearance under three.js >= 0.152.
 *
 * Before r152/r155 three.js did not convert colours (hex values went to the framebuffer as they
 * are) and lights were "legacy": the intensity of ambient/directional lights was divided by PI
 * inside the shader. The viewports were tuned for that look (palette of the design, light 0.45/0.7).
 * To keep the look unchanged with the current three.js:
 *   - colour management is off (colours are used as written, as before);
 *   - the renderer outputs linear values without conversion (LinearSRGBColorSpace);
 *   - light intensities are multiplied by PI (physical units, the old value stays readable).
 * Import this module before any THREE.Color is created.
 */
import * as THREE from 'three';

THREE.ColorManagement.enabled = false;

/** Intensity of a light, in the units the viewports were tuned for before the physical model. */
export function lightIntensity(legacy: number): number {
  return legacy * Math.PI;
}

/** Renderer output without colour-space conversion (the pre-r152 behaviour). */
export function applyLegacyOutput(renderer: THREE.WebGLRenderer): void {
  renderer.outputColorSpace = THREE.LinearSRGBColorSpace;
}
