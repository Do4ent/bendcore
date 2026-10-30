// CSS Modules
declare module '*.module.css' {
  const classes: { [key: string]: string };
  export default classes;
}

declare module '*.css' {
  const classes: { [key: string]: string };
  export default classes;
}

// Three.js type stubs (minimal, for projects without @types/three)
declare module 'three' {
  export class Vector3 {
    x: number;
    y: number;
    z: number;
    constructor(x?: number, y?: number, z?: number);
    set(x: number, y: number, z: number): this;
    clone(): Vector3;
    copy(v: Vector3): this;
    add(v: Vector3): this;
    sub(v: Vector3): this;
    multiplyScalar(s: number): this;
    normalize(): this;
    length(): number;
    dot(v: Vector3): number;
    cross(v: Vector3): this;
    crossVectors(a: Vector3, b: Vector3): this;
    applyMatrix4(m: Matrix4): this;
    distanceTo(v: Vector3): number;
    divideScalar(s: number): this;
    project(camera: Camera): this;
    unproject(camera: Camera): this;
  }

  export class Matrix4 {
    elements: number[];
    constructor();
    identity(): this;
    multiply(m: Matrix4): this;
  }

  export class BufferGeometry {
    constructor();
    attributes: Record<string, BufferAttribute>;
    boundingBox: Box3 | null;
    setAttribute(name: string, attribute: BufferAttribute): this;
    setIndex(index: number[] | BufferAttribute): this;
    setFromPoints(points: Vector3[]): this;
    computeBoundingBox(): void;
    computeVertexNormals(): void;
    dispose(): void;
  }

  export class BufferAttribute {
    array: ArrayLike<number>;
    itemSize: number;
    count: number;
    constructor(array: ArrayLike<number>, itemSize: number);
    getX(index: number): number;
    getY(index: number): number;
    getZ(index: number): number;
  }

  export class Float32BufferAttribute extends BufferAttribute {
    constructor(array: ArrayLike<number> | number, itemSize: number);
  }

  export class Material {
    dispose(): void;
  }

  export class LineBasicMaterial extends Material {
    constructor(params?: { color?: number | string; linewidth?: number });
  }

  export class MeshPhongMaterial extends Material {
    emissive: Color;
    shininess: number;
    constructor(params?: { color?: number | string; transparent?: boolean; opacity?: number; side?: number; emissive?: number | string; shininess?: number });
  }

  export class MeshBasicMaterial extends Material {
    constructor(params?: { color?: number | string; map?: Texture; transparent?: boolean });
  }

  export class Texture {
    needsUpdate: boolean;
    dispose(): void;
  }

  export class CanvasTexture extends Texture {
    constructor(canvas: HTMLCanvasElement);
  }

  export class Object3D {
    name: string;
    visible: boolean;
    position: Vector3;
    rotation: { x: number; y: number; z: number };
    scale: Vector3;
    children: Object3D[];
    userData: Record<string, any>;
    add(...objects: Object3D[]): this;
    remove(...objects: Object3D[]): this;
    traverse(callback: (obj: Object3D) => void): void;
    lookAt(vector: Vector3): void;
    lookAt(x: number, y: number, z: number): void;
    rotateX(angle: number): this;
    rotateY(angle: number): this;
    rotateZ(angle: number): this;
    clone(): this;
    copy(source: Object3D): this;
    clear(): this;
  }

  export class Group extends Object3D {
    constructor();
  }

  export class Mesh extends Object3D {
    constructor(geometry?: BufferGeometry, material?: Material);
    geometry: BufferGeometry;
    material: Material;
    matrixWorld: Matrix4;
    castShadow: boolean;
    receiveShadow: boolean;
  }

  export class LineSegments extends Object3D {
    constructor(geometry?: BufferGeometry, material?: Material);
    geometry: BufferGeometry;
    material: Material;
  }

  export class Line extends Object3D {
    constructor(geometry?: BufferGeometry, material?: Material);
    geometry: BufferGeometry;
    material: Material;
  }

  export class Scene extends Object3D {
    constructor();
    background: Color | null;
  }

  export class Color {
    constructor(color?: number | string);
    r: number;
    g: number;
    b: number;
  }

  export class Camera extends Object3D {}

  export class PerspectiveCamera extends Camera {
    constructor(fov?: number, aspect?: number, near?: number, far?: number);
    fov: number;
    aspect: number;
    near: number;
    far: number;
    updateProjectionMatrix(): void;
    getWorldDirection(target: Vector3): Vector3;
    updateMatrixWorld(force?: boolean): void;
  }

  export class Box3 {
    min: Vector3;
    max: Vector3;
    constructor();
    setFromObject(object: Object3D): this;
    getCenter(target: Vector3): Vector3;
    getSize(target: Vector3): Vector3;
    isEmpty(): boolean;
    union(box: Box3): this;
    expandByObject(object: Object3D): this;
  }

  export class WebGLRenderer {
    constructor(params?: { canvas?: HTMLCanvasElement; antialias?: boolean; alpha?: boolean });
    domElement: HTMLCanvasElement;
    shadowMap: { enabled: boolean; type: number };
    setSize(width: number, height: number): void;
    setPixelRatio(ratio: number): void;
    render(scene: Scene, camera: Camera): void;
    dispose(): void;
  }

  export class GridHelper extends Object3D {
    constructor(size?: number, divisions?: number, colorCenterLine?: number | string, colorGrid?: number | string);
  }

  export class AxesHelper extends Object3D {
    constructor(size?: number);
  }

  export class Plane {
    constructor(normal?: Vector3, constant?: number);
  }

  export class Ray {
    origin: Vector3;
    direction: Vector3;
    intersectPlane(plane: Plane, target: Vector3): Vector3 | null;
  }

  export class BoxGeometry extends BufferGeometry {
    constructor(width?: number, height?: number, depth?: number);
  }

  export class PlaneGeometry extends BufferGeometry {
    constructor(width?: number, height?: number);
  }

  export class SphereGeometry extends BufferGeometry {
    constructor(radius?: number, widthSegments?: number, heightSegments?: number);
  }

  export class CylinderGeometry extends BufferGeometry {
    constructor(radiusTop?: number, radiusBottom?: number, height?: number, radialSegments?: number);
  }

  export class AmbientLight extends Object3D {
    constructor(color?: number | string, intensity?: number);
  }

  export class DirectionalLight extends Object3D {
    constructor(color?: number | string, intensity?: number);
    castShadow: boolean;
    shadow: { mapSize: { width: number; height: number }; camera: { near: number; far: number } };
  }

  export class Raycaster {
    constructor();
    ray: Ray;
    setFromCamera(coords: { x: number; y: number }, camera: Camera): void;
    intersectObjects(objects: Object3D[], recursive?: boolean): Array<{ object: Object3D; point: Vector3; distance: number }>;
  }

  export class Vector2 {
    x: number;
    y: number;
    constructor(x?: number, y?: number);
  }

  export const FrontSide: number;
  export const BackSide: number;
  export const DoubleSide: number;
  export const PCFSoftShadowMap: number;

  export class Quaternion {
    x: number;
    y: number;
    z: number;
    w: number;
    constructor(x?: number, y?: number, z?: number, w?: number);
    setFromEuler(euler: { x: number; y: number; z: number }): this;
    multiply(q: Quaternion): this;
    clone(): Quaternion;
  }

  export class LOD extends Object3D {
    constructor();
    addLevel(object: Object3D, distance: number): this;
    update(camera: Camera): void;
  }
}
