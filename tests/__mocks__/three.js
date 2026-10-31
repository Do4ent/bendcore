/**
 * Minimal three.js mock for Jest/jsdom.
 * Only the classes and constants used by the application are mocked.
 */

const mockFn = () => jest.fn();

class Vector3 {
  constructor(x = 0, y = 0, z = 0) {
    this.x = x;
    this.y = y;
    this.z = z;
  }
  set(x, y, z) { this.x = x; this.y = y; this.z = z; return this; }
  copy(v) { this.x = v.x; this.y = v.y; this.z = v.z; return this; }
  add(v) { this.x += v.x; this.y += v.y; this.z += v.z; return this; }
  sub(v) { this.x -= v.x; this.y -= v.y; this.z -= v.z; return this; }
  multiplyScalar(s) { this.x *= s; this.y *= s; this.z *= s; return this; }
  divideScalar(s) { return this.multiplyScalar(1 / s); }
  length() { return Math.sqrt(this.x * this.x + this.y * this.y + this.z * this.z); }
  normalize() { return this.divideScalar(this.length() || 1); }
  clone() { return new Vector3(this.x, this.y, this.z); }
  dot(v) { return this.x * v.x + this.y * v.y + this.z * v.z; }
  cross(v) {
    const ax = this.x, ay = this.y, az = this.z;
    this.x = ay * v.z - az * v.y;
    this.y = az * v.x - ax * v.z;
    this.z = ax * v.y - ay * v.x;
    return this;
  }
  distanceTo(v) {
    return Math.sqrt((this.x-v.x)**2 + (this.y-v.y)**2 + (this.z-v.z)**2);
  }
  applyMatrix4() { return this; }
  applyQuaternion() { return this; }
  unproject() { return this; }
  project() { return this; }
  setFromMatrixPosition() { return this; }
  toArray() { return [this.x, this.y, this.z]; }
}

class Vector2 {
  constructor(x = 0, y = 0) { this.x = x; this.y = y; }
  set(x, y) { this.x = x; this.y = y; return this; }
  clone() { return new Vector2(this.x, this.y); }
}

class Color {
  constructor(r = 0, g = 0, b = 0) {
    if (typeof r === 'number' && arguments.length === 1) {
      this.r = ((r >> 16) & 255) / 255;
      this.g = ((r >> 8) & 255) / 255;
      this.b = (r & 255) / 255;
    } else {
      this.r = r; this.g = g; this.b = b;
    }
  }
  set() { return this; }
  setHex() { return this; }
  getHex() { return 0; }
  clone() { return new Color(this.r, this.g, this.b); }
}

class Quaternion {
  constructor(x = 0, y = 0, z = 0, w = 1) { this.x = x; this.y = y; this.z = z; this.w = w; }
  set(x, y, z, w) { this.x=x; this.y=y; this.z=z; this.w=w; return this; }
  setFromAxisAngle() { return this; }
  setFromEuler() { return this; }
  multiply() { return this; }
  clone() { return new Quaternion(this.x, this.y, this.z, this.w); }
  copy(q) { this.x=q.x; this.y=q.y; this.z=q.z; this.w=q.w; return this; }
  invert() { return this; }
  normalize() { return this; }
  slerp() { return this; }
}

class Euler {
  constructor(x = 0, y = 0, z = 0, order = 'XYZ') { this.x=x; this.y=y; this.z=z; this.order=order; }
  set(x, y, z) { this.x=x; this.y=y; this.z=z; return this; }
  clone() { return new Euler(this.x, this.y, this.z, this.order); }
}

class Matrix4 {
  constructor() { this.elements = new Float32Array(16); this.identity(); }
  identity() {
    const e = this.elements;
    e[0]=1; e[4]=0; e[8]=0;  e[12]=0;
    e[1]=0; e[5]=1; e[9]=0;  e[13]=0;
    e[2]=0; e[6]=0; e[10]=1; e[14]=0;
    e[3]=0; e[7]=0; e[11]=0; e[15]=1;
    return this;
  }
  copy(m) { this.elements.set(m.elements); return this; }
  clone() { return new Matrix4().copy(this); }
  multiplyMatrices() { return this; }
  setPosition() { return this; }
  getInverse() { return this; }
  invert() { return this; }
  lookAt() { return this; }
  makePerspective() { return this; }
  makeOrthographic() { return this; }
  multiply() { return this; }
  premultiply() { return this; }
  makeRotationFromQuaternion() { return this; }
  decompose() { return this; }
  compose() { return this; }
  extractRotation() { return this; }
}

class Box3 {
  constructor() {
    this.min = new Vector3(Infinity, Infinity, Infinity);
    this.max = new Vector3(-Infinity, -Infinity, -Infinity);
  }
  makeEmpty() { return this; }
  expandByObject() { return this; }
  expandByPoint() { return this; }
  getCenter(target) { return target || new Vector3(); }
  getSize(target) { return target || new Vector3(); }
  isEmpty() { return true; }
}

class Sphere {
  constructor(center = new Vector3(), radius = 0) { this.center = center; this.radius = radius; }
}

class Ray {
  constructor() { this.origin = new Vector3(); this.direction = new Vector3(); }
  intersectBox() { return null; }
  intersectSphere() { return null; }
}

class Raycaster {
  constructor() {
    this.ray = new Ray();
    this.near = 0;
    this.far = Infinity;
  }
  setFromCamera() {}
  intersectObjects() { return []; }
  intersectObject() { return []; }
}

const makeEventDispatcher = (Base) => {
  return class extends Base {
    addEventListener() {}
    removeEventListener() {}
    dispatchEvent() {}
    hasEventListener() { return false; }
  };
};

class EventDispatcher {
  addEventListener() {}
  removeEventListener() {}
  dispatchEvent() {}
  hasEventListener() { return false; }
}

class Object3D extends EventDispatcher {
  constructor() {
    super();
    this.uuid = Math.random().toString(36);
    this.name = '';
    this.type = 'Object3D';
    this.parent = null;
    this.children = [];
    this.position = new Vector3();
    this.rotation = new Euler();
    this.quaternion = new Quaternion();
    this.scale = new Vector3(1, 1, 1);
    this.matrix = new Matrix4();
    this.matrixWorld = new Matrix4();
    this.visible = true;
    this.castShadow = false;
    this.receiveShadow = false;
    this.userData = {};
    this.layers = { mask: 1, set: () => {}, enable: () => {}, disable: () => {}, test: () => true };
    this.renderOrder = 0;
  }
  add(...objects) { objects.forEach(o => { if (o) { o.parent = this; this.children.push(o); } }); return this; }
  remove(...objects) { objects.forEach(o => { const i = this.children.indexOf(o); if (i > -1) this.children.splice(i, 1); }); return this; }
  clear() { this.children = []; return this; }
  traverse(fn) { fn(this); this.children.forEach(c => c.traverse && c.traverse(fn)); }
  traverseVisible(fn) { if (this.visible) { fn(this); this.children.forEach(c => c.traverseVisible && c.traverseVisible(fn)); } }
  updateMatrix() {}
  updateMatrixWorld() {}
  getWorldPosition(target) { return target || new Vector3(); }
  getWorldDirection(target) { return target || new Vector3(); }
  getWorldQuaternion(target) { return target || new Quaternion(); }
  lookAt() {}
  clone() { return new Object3D(); }
  copy() { return this; }
  raycast() {}
  updateWorldMatrix() {}
}

class Scene extends Object3D {
  constructor() {
    super();
    this.type = 'Scene';
    this.background = null;
    this.fog = null;
    this.autoUpdate = true;
    this.overrideMaterial = null;
  }
}

class Camera extends Object3D {
  constructor() {
    super();
    this.type = 'Camera';
    this.matrixWorldInverse = new Matrix4();
    this.projectionMatrix = new Matrix4();
    this.projectionMatrixInverse = new Matrix4();
  }
  updateProjectionMatrix() {}
  clone() { return new Camera(); }
}

class PerspectiveCamera extends Camera {
  constructor(fov = 50, aspect = 1, near = 0.1, far = 2000) {
    super();
    this.type = 'PerspectiveCamera';
    this.fov = fov;
    this.aspect = aspect;
    this.near = near;
    this.far = far;
    this.zoom = 1;
  }
  updateProjectionMatrix() {}
  clone() { return new PerspectiveCamera(this.fov, this.aspect, this.near, this.far); }
}

class OrthographicCamera extends Camera {
  constructor(left = -1, right = 1, top = 1, bottom = -1, near = 0.1, far = 2000) {
    super();
    this.left = left; this.right = right; this.top = top; this.bottom = bottom;
    this.near = near; this.far = far;
    this.zoom = 1;
  }
  updateProjectionMatrix() {}
}

class BufferAttribute {
  constructor(array, itemSize) {
    this.array = array;
    this.itemSize = itemSize;
    this.count = array ? Math.floor(array.length / itemSize) : 0;
    this.needsUpdate = false;
  }
  setXYZ(index, x, y, z) {
    const i = index * this.itemSize;
    this.array[i] = x;
    this.array[i + 1] = y;
    if (this.itemSize > 2) this.array[i + 2] = z;
    return this;
  }
  setX(index, x) { this.array[index * this.itemSize] = x; return this; }
  setY(index, y) { this.array[index * this.itemSize + 1] = y; return this; }
  getX(index) { return this.array[index * this.itemSize]; }
  getY(index) { return this.array[index * this.itemSize + 1]; }
  getZ(index) { return this.array[index * this.itemSize + 2]; }
  clone() { return new BufferAttribute(this.array.slice(), this.itemSize); }
}

class Float32BufferAttribute extends BufferAttribute {
  constructor(array, itemSize) {
    super(array instanceof Float32Array ? array : new Float32Array(array), itemSize);
  }
}

class BufferGeometry extends EventDispatcher {
  constructor() {
    super();
    this.uuid = Math.random().toString(36);
    this.attributes = {};
    this.index = null;
    this.groups = [];
    this.boundingBox = null;
    this.boundingSphere = null;
    this.drawRange = { start: 0, count: Infinity };
  }
  setAttribute(name, attr) { this.attributes[name] = attr; return this; }
  getAttribute(name) { return this.attributes[name]; }
  setIndex(index) { this.index = index; return this; }
  setFromPoints(points) {
    const positions = new Float32Array(points.length * 3);
    points.forEach((p, i) => { positions[i*3]=p.x; positions[i*3+1]=p.y; positions[i*3+2]=p.z; });
    this.setAttribute('position', new BufferAttribute(positions, 3));
    return this;
  }
  computeVertexNormals() {}
  computeBoundingBox() { this.boundingBox = new Box3(); }
  computeBoundingSphere() { this.boundingSphere = new Sphere(); }
  dispose() {}
  clone() { return new BufferGeometry(); }
  copy() { return this; }
  addGroup() {}
}

class SphereGeometry extends BufferGeometry {}
class CylinderGeometry extends BufferGeometry {}
class BoxGeometry extends BufferGeometry {}
class PlaneGeometry extends BufferGeometry {}
class TubeGeometry extends BufferGeometry {}
class ConeGeometry extends BufferGeometry {}
class CircleGeometry extends BufferGeometry {}
class TorusGeometry extends BufferGeometry {}
class EdgesGeometry extends BufferGeometry {}
class WireframeGeometry extends BufferGeometry {}

class Material extends EventDispatcher {
  constructor() {
    super();
    this.uuid = Math.random().toString(36);
    this.type = 'Material';
    this.transparent = false;
    this.opacity = 1;
    this.visible = true;
    this.side = 0;
    this.needsUpdate = false;
    this.depthWrite = true;
    this.depthTest = true;
    this.wireframe = false;
  }
  dispose() {}
  clone() { return new Material(); }
  copy() { return this; }
}

class MeshBasicMaterial extends Material {
  constructor(params = {}) {
    super();
    this.color = params.color !== undefined ? new Color(params.color) : new Color(0xffffff);
    this.wireframe = params.wireframe || false;
    this.transparent = params.transparent || false;
    this.opacity = params.opacity !== undefined ? params.opacity : 1;
  }
}

class MeshStandardMaterial extends Material {
  constructor(params = {}) {
    super();
    this.color = params.color !== undefined ? new Color(params.color) : new Color(0xffffff);
    this.roughness = params.roughness !== undefined ? params.roughness : 0.5;
    this.metalness = params.metalness !== undefined ? params.metalness : 0;
    this.emissive = new Color(0);
    this.emissiveIntensity = 1;
  }
}

class MeshPhongMaterial extends Material {
  constructor(params = {}) {
    super();
    this.color = params.color !== undefined ? new Color(params.color) : new Color(0xffffff);
    this.emissive = new Color(0);
    this.specular = new Color(0x111111);
    this.shininess = params.shininess || 30;
  }
}

class LineBasicMaterial extends Material {
  constructor(params = {}) {
    super();
    this.color = params.color !== undefined ? new Color(params.color) : new Color(0xffffff);
    this.linewidth = params.linewidth || 1;
  }
}

class PointsMaterial extends Material {
  constructor(params = {}) {
    super();
    this.color = params.color !== undefined ? new Color(params.color) : new Color(0xffffff);
    this.size = params.size || 1;
    this.sizeAttenuation = params.sizeAttenuation !== undefined ? params.sizeAttenuation : true;
  }
}

class SpriteMaterial extends Material {
  constructor(params = {}) {
    super();
    this.color = params.color !== undefined ? new Color(params.color) : new Color(0xffffff);
    this.map = params.map || null;
  }
}

class Mesh extends Object3D {
  constructor(geometry = new BufferGeometry(), material = new MeshBasicMaterial()) {
    super();
    this.type = 'Mesh';
    this.geometry = geometry;
    this.material = material;
  }
  raycast() {}
  clone() { return new Mesh(this.geometry, this.material); }
}

class Line extends Object3D {
  constructor(geometry = new BufferGeometry(), material = new LineBasicMaterial()) {
    super();
    this.type = 'Line';
    this.geometry = geometry;
    this.material = material;
  }
  raycast() {}
  computeLineDistances() { return this; }
}

class LineSegments extends Line {
  constructor(geometry, material) {
    super(geometry, material);
    this.type = 'LineSegments';
  }
}

class Points extends Object3D {
  constructor(geometry = new BufferGeometry(), material = new PointsMaterial()) {
    super();
    this.type = 'Points';
    this.geometry = geometry;
    this.material = material;
  }
  raycast() {}
}

class Sprite extends Object3D {
  constructor(material = new SpriteMaterial()) {
    super();
    this.type = 'Sprite';
    this.material = material;
    this.center = new Vector2(0.5, 0.5);
  }
}

class Group extends Object3D {
  constructor() {
    super();
    this.type = 'Group';
  }
}

class WebGLRenderer {
  constructor() {
    this.domElement = document.createElement('canvas');
    this.shadowMap = { enabled: false, type: 0 };
    this.outputColorSpace = '';
    this.toneMapping = 0;
    this.toneMappingExposure = 1;
    this.info = { memory: {}, render: {} };
  }
  setSize() {}
  setPixelRatio() {}
  render() {}
  dispose() {}
  clear() {}
  setClearColor() {}
  setViewport() {}
  getSize(target) { return target || { width: 800, height: 600 }; }
  getPixelRatio() { return 1; }
  getRenderTarget() { return null; }
  setRenderTarget() {}
  readRenderTargetPixels() {}
  getContext() { return null; }
}

class DirectionalLight extends Object3D {
  constructor(color = 0xffffff, intensity = 1) {
    super();
    this.type = 'DirectionalLight';
    this.color = new Color(color);
    this.intensity = intensity;
    this.target = new Object3D();
    this.shadow = { camera: new OrthographicCamera(), mapSize: { width: 512, height: 512 } };
  }
}

class AmbientLight extends Object3D {
  constructor(color = 0xffffff, intensity = 1) {
    super();
    this.type = 'AmbientLight';
    this.color = new Color(color);
    this.intensity = intensity;
  }
}

class PointLight extends Object3D {
  constructor(color = 0xffffff, intensity = 1, distance = 0) {
    super();
    this.type = 'PointLight';
    this.color = new Color(color);
    this.intensity = intensity;
    this.distance = distance;
  }
}

class HemisphereLight extends Object3D {
  constructor(sky = 0xffffff, ground = 0x000000, intensity = 1) {
    super();
    this.type = 'HemisphereLight';
    this.color = new Color(sky);
    this.groundColor = new Color(ground);
    this.intensity = intensity;
  }
}

class GridHelper extends Object3D {
  constructor() { super(); this.type = 'GridHelper'; }
}

class AxesHelper extends Object3D {
  constructor() { super(); this.type = 'AxesHelper'; }
}

class ArrowHelper extends Object3D {
  constructor() { super(); this.type = 'ArrowHelper'; }
}

class CatmullRomCurve3 {
  constructor(points = []) { this.points = points; this.closed = false; this.type = 'catmullrom'; }
  getPoints(divisions = 12) {
    return this.points.length ? this.points : [];
  }
  getSpacedPoints(divisions = 12) {
    return this.points.length ? this.points : [];
  }
  getPoint(t) { return new Vector3(); }
}

class CurvePath {
  constructor() { this.curves = []; }
  add(curve) { this.curves.push(curve); }
  getPoints(divisions = 12) { return []; }
}

class ShapeGeometry extends BufferGeometry {}
class Shape {
  constructor() { this.curves = []; }
  moveTo() {}
  lineTo() {}
  quadraticCurveTo() {}
  bezierCurveTo() {}
  closePath() {}
}

class CanvasTexture {
  constructor() { this.needsUpdate = false; }
  dispose() {}
}

class Texture {
  constructor() { this.needsUpdate = false; }
  dispose() {}
}

class Clock {
  constructor(autoStart = true) {
    this.startTime = 0;
    this.oldTime = 0;
    this.elapsedTime = 0;
    this.running = false;
    if (autoStart) this.start();
  }
  start() { this.startTime = Date.now(); this.oldTime = this.startTime; this.running = true; }
  stop() { this.running = false; }
  getElapsedTime() { return this.elapsedTime; }
  getDelta() { return 0.016; }
}

// Constants
const DoubleSide = 2;
const FrontSide = 0;
const BackSide = 1;
const AdditiveBlending = 2;
const NormalBlending = 1;
const SRGBColorSpace = 'srgb';
const LinearSRGBColorSpace = 'srgb-linear';
const PCFSoftShadowMap = 2;
const LinearToneMapping = 1;
const ACESFilmicToneMapping = 4;
const RepeatWrapping = 1000;
const ClampToEdgeWrapping = 1001;
const MeshNormalMaterial = class extends Material {};

module.exports = {
  Vector3,
  Vector2,
  Color,
  Quaternion,
  Euler,
  Matrix4,
  Box3,
  Sphere,
  Ray,
  Raycaster,
  EventDispatcher,
  Object3D,
  Scene,
  Camera,
  PerspectiveCamera,
  OrthographicCamera,
  BufferAttribute,
  Float32BufferAttribute,
  BufferGeometry,
  SphereGeometry,
  CylinderGeometry,
  BoxGeometry,
  PlaneGeometry,
  TubeGeometry,
  ConeGeometry,
  CircleGeometry,
  TorusGeometry,
  EdgesGeometry,
  WireframeGeometry,
  ShapeGeometry,
  Shape,
  Material,
  MeshBasicMaterial,
  MeshStandardMaterial,
  MeshPhongMaterial,
  MeshNormalMaterial,
  LineBasicMaterial,
  PointsMaterial,
  SpriteMaterial,
  Mesh,
  Line,
  LineSegments,
  Points,
  Sprite,
  Group,
  WebGLRenderer,
  DirectionalLight,
  AmbientLight,
  PointLight,
  HemisphereLight,
  GridHelper,
  AxesHelper,
  ArrowHelper,
  CatmullRomCurve3,
  CurvePath,
  CanvasTexture,
  Texture,
  Clock,
  DoubleSide,
  FrontSide,
  BackSide,
  AdditiveBlending,
  NormalBlending,
  SRGBColorSpace,
  LinearSRGBColorSpace,
  PCFSoftShadowMap,
  LinearToneMapping,
  ACESFilmicToneMapping,
  RepeatWrapping,
  ClampToEdgeWrapping,
};
