/**
 * three.js test double for Jest/jsdom.
 *
 * jsdom has no WebGL, so only the GPU-bound parts of three.js are replaced.
 * Everything else (math, cameras, geometries, materials, raycasting, LOD, ...)
 * is the real three.js implementation, so tests exercise the same behaviour
 * as the application at runtime.
 */

// `three` is remapped to this file by jest.config.js moduleNameMapper, so the
// real package has to be loaded by its resolved path, not by its bare name.
const actualThree = jest.requireActual(
  require('path').join(__dirname, '..', '..', 'node_modules', 'three', 'build', 'three.js')
);

class WebGLRenderer {
  constructor(parameters = {}) {
    this.domElement = parameters.canvas || document.createElement('canvas');
    this.shadowMap = { enabled: false, autoUpdate: true, needsUpdate: false, type: actualThree.PCFShadowMap };
    this.outputEncoding = actualThree.LinearEncoding;
    this.outputColorSpace = '';
    this.toneMapping = actualThree.NoToneMapping;
    this.toneMappingExposure = 1;
    this.autoClear = true;
    this.info = {
      memory: { geometries: 0, textures: 0 },
      render: { frame: 0, calls: 0, triangles: 0, points: 0, lines: 0 },
      programs: [],
      autoReset: true,
      reset() {},
    };
    this._pixelRatio = 1;
    this._width = this.domElement.width || 0;
    this._height = this.domElement.height || 0;
    this._clearColor = new actualThree.Color(0x000000);
    this._clearAlpha = 1;
    this._renderTarget = null;
  }
  setSize(width, height, updateStyle = true) {
    this._width = width;
    this._height = height;
    this.domElement.width = Math.floor(width * this._pixelRatio);
    this.domElement.height = Math.floor(height * this._pixelRatio);
    if (updateStyle) {
      this.domElement.style.width = `${width}px`;
      this.domElement.style.height = `${height}px`;
    }
  }
  getSize(target = new actualThree.Vector2()) { return target.set(this._width, this._height); }
  setPixelRatio(value) { this._pixelRatio = value; }
  getPixelRatio() { return this._pixelRatio; }
  setClearColor(color, alpha = 1) { this._clearColor.set(color); this._clearAlpha = alpha; }
  getClearColor(target = new actualThree.Color()) { return target.copy(this._clearColor); }
  getClearAlpha() { return this._clearAlpha; }
  setViewport() {}
  setScissor() {}
  setScissorTest() {}
  render() { this.info.render.frame++; }
  clear() {}
  dispose() {}
  forceContextLoss() {}
  setAnimationLoop() {}
  getRenderTarget() { return this._renderTarget; }
  setRenderTarget(target) { this._renderTarget = target; }
  readRenderTargetPixels() {}
  getContext() { return null; }
  compile() {}
}

module.exports = {
  ...actualThree,
  WebGLRenderer,
};
