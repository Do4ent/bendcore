import * as THREE from 'three';

/**
 * CAD-CORE-028: ViewportControls
 * 3D viewport camera and interaction controls
 * ~300 LOC + tests
 */

export interface ViewportControlsConfig {
  /** Enable orbit camera */
  enableOrbit?: boolean;
  /** Enable pan */
  enablePan?: boolean;
  /** Enable zoom */
  enableZoom?: boolean;
  /** Enable keyboard shortcuts */
  enableKeyboard?: boolean;
  /** Zoom speed */
  zoomSpeed?: number;
  /** Pan speed */
  panSpeed?: number;
  /** Rotation speed */
  rotationSpeed?: number;
  /** Auto rotate */
  autoRotate?: boolean;
  /** Auto rotate speed */
  autoRotateSpeed?: number;
  /** Enable damping */
  enableDamping?: boolean;
  /** Damping factor */
  dampingFactor?: number;
  /** Min zoom distance */
  minDistance?: number;
  /** Max zoom distance */
  maxDistance?: number;
}

/**
 * Camera state for saving/restoring views
 */
export interface CameraState {
  position: { x: number; y: number; z: number };
  target: { x: number; y: number; z: number };
  up: { x: number; y: number; z: number };
}

/**
 * Viewport Controls Manager
 * Handles camera, orbit, pan, zoom, and keyboard controls
 */
export class ViewportControls {
  private camera: THREE.Camera;
  private canvas: HTMLCanvasElement;
  private config: Required<ViewportControlsConfig>;
  private raycaster: THREE.Raycaster;
  private mouse: THREE.Vector2;
  private target: THREE.Vector3;
  private isOrbiting: boolean = false;
  private isPanning: boolean = false;
  private velocity: THREE.Vector3;
  private savedViews: Map<string, CameraState> = new Map();
  private onCameraChange?: (camera: THREE.Camera) => void;

  constructor(camera: THREE.Camera, canvas: HTMLCanvasElement, config: ViewportControlsConfig = {}) {
    this.camera = camera;
    this.canvas = canvas;
    this.raycaster = new THREE.Raycaster();
    this.mouse = new THREE.Vector2();
    this.target = new THREE.Vector3(0, 0, 0);
    this.velocity = new THREE.Vector3();

    this.config = {
      enableOrbit: config.enableOrbit ?? true,
      enablePan: config.enablePan ?? true,
      enableZoom: config.enableZoom ?? true,
      enableKeyboard: config.enableKeyboard ?? true,
      zoomSpeed: config.zoomSpeed ?? 0.1,
      panSpeed: config.panSpeed ?? 0.005,
      rotationSpeed: config.rotationSpeed ?? 0.005,
      autoRotate: config.autoRotate ?? false,
      autoRotateSpeed: config.autoRotateSpeed ?? 0.01,
      enableDamping: config.enableDamping ?? true,
      dampingFactor: config.dampingFactor ?? 0.05,
      minDistance: config.minDistance ?? 1,
      maxDistance: config.maxDistance ?? 10000,
    };

    this.attachEventListeners();
  }

  /**
   * Attach event listeners to canvas
   */
  private attachEventListeners(): void {
    // Mouse events
    this.canvas.addEventListener('mousedown', (e) => this.onMouseDown(e));
    this.canvas.addEventListener('mousemove', (e) => this.onMouseMove(e));
    this.canvas.addEventListener('mouseup', (e) => this.onMouseUp(e));
    this.canvas.addEventListener('wheel', (e) => this.onMouseWheel(e), { passive: false });

    // Touch events
    this.canvas.addEventListener('touchstart', (e) => this.onTouchStart(e));
    this.canvas.addEventListener('touchmove', (e) => this.onTouchMove(e));
    this.canvas.addEventListener('touchend', (e) => this.onTouchEnd(e));

    // Keyboard events
    if (this.config.enableKeyboard) {
      document.addEventListener('keydown', (e) => this.onKeyDown(e));
      document.addEventListener('keyup', (e) => this.onKeyUp(e));
    }
  }

  /**
   * Mouse down handler
   */
  private onMouseDown(event: MouseEvent): void {
    const button = event.button;

    if (button === 0 && this.config.enableOrbit) {
      // Left click - orbit
      this.isOrbiting = true;
    } else if (button === 2 && this.config.enablePan) {
      // Right click - pan
      this.isPanning = true;
    }

    this.updateMousePosition(event);
  }

  /**
   * Mouse move handler
   */
  private onMouseMove(event: MouseEvent): void {
    this.updateMousePosition(event);

    if (this.isOrbiting) {
      this.orbitCamera(event);
    } else if (this.isPanning) {
      this.panCamera(event);
    }
  }

  /**
   * Mouse up handler
   */
  private onMouseUp(event: MouseEvent): void {
    this.isOrbiting = false;
    this.isPanning = false;
  }

  /**
   * Mouse wheel handler
   */
  private onMouseWheel(event: WheelEvent): void {
    event.preventDefault();

    if (!this.config.enableZoom) return;

    const direction = (this.camera as THREE.PerspectiveCamera).position
      .clone()
      .sub(this.target)
      .normalize();

    const distance = (this.camera as THREE.PerspectiveCamera).position.distanceTo(this.target);
    const newDistance = Math.max(
      this.config.minDistance,
      Math.min(this.config.maxDistance, distance + event.deltaY * this.config.zoomSpeed)
    );

    const factor = newDistance / distance;
    (this.camera as THREE.PerspectiveCamera).position
      .copy(this.target)
      .addScaledVector(direction, newDistance);

    this.onCameraChange?.(this.camera);
  }

  /**
   * Touch start handler
   */
  private onTouchStart(event: TouchEvent): void {
    if (event.touches.length === 2) {
      // Two-finger touch - zoom/pinch
      this.updateMousePosition(event.touches[0] as any);
    } else if (event.touches.length === 1) {
      // Single touch - orbit
      this.isOrbiting = true;
      this.updateMousePosition(event.touches[0] as any);
    }
  }

  /**
   * Touch move handler
   */
  private onTouchMove(event: TouchEvent): void {
    if (event.touches.length === 2) {
      // Pinch zoom
      const touch1 = event.touches[0];
      const touch2 = event.touches[1];
      const distance = Math.hypot(
        touch2.clientX - touch1.clientX,
        touch2.clientY - touch1.clientY
      );
      // Handle pinch zoom (simplified)
    } else if (event.touches.length === 1 && this.isOrbiting) {
      this.updateMousePosition(event.touches[0] as any);
      this.orbitCamera(event.touches[0] as any);
    }
  }

  /**
   * Touch end handler
   */
  private onTouchEnd(event: TouchEvent): void {
    this.isOrbiting = false;
    this.isPanning = false;
  }

  /**
   * Keyboard handler
   */
  private onKeyDown(event: KeyboardEvent): void {
    const key = event.key.toLowerCase();

    switch (key) {
      // Preset views
      case '1': // Front view
        this.setViewFront();
        break;
      case '2': // Top view
        this.setViewTop();
        break;
      case '3': // Right view
        this.setViewRight();
        break;
      case '0': // Isometric
        this.setViewIsometric();
        break;
      case 'f': // Fit all
        this.fitAllGeometry();
        break;
      case 'h': // Reset home
        this.resetToHome();
        break;
    }
  }

  /**
   * Keyboard up handler
   */
  private onKeyUp(event: KeyboardEvent): void {
    // Handle key releases if needed
  }

  /**
   * Update mouse position
   */
  private updateMousePosition(event: MouseEvent | Touch): void {
    const rect = this.canvas.getBoundingClientRect();
    this.mouse.x = ((event.clientX - rect.left) / rect.width) * 2 - 1;
    this.mouse.y = -((event.clientY - rect.top) / rect.height) * 2 + 1;
  }

  /**
   * Orbit camera around target
   */
  private orbitCamera(event: MouseEvent | Touch): void {
    const deltaX = (event as any).movementX || 0;
    const deltaY = (event as any).movementY || 0;

    if (deltaX === 0 && deltaY === 0) return;

    const position = (this.camera as THREE.PerspectiveCamera).position;
    const direction = position.clone().sub(this.target).normalize();

    // Rotate around Y axis
    const yAxis = new THREE.Vector3(0, 1, 0);
    const qY = new THREE.Quaternion();
    qY.setFromAxisAngle(yAxis, deltaX * this.config.rotationSpeed);

    // Rotate around X axis (perpendicular to direction)
    const xAxis = new THREE.Vector3().crossVectors(yAxis, direction).normalize();
    const qX = new THREE.Quaternion();
    qX.setFromAxisAngle(xAxis, deltaY * this.config.rotationSpeed);

    const newDirection = direction.clone().applyQuaternion(qY).applyQuaternion(qX);
    const distance = position.distanceTo(this.target);
    position.copy(this.target).addScaledVector(newDirection, distance);

    (this.camera as THREE.PerspectiveCamera).lookAt(this.target);
    this.onCameraChange?.(this.camera);
  }

  /**
   * Pan camera
   */
  private panCamera(event: MouseEvent | Touch): void {
    const deltaX = (event as any).movementX || 0;
    const deltaY = (event as any).movementY || 0;

    if (deltaX === 0 && deltaY === 0) return;

    const position = (this.camera as THREE.PerspectiveCamera).position;
    const forward = this.target.clone().sub(position).normalize();
    const right = new THREE.Vector3().crossVectors(forward, new THREE.Vector3(0, 1, 0)).normalize();
    const up = new THREE.Vector3().crossVectors(right, forward).normalize();

    this.target.addScaledVector(right, -deltaX * this.config.panSpeed);
    this.target.addScaledVector(up, deltaY * this.config.panSpeed);
    position.addScaledVector(right, -deltaX * this.config.panSpeed);
    position.addScaledVector(up, deltaY * this.config.panSpeed);

    (this.camera as THREE.PerspectiveCamera).lookAt(this.target);
    this.onCameraChange?.(this.camera);
  }

  /**
   * Set front view
   */
  setViewFront(): void {
    const distance = (this.camera as THREE.PerspectiveCamera).position.distanceTo(this.target);
    (this.camera as THREE.PerspectiveCamera).position.set(this.target.x, this.target.y, this.target.z + distance);
    (this.camera as THREE.PerspectiveCamera).lookAt(this.target);
    this.onCameraChange?.(this.camera);
  }

  /**
   * Set top view
   */
  setViewTop(): void {
    const distance = (this.camera as THREE.PerspectiveCamera).position.distanceTo(this.target);
    (this.camera as THREE.PerspectiveCamera).position.set(this.target.x, this.target.y + distance, this.target.z);
    (this.camera as THREE.PerspectiveCamera).up.set(0, 0, -1);
    (this.camera as THREE.PerspectiveCamera).lookAt(this.target);
    this.onCameraChange?.(this.camera);
  }

  /**
   * Set right view
   */
  setViewRight(): void {
    const distance = (this.camera as THREE.PerspectiveCamera).position.distanceTo(this.target);
    (this.camera as THREE.PerspectiveCamera).position.set(this.target.x + distance, this.target.y, this.target.z);
    (this.camera as THREE.PerspectiveCamera).lookAt(this.target);
    this.onCameraChange?.(this.camera);
  }

  /**
   * Set isometric view
   */
  setViewIsometric(): void {
    const distance = (this.camera as THREE.PerspectiveCamera).position.distanceTo(this.target);
    const offset = distance / Math.sqrt(3);
    (this.camera as THREE.PerspectiveCamera).position.set(
      this.target.x + offset,
      this.target.y + offset,
      this.target.z + offset
    );
    (this.camera as THREE.PerspectiveCamera).lookAt(this.target);
    this.onCameraChange?.(this.camera);
  }

  /**
   * Fit all geometry in view
   */
  fitAllGeometry(): void {
    // This would typically compute bounding box and fit camera
    // For now, reset to a reasonable distance
    this.resetToHome();
  }

  /**
   * Reset to home view
   */
  resetToHome(): void {
    (this.camera as THREE.PerspectiveCamera).position.set(100, 100, 100);
    (this.camera as THREE.PerspectiveCamera).lookAt(this.target);
    this.onCameraChange?.(this.camera);
  }

  /**
   * Save current view
   */
  saveView(name: string): void {
    const camera = this.camera as THREE.PerspectiveCamera;
    this.savedViews.set(name, {
      position: { x: camera.position.x, y: camera.position.y, z: camera.position.z },
      target: { x: this.target.x, y: this.target.y, z: this.target.z },
      up: { x: camera.up.x, y: camera.up.y, z: camera.up.z },
    });
  }

  /**
   * Load saved view
   */
  loadView(name: string): void {
    const view = this.savedViews.get(name);
    if (!view) return;

    const camera = this.camera as THREE.PerspectiveCamera;
    camera.position.set(view.position.x, view.position.y, view.position.z);
    this.target.set(view.target.x, view.target.y, view.target.z);
    camera.up.set(view.up.x, view.up.y, view.up.z);
    camera.lookAt(this.target);
    this.onCameraChange?.(camera);
  }

  /**
   * Get current camera state
   */
  getCameraState(): CameraState {
    const camera = this.camera as THREE.PerspectiveCamera;
    return {
      position: { x: camera.position.x, y: camera.position.y, z: camera.position.z },
      target: { x: this.target.x, y: this.target.y, z: this.target.z },
      up: { x: camera.up.x, y: camera.up.y, z: camera.up.z },
    };
  }

  /**
   * Set camera state
   */
  setCameraState(state: CameraState): void {
    const camera = this.camera as THREE.PerspectiveCamera;
    camera.position.set(state.position.x, state.position.y, state.position.z);
    this.target.set(state.target.x, state.target.y, state.target.z);
    camera.up.set(state.up.x, state.up.y, state.up.z);
    camera.lookAt(this.target);
    this.onCameraChange?.(camera);
  }

  /**
   * Set camera change callback
   */
  setOnCameraChange(callback: (camera: THREE.Camera) => void): void {
    this.onCameraChange = callback;
  }

  /**
   * Update configuration
   */
  updateConfig(config: Partial<ViewportControlsConfig>): void {
    Object.assign(this.config, config);
  }

  /**
   * Dispose and cleanup
   */
  dispose(): void {
    // Remove event listeners if needed
  }
}

export default ViewportControls;
