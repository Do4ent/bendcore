/**
 * Snap Constraints System for BendCore 3D Viewport
 * Manages visual snap indicators and snap point detection
 */

import * as THREE from 'three';

export enum SnapMode {
  ENDPOINT = 'endpoint',
  CENTER = 'center',
  GRID = 'grid',
  AXIS = 'axis',
  FACE = 'face',
  EDGE = 'edge',
}

export interface SnapPoint {
  position: { x: number; y: number; z: number };
  type: SnapMode;
  normal?: { x: number; y: number; z: number };
  distance: number;
}

/** Point in sketch/world coordinates; z is optional for 2D sketches. */
export interface SnapCoordinates {
  x: number;
  y: number;
  z?: number;
}

/** Vertex that points can snap to. */
export interface SnapVertex {
  id: string;
  position: SnapCoordinates;
}

/** Result of snapping a point, for drawing snap feedback. */
export interface SnapFeedback {
  original: SnapCoordinates;
  snapped: SnapCoordinates;
  isSnapped: boolean;
  type: SnapMode | null;
  /** Id of the vertex snapped to, for vertex snaps */
  targetId?: string;
  /** Distance between the original and snapped point */
  distance: number;
}

/** Snap mode names accepted by isSnapEnabled (in addition to SnapMode values). */
export type SnapModeName = SnapMode | 'grid' | 'vertex';

export interface SnapConstraint {
  position: { x: number; y: number; z: number };
  type: SnapMode;
  visual: THREE.Object3D;
}

/**
 * SnapConstraints: Manages snap points and visual feedback
 */
export class SnapConstraints {
  private snapMode: Set<SnapMode>;
  private snapRadius: number = 8;  // pixels
  private snapPoints: SnapPoint[] = [];
  private activeSnaps: SnapConstraint[] = [];
  private snapGroup: THREE.Group;
  private snapIndicators: Map<string, THREE.Object3D> = new Map();
  private isEnabled: boolean = true;
  private gridSize: number = 1;  // world units, for coordinate snapping
  private vertexSnapTolerance: number = 0.5;  // world units
  private snapVertices: SnapVertex[] = [];

  // Visual styles
  private readonly SNAP_POINT_SIZE = 0.5;
  private readonly SNAP_LINE_LENGTH = 2;
  private readonly SNAP_COLOR = 0x00ff00;  // Green
  private readonly SNAP_AXIS_COLOR_X = 0xff0000;  // Red
  private readonly SNAP_AXIS_COLOR_Y = 0x00ff00;  // Green
  private readonly SNAP_AXIS_COLOR_Z = 0x0000ff;  // Blue

  constructor() {
    this.snapMode = new Set([SnapMode.GRID, SnapMode.ENDPOINT]);
    this.snapGroup = new THREE.Group();
    this.snapGroup.name = 'SnapIndicators';
  }

  /**
   * Get snap group (add to scene)
   */
  getSnapGroup(): THREE.Group {
    return this.snapGroup;
  }

  /**
   * Enable/disable snap
   */
  setEnabled(enabled: boolean): void {
    this.isEnabled = enabled;
    this.snapGroup.visible = enabled;
  }

  /**
   * Check if snapping is enabled, optionally for a specific mode
   * ('vertex' is an alias for SnapMode.ENDPOINT).
   */
  isSnapEnabled(mode?: SnapModeName): boolean {
    if (mode === undefined) {
      return this.isEnabled;
    }
    return this.isEnabled && this.snapMode.has(SnapConstraints.resolveMode(mode));
  }

  private static resolveMode(mode: SnapModeName): SnapMode {
    if (mode === 'vertex') return SnapMode.ENDPOINT;
    if (mode === 'grid') return SnapMode.GRID;
    return mode;
  }

  /**
   * Enable/disable snapping to the grid
   */
  setSnapToGrid(enabled: boolean): void {
    if (enabled) {
      this.snapMode.add(SnapMode.GRID);
    } else {
      this.snapMode.delete(SnapMode.GRID);
    }
  }

  /**
   * Set grid spacing used for coordinate snapping (world units)
   */
  setGridSize(size: number): void {
    if (!(size > 0) || !Number.isFinite(size)) {
      throw new Error(`Grid size must be a positive number, got ${size}`);
    }
    this.gridSize = size;
  }

  getGridSize(): number {
    return this.gridSize;
  }

  /**
   * Enable/disable snapping to vertices, optionally replacing the vertex set
   */
  setSnapToVertices(enabled: boolean, vertices?: SnapVertex[]): void {
    if (vertices) {
      this.snapVertices = vertices.map((v) => ({ id: v.id, position: { ...v.position } }));
    }
    if (enabled) {
      this.snapMode.add(SnapMode.ENDPOINT);
    } else {
      this.snapMode.delete(SnapMode.ENDPOINT);
    }
  }

  /**
   * Set the maximum distance (world units) at which a point snaps to a vertex
   */
  setVertexSnapTolerance(tolerance: number): void {
    this.vertexSnapTolerance = Math.max(0, tolerance);
  }

  /**
   * Snap a point: to the nearest vertex within tolerance first, otherwise to
   * the grid. Returns the point unchanged when nothing applies.
   */
  snapPoint(point: SnapCoordinates): SnapCoordinates {
    return this.getSnapFeedback(point).snapped;
  }

  /**
   * Snap a point and describe the result for visual feedback
   */
  getSnapFeedback(point: SnapCoordinates): SnapFeedback {
    const original = { ...point };
    const unsnapped: SnapFeedback = {
      original,
      snapped: { ...point },
      isSnapped: false,
      type: null,
      distance: 0,
    };
    if (!this.isEnabled) {
      return unsnapped;
    }

    if (this.snapMode.has(SnapMode.ENDPOINT) && this.snapVertices.length > 0) {
      let best: { vertex: SnapVertex; distance: number } | null = null;
      for (const vertex of this.snapVertices) {
        const distance = SnapConstraints.distance(point, vertex.position);
        if (distance <= this.vertexSnapTolerance && (!best || distance < best.distance)) {
          best = { vertex, distance };
        }
      }
      if (best) {
        const snapped: SnapCoordinates = { x: best.vertex.position.x, y: best.vertex.position.y };
        if (point.z !== undefined || best.vertex.position.z !== undefined) {
          snapped.z = best.vertex.position.z ?? 0;
        }
        return {
          original,
          snapped,
          isSnapped: true,
          type: SnapMode.ENDPOINT,
          targetId: best.vertex.id,
          distance: best.distance,
        };
      }
    }

    if (this.snapMode.has(SnapMode.GRID)) {
      const round = (v: number) => Math.round(v / this.gridSize) * this.gridSize;
      const snapped: SnapCoordinates = { x: round(point.x), y: round(point.y) };
      if (point.z !== undefined) {
        snapped.z = round(point.z);
      }
      return {
        original,
        snapped,
        isSnapped: true,
        type: SnapMode.GRID,
        distance: SnapConstraints.distance(point, snapped),
      };
    }

    return unsnapped;
  }

  private static distance(a: SnapCoordinates, b: SnapCoordinates): number {
    return Math.hypot(a.x - b.x, a.y - b.y, (a.z ?? 0) - (b.z ?? 0));
  }

  /**
   * Set snap modes
   */
  setSnapModes(modes: SnapMode[]): void {
    this.snapMode.clear();
    modes.forEach((mode) => this.snapMode.add(mode));
  }

  /**
   * Get active snap modes
   */
  getSnapModes(): SnapMode[] {
    return Array.from(this.snapMode);
  }

  /**
   * Toggle snap mode
   */
  toggleSnapMode(mode: SnapMode): void {
    if (this.snapMode.has(mode)) {
      this.snapMode.delete(mode);
    } else {
      this.snapMode.add(mode);
    }
  }

  /**
   * Set snap radius in pixels
   */
  setSnapRadius(pixels: number): void {
    this.snapRadius = Math.max(1, pixels);
  }

  /**
   * Get snap radius
   */
  getSnapRadius(): number {
    return this.snapRadius;
  }

  /**
   * Detect snap points near cursor
   */
  detectSnapPoints(
    cursorX: number,
    cursorY: number,
    scene: THREE.Scene,
    camera: THREE.Camera
  ): SnapPoint[] {
    if (!this.isEnabled) {
      return [];
    }

    const detectedPoints: SnapPoint[] = [];

    // Sample snap points based on enabled modes
    const gridPoints = this.snapMode.has(SnapMode.GRID)
      ? this.generateGridSnapPoints()
      : [];
    const axisPoints = this.snapMode.has(SnapMode.AXIS)
      ? this.generateAxisSnapPoints()
      : [];
    const endpointPoints = this.snapMode.has(SnapMode.ENDPOINT)
      ? this.generateEndpointSnapPoints(scene)
      : [];
    const centerPoints = this.snapMode.has(SnapMode.CENTER)
      ? this.generateCenterSnapPoints(scene)
      : [];

    const allPoints = [
      ...gridPoints,
      ...axisPoints,
      ...endpointPoints,
      ...centerPoints,
    ];

    // Filter points near cursor
    allPoints.forEach((point) => {
      const screenPos = this.worldToScreen(point.position, camera);
      const distance = Math.hypot(
        screenPos.x - cursorX,
        screenPos.y - cursorY
      );

      if (distance < this.snapRadius) {
        detectedPoints.push(point);
      }
    });

    // Sort by distance
    detectedPoints.sort((a, b) => a.distance - b.distance);

    return detectedPoints.slice(0, 1);  // Return closest snap point
  }

  /**
   * Visualize snap point
   */
  visualizeSnapPoint(point: SnapPoint): void {
    // Clear existing snaps
    this.clearSnapIndicators();

    const key = `snap-${Date.now()}`;

    // Create visual indicator based on snap type
    let visual: THREE.Object3D | null = null;

    if (point.type === SnapMode.ENDPOINT) {
      visual = this.createSnapPointVisual(point.position);
    } else if (point.type === SnapMode.CENTER) {
      visual = this.createSnapPointVisual(point.position, true);
    } else if (point.type === SnapMode.GRID) {
      visual = this.createGridSnapVisual(point.position);
    } else if (point.type === SnapMode.AXIS) {
      visual = this.createAxisSnapVisual(point.position, point.normal);
    }

    if (visual) {
      this.snapGroup.add(visual);
      this.snapIndicators.set(key, visual);
    }

    this.activeSnaps = [{ position: point.position, type: point.type, visual: visual! }];
  }

  /**
   * Clear snap indicators
   */
  clearSnapIndicators(): void {
    this.snapIndicators.forEach((visual) => {
      this.snapGroup.remove(visual);
    });
    this.snapIndicators.clear();
    this.activeSnaps = [];
  }

  /**
   * Get active snaps
   */
  getActiveSnaps(): SnapConstraint[] {
    return this.activeSnaps;
  }

  /**
   * Generate grid snap points
   */
  private generateGridSnapPoints(): SnapPoint[] {
    const points: SnapPoint[] = [];
    const gridSize = 10;

    for (let x = -gridSize; x <= gridSize; x += 5) {
      for (let y = -gridSize; y <= gridSize; y += 5) {
        points.push({
          position: { x, y, z: 0 },
          type: SnapMode.GRID,
          distance: 0,
        });
      }
    }

    return points;
  }

  /**
   * Generate axis snap points
   */
  private generateAxisSnapPoints(): SnapPoint[] {
    const points: SnapPoint[] = [];

    // X axis
    for (let x = -20; x <= 20; x += 5) {
      points.push({
        position: { x, y: 0, z: 0 },
        type: SnapMode.AXIS,
        normal: { x: 1, y: 0, z: 0 },
        distance: 0,
      });
    }

    // Y axis
    for (let y = -20; y <= 20; y += 5) {
      points.push({
        position: { x: 0, y, z: 0 },
        type: SnapMode.AXIS,
        normal: { x: 0, y: 1, z: 0 },
        distance: 0,
      });
    }

    // Z axis
    for (let z = -20; z <= 20; z += 5) {
      points.push({
        position: { x: 0, y: 0, z },
        type: SnapMode.AXIS,
        normal: { x: 0, y: 0, z: 1 },
        distance: 0,
      });
    }

    return points;
  }

  /**
   * Generate endpoint snap points from scene objects
   */
  private generateEndpointSnapPoints(scene: THREE.Scene): SnapPoint[] {
    const points: SnapPoint[] = [];

    // Scan scene for geometries with vertices
    scene.traverse((object) => {
      if (object instanceof THREE.Mesh && object.geometry) {
        const positions = object.geometry.attributes.position;
        if (positions) {
          const positionArray = positions.array as Float32Array;
          for (let i = 0; i < positionArray.length; i += 3) {
            const pos = new THREE.Vector3(
              positionArray[i],
              positionArray[i + 1],
              positionArray[i + 2]
            );
            pos.applyMatrix4(object.matrixWorld);

            points.push({
              position: { x: pos.x, y: pos.y, z: pos.z },
              type: SnapMode.ENDPOINT,
              distance: 0,
            });
          }
        }
      }
    });

    return points;
  }

  /**
   * Generate center snap points
   */
  private generateCenterSnapPoints(scene: THREE.Scene): SnapPoint[] {
    const points: SnapPoint[] = [];

    scene.traverse((object) => {
      if (object instanceof THREE.Mesh && object.geometry) {
        object.geometry.computeBoundingBox();
        if (object.geometry.boundingBox) {
          const center = object.geometry.boundingBox.getCenter(new THREE.Vector3());
          center.applyMatrix4(object.matrixWorld);

          points.push({
            position: { x: center.x, y: center.y, z: center.z },
            type: SnapMode.CENTER,
            distance: 0,
          });
        }
      }
    });

    return points;
  }

  /**
   * Create snap point visual indicator
   */
  private createSnapPointVisual(
    position: { x: number; y: number; z: number },
    isCenter: boolean = false
  ): THREE.Object3D {
    const group = new THREE.Group();

    // Crosshair
    const lines = new THREE.LineSegments(
      new THREE.BufferGeometry().setFromPoints([
        new THREE.Vector3(-this.SNAP_POINT_SIZE, 0, 0),
        new THREE.Vector3(this.SNAP_POINT_SIZE, 0, 0),
        new THREE.Vector3(0, -this.SNAP_POINT_SIZE, 0),
        new THREE.Vector3(0, this.SNAP_POINT_SIZE, 0),
        new THREE.Vector3(0, 0, -this.SNAP_POINT_SIZE),
        new THREE.Vector3(0, 0, this.SNAP_POINT_SIZE),
      ]),
      new THREE.LineBasicMaterial({
        color: this.SNAP_COLOR,
        linewidth: 2,
      })
    );

    group.add(lines);
    group.position.set(position.x, position.y, position.z);

    return group;
  }

  /**
   * Create grid snap visual
   */
  private createGridSnapVisual(position: { x: number; y: number; z: number }): THREE.Object3D {
    const group = new THREE.Group();

    const geometry = new THREE.BufferGeometry().setFromPoints([
      new THREE.Vector3(-this.SNAP_LINE_LENGTH / 2, 0, 0),
      new THREE.Vector3(this.SNAP_LINE_LENGTH / 2, 0, 0),
      new THREE.Vector3(0, -this.SNAP_LINE_LENGTH / 2, 0),
      new THREE.Vector3(0, this.SNAP_LINE_LENGTH / 2, 0),
    ]);

    const lines = new THREE.LineSegments(
      geometry,
      new THREE.LineBasicMaterial({ color: this.SNAP_COLOR })
    );

    group.add(lines);
    group.position.set(position.x, position.y, position.z);

    return group;
  }

  /**
   * Create axis snap visual
   */
  private createAxisSnapVisual(
    position: { x: number; y: number; z: number },
    normal?: { x: number; y: number; z: number }
  ): THREE.Object3D {
    const group = new THREE.Group();

    const length = this.SNAP_LINE_LENGTH;
    let color = this.SNAP_COLOR;

    if (normal?.x !== 0) {
      color = this.SNAP_AXIS_COLOR_X;
    } else if (normal?.y !== 0) {
      color = this.SNAP_AXIS_COLOR_Y;
    } else if (normal?.z !== 0) {
      color = this.SNAP_AXIS_COLOR_Z;
    }

    const geometry = new THREE.BufferGeometry().setFromPoints([
      new THREE.Vector3(-length / 2, 0, 0),
      new THREE.Vector3(length / 2, 0, 0),
    ]);

    const line = new THREE.LineSegments(
      geometry,
      new THREE.LineBasicMaterial({ color, linewidth: 3 })
    );

    group.add(line);
    group.position.set(position.x, position.y, position.z);

    return group;
  }

  /**
   * Convert world coordinates to screen coordinates
   */
  private worldToScreen(
    position: { x: number; y: number; z: number },
    camera: THREE.Camera
  ): { x: number; y: number } {
    const vector = new THREE.Vector3(position.x, position.y, position.z);
    vector.project(camera);

    const canvas = (camera as any).userData?.canvas;
    if (!canvas) {
      return { x: 0, y: 0 };
    }

    return {
      x: (vector.x * canvas.clientWidth) / 2 + canvas.clientWidth / 2,
      y: -(vector.y * canvas.clientHeight) / 2 + canvas.clientHeight / 2,
    };
  }
}
