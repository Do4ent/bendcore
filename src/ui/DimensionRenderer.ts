/**
 * DimensionRenderer.ts - Phase 35B
 * Renders dimensions (L, S, B, A) on 3D path sketch canvas
 *
 * Dimensions:
 * - L: Length of segment (straight line distance)
 * - S: Straight portion before bend
 * - B: Bend angle in degrees
 * - A: Plane rotation angle
 */

import * as THREE from 'three';
import { PathSegment, PathVertex, Vector3, DimensionType } from './PathSketch3D';
import { OrbitCamera } from './OrbitCamera';

export class DimensionRenderer {
  private canvas: HTMLCanvasElement;
  private ctx: CanvasRenderingContext2D;
  private camera: OrbitCamera;
  private canvas2D: HTMLCanvasElement;
  private ctx2D: CanvasRenderingContext2D | null;

  constructor(camera: OrbitCamera) {
    this.camera = camera;
    this.canvas = document.createElement('canvas');
    this.canvas.width = 1024;
    this.canvas.height = 1024;
    this.ctx = this.canvas.getContext('2d') as CanvasRenderingContext2D;

    this.canvas2D = document.createElement('canvas');
    this.canvas2D.width = 2048;
    this.canvas2D.height = 2048;
    this.ctx2D = this.canvas2D.getContext('2d');
  }

  /**
   * Render a dimension on the 3D canvas
   */
  public renderDimension(
    segment: PathSegment,
    fromVertex: PathVertex,
    toVertex: PathVertex,
    group: THREE.Group
  ): void {
    const startPos = this.vector3ToThree(fromVertex.position);
    const endPos = this.vector3ToThree(toVertex.position);
    const segmentLength = startPos.distanceTo(endPos);

    // Render the selected dimension type
    switch (segment.selected_dimension) {
      case 'L':
        this.renderLengthDimension(startPos, endPos, segment.dimension_L || 0, group);
        break;
      case 'S':
        this.renderStraightDimension(startPos, endPos, segment.dimension_S || 0, group);
        break;
      case 'B':
        this.renderBendAngleDimension(startPos, endPos, segment.bend_angle || 0, group);
        break;
      case 'A':
        this.renderPlaneRotationDimension(startPos, endPos, segment.plane_rotation || 0, group);
        break;
    }

    // Render a small indicator showing which dimension is selected
    this.renderDimensionIndicator(startPos, endPos, segment.selected_dimension, group);
  }

  /**
   * Render length dimension (L) - straight distance between vertices
   */
  private renderLengthDimension(
    start: THREE.Vector3,
    end: THREE.Vector3,
    value: number,
    group: THREE.Group
  ): void {
    const mid = start.clone().add(end).multiplyScalar(0.5);
    const direction = end.clone().sub(start).normalize();
    const perpendicular = new THREE.Vector3(-direction.z, 0, direction.x).normalize();

    // Offset dimension above the segment
    const offset = perpendicular.multiplyScalar(2);
    const dimStart = start.clone().add(offset);
    const dimEnd = end.clone().add(offset);
    const dimMid = dimStart.clone().add(dimEnd).multiplyScalar(0.5);

    // Draw dimension line
    this.drawLine(dimStart, dimEnd, 0x1f6fc5, group);

    // Draw extension lines
    this.drawLine(start, dimStart, 0x999999, group);
    this.drawLine(end, dimEnd, 0x999999, group);

    // Draw arrows at the end
    this.drawArrow(dimStart, dimEnd, 0x1f6fc5, group);

    // Render text label
    this.renderDimensionText(dimMid, `L: ${value.toFixed(2)}`, 0x1f6fc5, group);
  }

  /**
   * Render straight dimension (S) - straight portion before bend
   */
  private renderStraightDimension(
    start: THREE.Vector3,
    end: THREE.Vector3,
    value: number,
    group: THREE.Group
  ): void {
    const mid = start.clone().add(end).multiplyScalar(0.5);
    const direction = end.clone().sub(start).normalize();
    const perpendicular = new THREE.Vector3(-direction.z, 0, direction.x).normalize();

    // Offset dimension below the segment
    const offset = perpendicular.multiplyScalar(-2);
    const dimMid = mid.clone().add(offset);

    // Draw dimension indicator
    const dimLength = Math.min(value * 1.5, 5);
    const dimStart = mid.clone()
      .sub(direction.clone().multiplyScalar(dimLength / 2))
      .add(offset);
    const dimEnd = mid
      .clone()
      .add(direction.clone().multiplyScalar(dimLength / 2))
      .add(offset);

    this.drawLine(dimStart, dimEnd, 0x4ecdc4, group);
    this.drawArrow(dimStart, dimEnd, 0x4ecdc4, group);

    // Render text label
    this.renderDimensionText(
      dimMid.clone().add(new THREE.Vector3(0, 0.5, 0)),
      `S: ${value.toFixed(2)}`,
      0x4ecdc4,
      group
    );
  }

  /**
   * Render bend angle dimension (B) - angle in degrees
   */
  private renderBendAngleDimension(
    start: THREE.Vector3,
    end: THREE.Vector3,
    value: number,
    group: THREE.Group
  ): void {
    const mid = start.clone().add(end).multiplyScalar(0.5);

    // Draw arc to indicate angle
    const radius = 3;
    const arc = this.createArc(mid, value * (Math.PI / 180), radius, 0xff9800, group);
    if (arc) {
      group.add(arc);
    }

    // Render text label
    const textPos = mid.clone().add(new THREE.Vector3(4, 0, 0));
    this.renderDimensionText(textPos, `B: ${value.toFixed(1)}°`, 0xff9800, group);
  }

  /**
   * Render plane rotation dimension (A) - rotation angle
   */
  private renderPlaneRotationDimension(
    start: THREE.Vector3,
    end: THREE.Vector3,
    value: number,
    group: THREE.Group
  ): void {
    const direction = end.clone().sub(start).normalize();
    const mid = start.clone().add(end).multiplyScalar(0.5);

    // Draw rotation indicator on perpendicular plane
    const perpendicular = new THREE.Vector3(-direction.z, 0, direction.x).normalize();
    const up = new THREE.Vector3(0, 1, 0);

    // Draw arc in the perpendicular plane
    const radius = 2.5;
    const arc = this.createArcInPlane(
      mid,
      perpendicular,
      up,
      value * (Math.PI / 180),
      radius,
      0x9c27b0,
      group
    );
    if (arc) {
      group.add(arc);
    }

    // Render text label
    const textPos = mid.clone().add(perpendicular.clone().multiplyScalar(3.5));
    this.renderDimensionText(textPos, `A: ${value.toFixed(1)}°`, 0x9c27b0, group);
  }

  /**
   * Render dimension indicator showing which type is selected
   */
  private renderDimensionIndicator(
    start: THREE.Vector3,
    end: THREE.Vector3,
    type: DimensionType,
    group: THREE.Group
  ): void {
    const mid = start.clone().add(end).multiplyScalar(0.5);
    const iconPos = mid.clone().add(new THREE.Vector3(0, 2, 0));

    // Draw small indicator icon
    const geometry = new THREE.BoxGeometry(0.3, 0.3, 0.3);
    const material = new THREE.MeshPhongMaterial({ color: this.getDimensionColor(type) });
    const box = new THREE.Mesh(geometry, material);
    box.position.copy(iconPos);
    group.add(box);
  }

  /**
   * Helper: Draw a line between two points
   */
  private drawLine(
    start: THREE.Vector3,
    end: THREE.Vector3,
    color: number,
    group: THREE.Group
  ): void {
    const geometry = new THREE.BufferGeometry();
    geometry.setAttribute('position', new THREE.BufferAttribute(
      new Float32Array([start.x, start.y, start.z, end.x, end.y, end.z]),
      3
    ));

    const material = new THREE.LineBasicMaterial({ color });
    const line = new THREE.Line(geometry, material);
    group.add(line);
  }

  /**
   * Helper: Draw an arrow from start to end
   */
  private drawArrow(
    start: THREE.Vector3,
    end: THREE.Vector3,
    color: number,
    group: THREE.Group
  ): void {
    const direction = end.clone().sub(start).normalize();
    const arrowSize = 0.3;

    // Arrow head on the end
    const left = new THREE.Vector3().crossVectors(direction, new THREE.Vector3(0, 1, 0)).normalize();
    const up = new THREE.Vector3().crossVectors(left, direction).normalize();

    const arrowStart = end.clone().sub(direction.clone().multiplyScalar(arrowSize));
    const arrowLeft = arrowStart
      .clone()
      .add(left.clone().multiplyScalar(arrowSize * 0.5))
      .add(direction.clone().multiplyScalar(arrowSize * 0.5));
    const arrowRight = arrowStart
      .clone()
      .sub(left.clone().multiplyScalar(arrowSize * 0.5))
      .add(direction.clone().multiplyScalar(arrowSize * 0.5));

    this.drawLine(end, arrowLeft, color, group);
    this.drawLine(end, arrowRight, color, group);
  }

  /**
   * Helper: Create an arc in 3D space
   */
  private createArc(
    center: THREE.Vector3,
    angle: number,
    radius: number,
    color: number,
    group: THREE.Group
  ): THREE.Line | null {
    const segments = Math.max(10, Math.ceil(Math.abs(angle) * 5));
    const points: THREE.Vector3[] = [];

    for (let i = 0; i <= segments; i++) {
      const t = i / segments;
      const currentAngle = t * angle;
      const x = center.x + radius * Math.cos(currentAngle);
      const y = center.y;
      const z = center.z + radius * Math.sin(currentAngle);
      points.push(new THREE.Vector3(x, y, z));
    }

    const geometry = new THREE.BufferGeometry().setFromPoints(points);
    const material = new THREE.LineBasicMaterial({ color, linewidth: 2 });
    return new THREE.Line(geometry, material);
  }

  /**
   * Helper: Create an arc in a specific plane
   */
  private createArcInPlane(
    center: THREE.Vector3,
    xAxis: THREE.Vector3,
    yAxis: THREE.Vector3,
    angle: number,
    radius: number,
    color: number,
    group: THREE.Group
  ): THREE.Line | null {
    const segments = Math.max(10, Math.ceil(Math.abs(angle) * 5));
    const points: THREE.Vector3[] = [];

    for (let i = 0; i <= segments; i++) {
      const t = i / segments;
      const currentAngle = t * angle;
      const x = Math.cos(currentAngle) * radius;
      const y = Math.sin(currentAngle) * radius;

      const point = center
        .clone()
        .add(xAxis.clone().multiplyScalar(x))
        .add(yAxis.clone().multiplyScalar(y));

      points.push(point);
    }

    const geometry = new THREE.BufferGeometry().setFromPoints(points);
    const material = new THREE.LineBasicMaterial({ color, linewidth: 2 });
    return new THREE.Line(geometry, material);
  }

  /**
   * Helper: Render text label at a position
   */
  private renderDimensionText(
    position: THREE.Vector3,
    text: string,
    color: number,
    group: THREE.Group
  ): void {
    // Create a canvas texture for the text
    const canvas = document.createElement('canvas');
    canvas.width = 256;
    canvas.height = 64;

    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    ctx.fillStyle = this.colorToHex(color);
    ctx.font = 'bold 24px Arial';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.fillText(text, 128, 32);

    const texture = new THREE.CanvasTexture(canvas);
    const material = new THREE.MeshBasicMaterial({ map: texture, transparent: true });
    const geometry = new THREE.PlaneGeometry(2, 0.5);
    const mesh = new THREE.Mesh(geometry, material);

    mesh.position.copy(position);
    mesh.lookAt(this.camera.getCamera().position);

    group.add(mesh);
  }

  /**
   * Helper: Get color for dimension type
   */
  private getDimensionColor(type: DimensionType): number {
    switch (type) {
      case 'L':
        return 0x1f6fc5; // Blue
      case 'S':
        return 0x4ecdc4; // Teal
      case 'B':
        return 0xff9800; // Orange
      case 'A':
        return 0x9c27b0; // Purple
      default:
        return 0x666666;
    }
  }

  /**
   * Helper: Convert color number to hex string
   */
  private colorToHex(color: number): string {
    return '#' + color.toString(16).padStart(6, '0');
  }

  /**
   * Helper: Convert Vector3 to THREE.Vector3
   */
  private vector3ToThree(v: Vector3): THREE.Vector3 {
    return new THREE.Vector3(v.x, v.y, v.z);
  }

  /**
   * Dispose resources
   */
  public dispose(): void {
    this.canvas = null as any;
    this.ctx = null as any;
    this.canvas2D = null as any;
    this.ctx2D = null;
  }
}
