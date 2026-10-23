/**
 * Viewport3D Component for BendCore CAD
 * Main 3D rendering viewport with camera, selection, and snap support
 */

import React, { useEffect, useRef, useState } from 'react';
import * as THREE from 'three';
import { OrbitCamera, CameraState } from './OrbitCamera';
import { MouseGestures, GestureEvent } from './MouseGestures';
import { SnapConstraints, SnapMode, SnapPoint } from './SnapConstraints';
import SKeyMenu from './SKeyMenu';
import './Viewport3D.css';

export interface Viewport3DProps {
  onCommand?: (commandId: string, args?: Record<string, any>) => void;
  onObjectSelect?: (objectId: string) => void;
  onCameraMove?: (state: CameraState) => void;
  onSnapPoint?: (point: SnapPoint | null) => void;
  width?: number;
  height?: number;
}

/**
 * Viewport3D: Main 3D viewport component
 */
const Viewport3D: React.FC<Viewport3DProps> = ({
  onCommand,
  onObjectSelect,
  onCameraMove,
  onSnapPoint,
  width = 1200,
  height = 800,
}) => {
  const containerRef = useRef<HTMLDivElement>(null);
  const rendererRef = useRef<THREE.WebGLRenderer | null>(null);
  const sceneRef = useRef<THREE.Scene | null>(null);
  const cameraRef = useRef<OrbitCamera | null>(null);
  const mouseGesturesRef = useRef<MouseGestures | null>(null);
  const snapConstraintsRef = useRef<SnapConstraints | null>(null);
  const rafIdRef = useRef<number | null>(null);

  // State
  const [sKeyMenuOpen, setSKeyMenuOpen] = useState(false);
  const [snapMode, setSnapMode] = useState(true);
  const [selectedObjects, setSelectedObjects] = useState<Set<string>>(new Set());

  // Initialize viewport
  useEffect(() => {
    if (!containerRef.current) return;

    // Create scene
    const scene = new THREE.Scene();
    scene.background = new THREE.Color(0xfafafa);
    sceneRef.current = scene;

    // Create renderer
    const renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true });
    renderer.setSize(width, height);
    renderer.setPixelRatio(window.devicePixelRatio);
    renderer.shadowMap.enabled = true;
    containerRef.current.appendChild(renderer.domElement);
    rendererRef.current = renderer;

    // Create camera
    const camera = new OrbitCamera(width / height);
    cameraRef.current = camera;

    // Create lights
    const ambientLight = new THREE.AmbientLight(0xffffff, 0.6);
    scene.add(ambientLight);

    const directionalLight = new THREE.DirectionalLight(0xffffff, 0.8);
    directionalLight.position.set(10, 20, 10);
    directionalLight.castShadow = true;
    directionalLight.shadow.mapSize.width = 2048;
    directionalLight.shadow.mapSize.height = 2048;
    scene.add(directionalLight);

    // Add grid helper
    const gridHelper = new THREE.GridHelper(100, 10, 0xcccccc, 0xeeeeee);
    scene.add(gridHelper);

    // Add axes helper
    const axesHelper = new THREE.AxesHelper(50);
    scene.add(axesHelper);

    // Create example geometry for testing
    const geometry = new THREE.BoxGeometry(10, 10, 10);
    const material = new THREE.MeshPhongMaterial({ color: 0x1f6fc5 });
    const cube = new THREE.Mesh(geometry, material);
    cube.castShadow = true;
    cube.receiveShadow = true;
    cube.userData.id = 'cube_1';
    scene.add(cube);

    // Add snap constraints
    const snapConstraints = new SnapConstraints();
    snapConstraintsRef.current = snapConstraints;
    scene.add(snapConstraints.getSnapGroup());

    // Create mouse gestures handler
    const mouseGestures = new MouseGestures(camera);
    mouseGesturesRef.current = mouseGestures;

    mouseGestures.on((event: GestureEvent) => {
      handleGesture(event, scene);
    });

    // Attach event listeners
    mouseGestures.attachToElement(renderer.domElement);

    // Handle window resize
    const handleResize = () => {
      const newWidth = containerRef.current?.clientWidth || width;
      const newHeight = containerRef.current?.clientHeight || height;

      renderer.setSize(newWidth, newHeight);
      renderer.setPixelRatio(window.devicePixelRatio);
      camera.handleWindowResize(newWidth, newHeight);
    };

    window.addEventListener('resize', handleResize);

    // Handle S key for menu
    const handleKeyDown = (event: KeyboardEvent) => {
      if (event.key.toLowerCase() === 's' && !event.ctrlKey && !event.metaKey) {
        setSKeyMenuOpen(true);
      }
    };

    document.addEventListener('keydown', handleKeyDown);

    // Render loop
    const renderLoop = () => {
      rafIdRef.current = requestAnimationFrame(renderLoop);

      // Update animation
      camera.updateAnimation();

      // Render scene
      renderer.render(scene, camera.getCamera());
    };

    renderLoop();

    // Cleanup
    return () => {
      window.removeEventListener('resize', handleResize);
      document.removeEventListener('keydown', handleKeyDown);
      if (rafIdRef.current) {
        cancelAnimationFrame(rafIdRef.current);
      }
      if (rendererRef.current && containerRef.current) {
        containerRef.current.removeChild(rendererRef.current.domElement);
      }
      renderer.dispose();
      mouseGestures.detachFromElement(renderer.domElement);
    };
  }, [width, height]);

  // Handle viewport gestures
  const handleGesture = (event: GestureEvent, scene: THREE.Scene) => {
    const snap = snapConstraintsRef.current;
    const camera = cameraRef.current;

    switch (event.type) {
      case 'select': {
        const x = event.data?.x || 0;
        const y = event.data?.y || 0;
        handleObjectSelection(x, y, scene, false);
        break;
      }

      case 'multi-select': {
        const x = event.data?.x || 0;
        const y = event.data?.y || 0;
        handleObjectSelection(x, y, scene, true);
        break;
      }

      case 'fit-view': {
        if (camera) {
          const box = new THREE.Box3();
          scene.traverse((obj) => {
            if (obj instanceof THREE.Mesh) {
              box.expandByObject(obj);
            }
          });
          camera.fitToView(box);
        }
        break;
      }

      case 'orbit': {
        if (onCameraMove && camera) {
          onCameraMove(camera.getState());
        }
        break;
      }

      case 'pan': {
        if (onCameraMove && camera) {
          onCameraMove(camera.getState());
        }
        break;
      }

      case 'zoom': {
        if (onCameraMove && camera) {
          onCameraMove(camera.getState());
        }
        break;
      }

      case 'deselect': {
        setSelectedObjects(new Set());
        break;
      }
    }
  };

  // Handle object selection with raycasting
  const handleObjectSelection = (
    clientX: number,
    clientY: number,
    scene: THREE.Scene,
    multiSelect: boolean
  ) => {
    const renderer = rendererRef.current;
    const camera = cameraRef.current;

    if (!renderer || !camera) return;

    const canvas = renderer.domElement;
    const rect = canvas.getBoundingClientRect();
    const x = ((clientX - rect.left) / rect.width) * 2 - 1;
    const y = -((clientY - rect.top) / rect.height) * 2 + 1;

    const raycaster = new THREE.Raycaster();
    raycaster.setFromCamera(new THREE.Vector2(x, y), camera.getCamera());

    const objects: THREE.Object3D[] = [];
    scene.traverse((obj) => {
      if (obj instanceof THREE.Mesh && obj.userData.id) {
        objects.push(obj);
      }
    });

    const intersects = raycaster.intersectObjects(objects);

    if (intersects.length > 0) {
      const selected = intersects[0].object;
      const objectId = selected.userData.id as string;

      if (multiSelect) {
        const newSelected = new Set(selectedObjects);
        if (newSelected.has(objectId)) {
          newSelected.delete(objectId);
        } else {
          newSelected.add(objectId);
        }
        setSelectedObjects(newSelected);
      } else {
        setSelectedObjects(new Set([objectId]));
      }

      if (onObjectSelect) {
        onObjectSelect(objectId);
      }

      // Highlight selected object
      highlightObject(selected, true);
    } else {
      setSelectedObjects(new Set());
    }
  };

  // Highlight selected objects
  const highlightObject = (object: THREE.Object3D, selected: boolean) => {
    if (object instanceof THREE.Mesh) {
      if (selected) {
        const mat = object.material as THREE.MeshPhongMaterial;
        mat.emissive.setHex(0x444444);
      } else {
        const mat = object.material as THREE.MeshPhongMaterial;
        mat.emissive.setHex(0x000000);
      }
    }
  };

  // Handle S-key menu selection
  const handleSKeyMenuSelect = (itemId: string) => {
    setSKeyMenuOpen(false);

    if (onCommand) {
      onCommand(itemId);
    }

    switch (itemId) {
      case 'new-sketch':
        onCommand?.('new-sketch');
        break;
      case 'new-tube':
        onCommand?.('new-tube');
        break;
      case 'new-port':
        onCommand?.('new-port');
        break;
      case 'reference-plane':
        onCommand?.('reference-plane');
        break;
      case 'reference-axis':
        onCommand?.('reference-axis');
        break;
      case 'edit-selected':
        if (selectedObjects.size > 0) {
          const objectId = Array.from(selectedObjects)[0];
          onCommand?.('edit', { objectId });
        }
        break;
    }
  };

  // Toggle snap mode
  const toggleSnapMode = () => {
    const newSnapMode = !snapMode;
    setSnapMode(newSnapMode);

    if (snapConstraintsRef.current) {
      snapConstraintsRef.current.setEnabled(newSnapMode);
    }

    if (onCommand) {
      onCommand('toggle-snap', { enabled: newSnapMode });
    }
  };

  return (
    <div className="viewport-3d-container" ref={containerRef}>
      {/* Viewport canvas */}
      <div className="viewport-3d-canvas" />

      {/* S-Key Menu */}
      <SKeyMenu
        position={{ x: 100, y: 100 }}
        isOpen={sKeyMenuOpen}
        onClose={() => setSKeyMenuOpen(false)}
        onSelect={handleSKeyMenuSelect}
      />

      {/* Viewport Controls */}
      <div className="viewport-controls">
        <button
          className={`viewport-control-button snap-toggle ${snapMode ? 'active' : ''}`}
          onClick={toggleSnapMode}
          title="Toggle Snap (Ctrl+Shift+S)"
        >
          🧲
        </button>

        <div className="viewport-control-separator" />

        <button
          className="viewport-control-button view-button"
          onClick={() => {
            cameraRef.current?.switchToView('Front');
          }}
          title="Front View"
        >
          Front
        </button>

        <button
          className="viewport-control-button view-button"
          onClick={() => {
            cameraRef.current?.switchToView('Top');
          }}
          title="Top View"
        >
          Top
        </button>

        <button
          className="viewport-control-button view-button"
          onClick={() => {
            cameraRef.current?.switchToView('Right');
          }}
          title="Right View"
        >
          Right
        </button>

        <button
          className="viewport-control-button view-button"
          onClick={() => {
            cameraRef.current?.switchToView('Isometric');
          }}
          title="Isometric View"
        >
          ISO
        </button>

        <button
          className="viewport-control-button home-button"
          onClick={() => {
            cameraRef.current?.homeView();
          }}
          title="Home View"
        >
          Home
        </button>
      </div>

      {/* Viewport Hints */}
      <div className="viewport-hints">
        <div className="viewport-hint-row">
          <span className="viewport-hint-label">Right-drag:</span>
          <span className="viewport-hint-text">Orbit • Middle-drag/Shift: Pan • Scroll: Zoom</span>
        </div>
        <div className="viewport-hint-row">
          <span className="viewport-hint-label">S:</span>
          <span className="viewport-hint-text">Quick menu • Ctrl+Click: Multi-select</span>
        </div>
        <div className="viewport-hint-row">
          <span className="viewport-hint-label">Snap:</span>
          <span className="viewport-hint-text">
            {snapMode ? 'Enabled' : 'Disabled'} • Press Ctrl+Shift+S to toggle
          </span>
        </div>
      </div>
    </div>
  );
};

export default Viewport3D;
