/**
 * Mouse Gestures Handler for BendCore 3D Viewport
 * Converts mouse events to camera and selection commands
 */

import { OrbitCamera } from './OrbitCamera';

export interface MouseGestureConfig {
  orbitButton?: 'right' | 'middle';  // Default: 'right'
  panButton?: 'middle' | 'shift';     // Default: 'middle'
  zoomButton?: 'scroll';              // Default: 'scroll'
  multiSelectModifier?: 'ctrl' | 'shift';  // Default: 'ctrl'
}

export interface GestureEvent {
  type: 'orbit' | 'pan' | 'zoom' | 'select' | 'deselect' | 'multi-select' | 'fit-view';
  data?: any;
}

export type GestureHandler = (event: GestureEvent) => void;

/**
 * MouseGestures: Converts mouse events into viewport gestures
 */
export class MouseGestures {
  private config: Required<MouseGestureConfig>;
  private camera: OrbitCamera;
  private handlers: Set<GestureHandler>;

  // State tracking
  private isMouseDown: boolean = false;
  private lastMousePos: { x: number; y: number } = { x: 0, y: 0 };
  private dragStartPos: { x: number; y: number } = { x: 0, y: 0 };
  private dragStartTime: number = 0;
  private activeGesture: 'orbit' | 'pan' | null = null;

  // Keyboard state
  private keysPressed: Set<string> = new Set();
  private lastClickTime: number = 0;
  private lastClickPos: { x: number; y: number } = { x: 0, y: 0 };
  private readonly DOUBLE_CLICK_THRESHOLD = 300;  // ms
  private readonly DRAG_THRESHOLD = 5;  // pixels

  // Listeners registered by attachToElement, kept so they can be removed again
  private attachedElement: HTMLElement | null = null;
  private readonly onElementMouseDown = (e: MouseEvent) =>
    this.handleMouseDown(e, this.attachedElement as HTMLElement);
  private readonly onElementMouseMove = (e: MouseEvent) =>
    this.handleMouseMove(e, this.attachedElement as HTMLElement);
  private readonly onElementMouseUp = (e: MouseEvent) => this.handleMouseUp(e);
  private readonly onElementWheel = (e: WheelEvent) => this.handleWheel(e);
  private readonly onElementContextMenu = (e: Event) => e.preventDefault();
  private readonly onDocumentKeyDown = (e: KeyboardEvent) => this.handleKeyDown(e);
  private readonly onDocumentKeyUp = (e: KeyboardEvent) => this.handleKeyUp(e);

  constructor(camera: OrbitCamera, config?: MouseGestureConfig) {
    this.camera = camera;
    this.config = {
      orbitButton: config?.orbitButton || 'right',
      panButton: config?.panButton || 'middle',
      zoomButton: config?.zoomButton || 'scroll',
      multiSelectModifier: config?.multiSelectModifier || 'ctrl',
    };
    this.handlers = new Set();
  }

  /**
   * Register a gesture handler
   */
  on(handler: GestureHandler): void {
    this.handlers.add(handler);
  }

  /**
   * Unregister a gesture handler
   */
  off(handler: GestureHandler): void {
    this.handlers.delete(handler);
  }

  /**
   * Emit gesture event to all handlers
   */
  private emit(event: GestureEvent): void {
    this.handlers.forEach((handler) => handler(event));
  }

  /**
   * Handle mouse down event
   */
  handleMouseDown(event: MouseEvent, viewport: HTMLElement): void {
    this.isMouseDown = true;
    this.lastMousePos = { x: event.clientX, y: event.clientY };
    this.dragStartPos = { x: event.clientX, y: event.clientY };
    this.dragStartTime = Date.now();

    // Determine which gesture is active
    if (event.button === 2) {  // Right click
      this.activeGesture = 'orbit';
    } else if (event.button === 1) {  // Middle click
      this.activeGesture = 'pan';
    } else if (event.button === 0 && this.isShiftActive(event)) {  // Left + Shift = pan
      this.activeGesture = 'pan';
    }

    // Prevent context menu on right click
    if (event.button === 2) {
      event.preventDefault();
    }
  }

  /**
   * Handle mouse move event
   */
  handleMouseMove(event: MouseEvent, viewport: HTMLElement): void {
    if (!this.isMouseDown || !this.activeGesture) {
      return;
    }

    const deltaX = event.clientX - this.lastMousePos.x;
    const deltaY = event.clientY - this.lastMousePos.y;

    this.lastMousePos = { x: event.clientX, y: event.clientY };

    // Only trigger gesture if dragging past threshold
    const dragDistance = Math.hypot(
      event.clientX - this.dragStartPos.x,
      event.clientY - this.dragStartPos.y
    );

    if (dragDistance < this.DRAG_THRESHOLD) {
      return;
    }

    if (this.activeGesture === 'orbit') {
      this.camera.orbit(deltaY, deltaX);
      this.emit({
        type: 'orbit',
        data: { deltaPitch: deltaY, deltaYaw: deltaX },
      });
    } else if (this.activeGesture === 'pan') {
      this.camera.pan(-deltaX * 0.5, deltaY * 0.5);
      this.emit({
        type: 'pan',
        data: { deltaX: -deltaX * 0.5, deltaY: deltaY * 0.5 },
      });
    }
  }

  /**
   * Handle mouse up event
   */
  handleMouseUp(event: MouseEvent): void {
    const dragDistance = Math.hypot(
      event.clientX - this.dragStartPos.x,
      event.clientY - this.dragStartPos.y
    );
    const dragDuration = Date.now() - this.dragStartTime;

    // Check for click (not a drag)
    if (dragDistance < this.DRAG_THRESHOLD && dragDuration < 200) {
      this.handleClick(event);
    }

    this.isMouseDown = false;
    this.activeGesture = null;
  }

  /**
   * Handle click event
   */
  private handleClick(event: MouseEvent): void {
    const now = Date.now();
    const clickDistance = Math.hypot(
      event.clientX - this.lastClickPos.x,
      event.clientY - this.lastClickPos.y
    );

    // Check for double-click
    if (
      now - this.lastClickTime < this.DOUBLE_CLICK_THRESHOLD &&
      clickDistance < 10
    ) {
      this.emit({
        type: 'fit-view',
        data: { x: event.clientX, y: event.clientY },
      });
      this.lastClickTime = 0;  // Reset to prevent triple-click
      return;
    }

    this.lastClickTime = now;
    this.lastClickPos = { x: event.clientX, y: event.clientY };

    // Single click selection
    if (event.button === 0) {  // Left click
      if (this.isMultiSelectModifierActive(event)) {
        // Multi-select
        this.emit({
          type: 'multi-select',
          data: { x: event.clientX, y: event.clientY },
        });
      } else {
        // Single select
        this.emit({
          type: 'select',
          data: { x: event.clientX, y: event.clientY },
        });
      }
    }
  }

  /**
   * Handle wheel event for zoom
   */
  handleWheel(event: WheelEvent): void {
    event.preventDefault();

    const zoomFactor = event.deltaY > 0 ? 1.1 : 0.909;  // 10% zoom
    this.camera.zoom(zoomFactor);

    this.emit({
      type: 'zoom',
      data: { factor: zoomFactor },
    });
  }

  /**
   * Handle keyboard down event
   */
  handleKeyDown(event: KeyboardEvent): void {
    this.keysPressed.add(event.code);

    // Arrow keys for pan
    if (event.code === 'ArrowUp' || event.code === 'KeyW') {
      this.camera.pan(0, 10);
      this.emit({ type: 'pan', data: { deltaX: 0, deltaY: 10 } });
    } else if (event.code === 'ArrowDown' || event.code === 'KeyS') {
      this.camera.pan(0, -10);
      this.emit({ type: 'pan', data: { deltaX: 0, deltaY: -10 } });
    } else if (event.code === 'ArrowLeft' || event.code === 'KeyA') {
      this.camera.pan(-10, 0);
      this.emit({ type: 'pan', data: { deltaX: -10, deltaY: 0 } });
    } else if (event.code === 'ArrowRight' || event.code === 'KeyD') {
      this.camera.pan(10, 0);
      this.emit({ type: 'pan', data: { deltaX: 10, deltaY: 0 } });
    }

    // +/- for zoom
    if (event.code === 'Equal' || event.code === 'Plus') {
      this.camera.zoom(0.909);  // 10% zoom in
      this.emit({ type: 'zoom', data: { factor: 0.909 } });
    } else if (event.code === 'Minus') {
      this.camera.zoom(1.1);  // 10% zoom out
      this.emit({ type: 'zoom', data: { factor: 1.1 } });
    }

    // Home key for home view
    if (event.code === 'Home') {
      this.camera.homeView();
      this.emit({ type: 'orbit', data: { action: 'home-view' } });
    }

    // Escape for deselect
    if (event.code === 'Escape') {
      this.emit({ type: 'deselect' });
    }
  }

  /**
   * Handle keyboard up event
   */
  handleKeyUp(event: KeyboardEvent): void {
    this.keysPressed.delete(event.code);
  }

  /**
   * Attach event listeners to viewport element
   */
  attachToElement(element: HTMLElement): void {
    if (this.attachedElement) {
      this.detachFromElement(this.attachedElement);
    }
    this.attachedElement = element;

    element.addEventListener('mousedown', this.onElementMouseDown);
    element.addEventListener('mousemove', this.onElementMouseMove);
    element.addEventListener('mouseup', this.onElementMouseUp);
    element.addEventListener('wheel', this.onElementWheel, { passive: false });
    element.addEventListener('contextmenu', this.onElementContextMenu);

    document.addEventListener('keydown', this.onDocumentKeyDown);
    document.addEventListener('keyup', this.onDocumentKeyUp);
  }

  /**
   * Detach event listeners from viewport element
   */
  detachFromElement(element: HTMLElement): void {
    element.removeEventListener('mousedown', this.onElementMouseDown);
    element.removeEventListener('mousemove', this.onElementMouseMove);
    element.removeEventListener('mouseup', this.onElementMouseUp);
    element.removeEventListener('wheel', this.onElementWheel);
    element.removeEventListener('contextmenu', this.onElementContextMenu);

    if (this.attachedElement === element) {
      document.removeEventListener('keydown', this.onDocumentKeyDown);
      document.removeEventListener('keyup', this.onDocumentKeyUp);
      this.attachedElement = null;
    }
  }

  /**
   * Shift held: tracked key codes (ShiftLeft/ShiftRight) or the event's own flag
   */
  private isShiftActive(event: MouseEvent): boolean {
    return event.shiftKey || this.getModifierState().shift;
  }

  /**
   * Modifier configured for multi-select (Ctrl, or Cmd on macOS; or Shift)
   */
  private isMultiSelectModifierActive(event: MouseEvent): boolean {
    if (this.config.multiSelectModifier === 'shift') {
      return this.isShiftActive(event);
    }
    const modifiers = this.getModifierState();
    return event.ctrlKey || event.metaKey || modifiers.ctrl || modifiers.meta;
  }

  /**
   * Get current keyboard modifier state
   */
  getModifierState(): {
    ctrl: boolean;
    shift: boolean;
    alt: boolean;
    meta: boolean;
  } {
    return {
      ctrl: this.keysPressed.has('ControlLeft') || this.keysPressed.has('ControlRight'),
      shift: this.keysPressed.has('ShiftLeft') || this.keysPressed.has('ShiftRight'),
      alt: this.keysPressed.has('AltLeft') || this.keysPressed.has('AltRight'),
      meta: this.keysPressed.has('MetaLeft') || this.keysPressed.has('MetaRight'),
    };
  }
}
