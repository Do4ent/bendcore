/**
 * OrbitCamera System for BendCore 3D Viewport
 * Manages camera position, rotation, and view transformations
 * Supports orbit, pan, zoom, and named views
 */

import * as THREE from 'three';

export interface CameraState {
  position: { x: number; y: number; z: number };
  target: { x: number; y: number; z: number };
  distance: number;
  pitch: number;  // Vertical rotation in degrees
  yaw: number;    // Horizontal rotation in degrees
  fov: number;
  near: number;
  far: number;
}

export interface NamedView {
  name: string;
  state: CameraState;
  description?: string;
}

/**
 * OrbitCamera: Manages 3D viewport camera with orbit, pan, and zoom
 */
export class OrbitCamera {
  private camera: THREE.PerspectiveCamera;
  private state: CameraState;
  private namedViews: Map<string, NamedView>;
  private isAnimating: boolean = false;
  private animationStartTime: number = 0;
  private animationDuration: number = 500;  // ms
  private animationStartState: CameraState | null = null;
  private animationEndState: CameraState | null = null;
  private viewport: { width: number; height: number } | null = null;

  // Default settings
  private readonly DEFAULT_DISTANCE = 100;
  private readonly DEFAULT_FOV = 50;
  private readonly MIN_DISTANCE = 1;
  private readonly MAX_DISTANCE = 10000;
  private readonly ZOOM_SENSITIVITY = 1.1;
  private readonly PAN_SENSITIVITY = 0.1;
  private readonly ROTATION_SENSITIVITY = 0.5;

  constructor(aspect: number = 16 / 9) {
    this.camera = new THREE.PerspectiveCamera(
      this.DEFAULT_FOV,
      aspect,
      0.1,
      this.MAX_DISTANCE * 2
    );

    this.state = {
      position: { x: 0, y: 0, z: this.DEFAULT_DISTANCE },
      target: { x: 0, y: 0, z: 0 },
      distance: this.DEFAULT_DISTANCE,
      pitch: 0,
      yaw: 0,
      fov: this.DEFAULT_FOV,
      near: 0.1,
      far: this.MAX_DISTANCE * 2,
    };

    this.namedViews = new Map();
    this.setupDefaultViews();
    this.updateCameraMatrix();
  }

  /**
   * Get the Three.js camera object
   */
  getCamera(): THREE.PerspectiveCamera {
    return this.camera;
  }

  /**
   * Get current camera state
   */
  getState(): CameraState {
    return OrbitCamera.cloneState(this.state);
  }

  /**
   * Deep-copy a camera state so callers (and animation snapshots) never share
   * the nested position/target objects with the live state.
   */
  private static cloneState(state: CameraState): CameraState {
    return {
      ...state,
      position: { ...state.position },
      target: { ...state.target },
    };
  }

  /**
   * Set camera state directly
   */
  setState(state: Partial<CameraState>): void {
    if (state.position) {
      this.state.position = { ...state.position };
    }
    if (state.target) {
      this.state.target = { ...state.target };
    }
    if (state.distance !== undefined) {
      this.state.distance = Math.max(
        this.MIN_DISTANCE,
        Math.min(state.distance, this.MAX_DISTANCE)
      );
    }
    if (state.pitch !== undefined) {
      this.state.pitch = state.pitch;
    }
    if (state.yaw !== undefined) {
      this.state.yaw = state.yaw;
    }
    if (state.fov !== undefined) {
      this.camera.fov = state.fov;
      this.state.fov = state.fov;
      this.camera.updateProjectionMatrix();
    }

    this.updateCameraMatrix();
  }

  /**
   * Orbit camera around target point
   * @param deltaPitch Change in pitch (vertical rotation) in degrees
   * @param deltaYaw Change in yaw (horizontal rotation) in degrees
   */
  orbit(deltaPitch: number, deltaYaw: number): void {
    this.state.pitch += deltaPitch * this.ROTATION_SENSITIVITY;
    this.state.yaw += deltaYaw * this.ROTATION_SENSITIVITY;

    // Clamp pitch to prevent gimbal lock
    this.state.pitch = Math.max(-89, Math.min(89, this.state.pitch));

    this.updateCameraMatrix();
  }

  /**
   * Pan camera in screen space
   * @param deltaX Change in X (in world units)
   * @param deltaY Change in Y (in world units)
   */
  pan(deltaX: number, deltaY: number): void {
    // Calculate pan amount based on distance and FOV
    const panAmount = (this.state.distance / this.DEFAULT_DISTANCE) * this.PAN_SENSITIVITY;

    // Get camera right and up vectors
    const right = new THREE.Vector3();
    const up = new THREE.Vector3();
    this.camera.getWorldDirection(new THREE.Vector3());

    this.camera.getWorldDirection(new THREE.Vector3());
    right.crossVectors(this.camera.getWorldDirection(new THREE.Vector3()), new THREE.Vector3(0, 1, 0)).normalize();
    up.set(0, 1, 0);

    // Apply pan
    this.state.target.x += right.x * deltaX * panAmount;
    this.state.target.y += up.y * deltaY * panAmount;
    this.state.target.z += right.z * deltaX * panAmount;

    this.updateCameraMatrix();
  }

  /**
   * Zoom camera
   * @param factor Zoom factor (1.1 = 10% zoom in, 0.9 = 10% zoom out)
   */
  zoom(factor: number): void {
    this.state.distance /= factor;
    this.state.distance = Math.max(
      this.MIN_DISTANCE,
      Math.min(this.state.distance, this.MAX_DISTANCE)
    );

    this.updateCameraMatrix();
  }

  /**
   * Fit all objects into view
   * @param boundingBox Bounding box to fit
   */
  fitToView(boundingBox: THREE.Box3 = new THREE.Box3()): void {
    const size = boundingBox.getSize(new THREE.Vector3());
    const center = boundingBox.getCenter(new THREE.Vector3());

    this.state.target = { x: center.x, y: center.y, z: center.z };

    // Calculate required distance based on bounding box size
    const maxDim = Math.max(size.x, size.y, size.z);
    const fov = (this.state.fov * Math.PI) / 180;
    let cameraZ = Math.abs(maxDim / 2 / Math.tan(fov / 2));

    // Add 10% padding
    cameraZ *= 1.1;

    this.state.distance = Math.max(
      this.MIN_DISTANCE,
      Math.min(cameraZ, this.MAX_DISTANCE)
    );

    this.state.pitch = 30;  // Isometric-like view
    this.state.yaw = 45;

    this.updateCameraMatrix();
  }

  /**
   * Fit everything into view. Without a (non-empty) bounding box this resets
   * to the default framing immediately.
   */
  fitAll(boundingBox?: THREE.Box3): void {
    if (boundingBox && !boundingBox.isEmpty()) {
      this.fitToView(boundingBox);
      return;
    }
    this.isAnimating = false;
    this.state.target = { x: 0, y: 0, z: 0 };
    this.state.distance = this.DEFAULT_DISTANCE;
    this.state.pitch = 30;  // Isometric-like view, as fitToView
    this.state.yaw = 45;
    this.updateCameraMatrix();
  }

  /**
   * Current orbit angles in degrees: x = pitch, y = yaw
   */
  getRotation(): { x: number; y: number } {
    return { x: this.state.pitch, y: this.state.yaw };
  }

  /**
   * Current camera position in world space
   */
  getPosition(): { x: number; y: number; z: number } {
    const { x, y, z } = this.camera.position;
    return { x, y, z };
  }

  /**
   * Current distance from the orbit target
   */
  getDistance(): number {
    return this.state.distance;
  }

  /**
   * Set the viewport size in pixels and update the camera aspect ratio
   */
  setViewport(width: number, height: number): void {
    if (!(width > 0) || !(height > 0)) {
      throw new Error(`Invalid viewport size: ${width}x${height}`);
    }
    this.viewport = { width, height };
    this.handleWindowResize(width, height);
  }

  /**
   * Viewport size last set with setViewport (null until set)
   */
  getViewport(): { width: number; height: number } | null {
    return this.viewport ? { ...this.viewport } : null;
  }

  /**
   * Set camera to home/default view
   */
  homeView(): void {
    this.animateTo({
      position: { x: 0, y: 0, z: this.DEFAULT_DISTANCE },
      target: { x: 0, y: 0, z: 0 },
      distance: this.DEFAULT_DISTANCE,
      pitch: 0,
      yaw: 0,
      fov: this.DEFAULT_FOV,
      near: 0.1,
      far: this.MAX_DISTANCE * 2,
    });
  }

  /**
   * Switch to a named view with animation
   */
  switchToView(viewName: string): void {
    const view = this.namedViews.get(viewName);
    if (view) {
      this.animateTo(view.state);
    }
  }

  /**
   * Save current view as named view
   */
  saveView(viewName: string, description?: string): void {
    this.namedViews.set(viewName, {
      name: viewName,
      state: this.getState(),
      description,
    });
  }

  /**
   * Get all named views
   */
  getNamedViews(): NamedView[] {
    return Array.from(this.namedViews.values());
  }

  /**
   * Animate camera to a new state
   */
  animateTo(targetState: CameraState, duration: number = this.animationDuration): void {
    this.isAnimating = true;
    this.animationStartTime = Date.now();
    this.animationDuration = duration;
    this.animationStartState = this.getState();
    this.animationEndState = OrbitCamera.cloneState(targetState);
  }

  /**
   * Update animation frame (call in render loop)
   */
  updateAnimation(): void {
    if (!this.isAnimating || !this.animationStartState || !this.animationEndState) {
      return;
    }

    const elapsed = Date.now() - this.animationStartTime;
    const progress = Math.min(elapsed / this.animationDuration, 1);

    // Use easing function for smooth animation
    const eased = this.easeInOutCubic(progress);

    // Interpolate state
    this.state.position.x =
      this.animationStartState.position.x +
      (this.animationEndState.position.x - this.animationStartState.position.x) * eased;
    this.state.position.y =
      this.animationStartState.position.y +
      (this.animationEndState.position.y - this.animationStartState.position.y) * eased;
    this.state.position.z =
      this.animationStartState.position.z +
      (this.animationEndState.position.z - this.animationStartState.position.z) * eased;

    this.state.target.x =
      this.animationStartState.target.x +
      (this.animationEndState.target.x - this.animationStartState.target.x) * eased;
    this.state.target.y =
      this.animationStartState.target.y +
      (this.animationEndState.target.y - this.animationStartState.target.y) * eased;
    this.state.target.z =
      this.animationStartState.target.z +
      (this.animationEndState.target.z - this.animationStartState.target.z) * eased;

    this.state.distance =
      this.animationStartState.distance +
      (this.animationEndState.distance - this.animationStartState.distance) * eased;

    this.state.pitch =
      this.animationStartState.pitch +
      (this.animationEndState.pitch - this.animationStartState.pitch) * eased;

    this.state.yaw =
      this.animationStartState.yaw +
      (this.animationEndState.yaw - this.animationStartState.yaw) * eased;

    this.updateCameraMatrix();

    if (progress >= 1) {
      this.isAnimating = false;
      this.animationStartState = null;
      this.animationEndState = null;
    }
  }

  /**
   * Handle window resize
   */
  handleWindowResize(width: number, height: number): void {
    const aspect = width / height;
    this.camera.aspect = aspect;
    this.camera.updateProjectionMatrix();
  }

  /**
   * Update camera matrix based on current state
   */
  private updateCameraMatrix(): void {
    // Convert spherical coordinates to Cartesian
    const pitchRad = (this.state.pitch * Math.PI) / 180;
    const yawRad = (this.state.yaw * Math.PI) / 180;

    const x =
      this.state.target.x +
      this.state.distance * Math.sin(yawRad) * Math.cos(pitchRad);
    const y = this.state.target.y + this.state.distance * Math.sin(pitchRad);
    const z =
      this.state.target.z +
      this.state.distance * Math.cos(yawRad) * Math.cos(pitchRad);

    this.camera.position.set(x, y, z);
    this.camera.lookAt(
      this.state.target.x,
      this.state.target.y,
      this.state.target.z
    );
    this.camera.updateMatrixWorld();
  }

  /**
   * Setup default named views
   */
  private setupDefaultViews(): void {
    // Front view
    this.namedViews.set('Front', {
      name: 'Front',
      state: {
        position: { x: 0, y: 0, z: this.DEFAULT_DISTANCE },
        target: { x: 0, y: 0, z: 0 },
        distance: this.DEFAULT_DISTANCE,
        pitch: 0,
        yaw: 0,
        fov: this.DEFAULT_FOV,
        near: 0.1,
        far: this.MAX_DISTANCE * 2,
      },
      description: 'Front view along Z axis',
    });

    // Top view
    this.namedViews.set('Top', {
      name: 'Top',
      state: {
        position: { x: 0, y: this.DEFAULT_DISTANCE, z: 0 },
        target: { x: 0, y: 0, z: 0 },
        distance: this.DEFAULT_DISTANCE,
        pitch: 90,
        yaw: 0,
        fov: this.DEFAULT_FOV,
        near: 0.1,
        far: this.MAX_DISTANCE * 2,
      },
      description: 'Top view along Y axis',
    });

    // Side view (Right)
    this.namedViews.set('Right', {
      name: 'Right',
      state: {
        position: { x: this.DEFAULT_DISTANCE, y: 0, z: 0 },
        target: { x: 0, y: 0, z: 0 },
        distance: this.DEFAULT_DISTANCE,
        pitch: 0,
        yaw: 90,
        fov: this.DEFAULT_FOV,
        near: 0.1,
        far: this.MAX_DISTANCE * 2,
      },
      description: 'Right side view along X axis',
    });

    // Isometric view
    this.namedViews.set('Isometric', {
      name: 'Isometric',
      state: {
        position: {
          x: this.DEFAULT_DISTANCE * 0.707,
          y: this.DEFAULT_DISTANCE * 0.707,
          z: this.DEFAULT_DISTANCE * 0.707,
        },
        target: { x: 0, y: 0, z: 0 },
        distance: this.DEFAULT_DISTANCE * 1.2,
        pitch: 35.26,
        yaw: 45,
        fov: this.DEFAULT_FOV,
        near: 0.1,
        far: this.MAX_DISTANCE * 2,
      },
      description: 'Isometric view (3D perspective)',
    });
  }

  /**
   * Easing function for smooth animation
   */
  private easeInOutCubic(t: number): number {
    return t < 0.5
      ? 4 * t * t * t
      : 1 - Math.pow(-2 * t + 2, 3) / 2;
  }
}
