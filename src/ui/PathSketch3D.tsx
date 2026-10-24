/**
 * PathSketch3D Component - Phase 35B
 * 3D path sketching system with vertex/segment/constraint editing
 *
 * Features:
 * - Click to add vertices in viewport
 * - Drag to move vertices (with snapping)
 * - Click two vertices to create segments
 * - Edit dimensions (L, S, B, A) on segments
 * - Constraint solver with Newton-Raphson
 * - Real-time validation against profile constraints
 * - 3D rendering of vertices, segments, and constraints
 */

import React, {
  useEffect,
  useRef,
  useState,
  useCallback,
  useMemo,
} from 'react';
import * as THREE from 'three';
import { OrbitCamera, CameraState } from './OrbitCamera';
import { DimensionRenderer } from './DimensionRenderer';
import { PathValidator, ValidationError } from './PathValidator';
import './PathSketch3D.css';
import { SketchConfirmCorner, AxisTriad } from './SketchControls';
import { lockedPoint, type Axis } from './core/axisLock';

// ============= TYPES =============

export interface Vector3 {
  x: number;
  y: number;
  z: number;
}

export interface PathVertex {
  id: string;
  position: Vector3;
  is_fixed: boolean;
  is_selected: boolean;
}

export type DimensionType = 'L' | 'S' | 'B' | 'A';

export interface PathSegment {
  id: string;
  from_vertex: string;
  to_vertex: string;
  dimension_L?: number;
  dimension_S?: number;
  bend_angle?: number;
  plane_rotation?: number;
  selected_dimension: DimensionType;
}

export type ConstraintType =
  | 'COINCIDENT'
  | 'PARALLEL'
  | 'PERPENDICULAR'
  | 'EQUALITY'
  | 'FIXED'
  | 'DISTANCE'
  | 'ANGLE';

export interface Constraint {
  id: string;
  type: ConstraintType;
  entity_ids: string[];
  value?: number;
  is_satisfied: boolean;
  error: number;
}

export interface PathSketchData {
  sketch_id: string;
  profile_id: string;
  profile_r: number;
  profile_s_min: number;
  profile_b_min: number;
  profile_b_max: number;
  vertices: PathVertex[];
  segments: PathSegment[];
  constraints: Constraint[];
  solver_state: {
    converged: boolean;
    iterations: number;
    residual: number;
  };
}

export interface PathSketch3DProps {
  sketch_id?: string;
  /** «Завершить эскиз» в углу подтверждения (1.2); без обработчика угол не показывается. */
  onFinish?: (sketch: PathSketchData) => void;
  /** «Отменить изменения» в углу подтверждения. */
  onCancel?: () => void;
  onCommand?: (commandId: string, args?: Record<string, any>) => void;
  onSketchChanged?: (sketch: PathSketchData) => void;
  onValidationChanged?: (errors: ValidationError[]) => void;
  camera?: OrbitCamera;
  renderer?: THREE.WebGLRenderer;
  scene?: THREE.Scene;
}

// ============= CONSTANTS =============

const VERTEX_RADIUS = 0.5;
const SELECTED_VERTEX_RADIUS = 0.8;
const VERTEX_COLOR = 0x1f6fc5;
const VERTEX_SELECTED_COLOR = 0x00ff00;
const VERTEX_FIXED_COLOR = 0xff6b6b;
const SEGMENT_COLOR = 0x4ecdc4;
const SEGMENT_RADIUS = 0.2;
const CONSTRAINT_POINT_SIZE = 3;
const GRID_SIZE = 0.5;
const GRID_SNAP_THRESHOLD = 0.3;
const DIMENSION_EDIT_DEBOUNCE = 200;
const SOLVER_DEBOUNCE = 200;

// ============= MAIN COMPONENT =============

export const PathSketch3D: React.FC<PathSketch3DProps> = ({
  sketch_id = 'sketch_' + Date.now(),
  onFinish,
  onCancel,
  onCommand,
  onSketchChanged,
  onValidationChanged,
  camera: externalCamera,
  renderer: externalRenderer,
  scene: externalScene,
}) => {
  // ===== REFS =====
  const containerRef = useRef<HTMLDivElement>(null);
  const rendererRef = useRef<THREE.WebGLRenderer | null>(externalRenderer || null);
  const sceneRef = useRef<THREE.Scene | null>(externalScene || null);
  const cameraRef = useRef<OrbitCamera | null>(externalCamera || null);
  const dimensionRendererRef = useRef<DimensionRenderer | null>(null);
  const validatorRef = useRef<PathValidator | null>(null);
  const rafIdRef = useRef<number | null>(null);

  // Group references for easy clearing
  const vertexGroupRef = useRef<THREE.Group | null>(null);
  const segmentGroupRef = useRef<THREE.Group | null>(null);
  const constraintGroupRef = useRef<THREE.Group | null>(null);
  const dimensionGroupRef = useRef<THREE.Group | null>(null);

  // ===== STATE =====
  const [sketch, setSketch] = useState<PathSketchData>({
    sketch_id,
    profile_id: '',
    profile_r: 10,
    profile_s_min: 2,
    profile_b_min: 5,
    profile_b_max: 180,
    vertices: [],
    segments: [],
    constraints: [],
    solver_state: {
      converged: false,
      iterations: 0,
      residual: 0,
    },
  });

  const [validationErrors, setValidationErrors] = useState<ValidationError[]>([]);
  const [isAddingVertex, setIsAddingVertex] = useState(false);
  const [selectedVertexForSegment, setSelectedVertexForSegment] = useState<string | null>(null);
  const [editingSegment, setEditingSegment] = useState<string | null>(null);
  const [draggedVertex, setDraggedVertex] = useState<string | null>(null);
  const [solverDebounceTimer, setSolverDebounceTimer] = useState<NodeJS.Timeout | null>(null);
  // Блокировка оси X/Y/Z (1.2): новая вершина — на оси через последнюю вершину.
  const [lockedAxis, setLockedAxis] = useState<Axis | null>(null);
  const lockedAxisRef = useRef<Axis | null>(null);
  lockedAxisRef.current = lockedAxis;
  const sketchRef = useRef<PathSketchData | null>(null);

  const validator = useMemo(
    () => new PathValidator(sketch.profile_s_min, sketch.profile_b_min, sketch.profile_b_max),
    [sketch.profile_s_min, sketch.profile_b_min, sketch.profile_b_max]
  );

  // ===== HELPER FUNCTIONS =====

  const generateId = (prefix: string): string => `${prefix}_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;

  const snapToGrid = (value: number): number => {
    const snapped = Math.round(value / GRID_SIZE) * GRID_SIZE;
    if (Math.abs(snapped - value) < GRID_SNAP_THRESHOLD) {
      return snapped;
    }
    return value;
  };

  const snapPosition = (pos: Vector3): Vector3 => ({
    x: snapToGrid(pos.x),
    y: snapToGrid(pos.y),
    z: snapToGrid(pos.z),
  });

  const vector3ToThree = (v: Vector3): THREE.Vector3 => new THREE.Vector3(v.x, v.y, v.z);
  const threeToVector3 = (v: THREE.Vector3): Vector3 => ({ x: v.x, y: v.y, z: v.z });

  // ===== VERTEX OPERATIONS =====

  const addVertex = useCallback(
    (position: Vector3, fixed: boolean = false) => {
      const snappedPos = snapPosition(position);
      const newVertex: PathVertex = {
        id: generateId('vertex'),
        position: snappedPos,
        is_fixed: fixed,
        is_selected: false,
      };

      setSketch((prev) => ({
        ...prev,
        vertices: [...prev.vertices, newVertex],
      }));

      return newVertex.id;
    },
    []
  );

  const updateVertex = useCallback((vertexId: string, position: Vector3, fixed?: boolean) => {
    setSketch((prev) => ({
      ...prev,
      vertices: prev.vertices.map((v) =>
        v.id === vertexId
          ? {
              ...v,
              position: snapPosition(position),
              is_fixed: fixed !== undefined ? fixed : v.is_fixed,
            }
          : v
      ),
    }));

    // Trigger solver after position update
    triggerSolver();
  }, []);

  const selectVertex = useCallback((vertexId: string | null, multiSelect: boolean = false) => {
    setSketch((prev) => ({
      ...prev,
      vertices: prev.vertices.map((v) => ({
        ...v,
        is_selected: multiSelect
          ? vertexId === v.id
            ? !v.is_selected
            : v.is_selected
          : vertexId === v.id,
      })),
    }));
  }, []);

  const deleteVertex = useCallback((vertexId: string) => {
    setSketch((prev) => ({
      ...prev,
      vertices: prev.vertices.filter((v) => v.id !== vertexId),
      segments: prev.segments.filter(
        (s) => s.from_vertex !== vertexId && s.to_vertex !== vertexId
      ),
      constraints: prev.constraints.filter(
        (c) => !c.entity_ids.includes(vertexId)
      ),
    }));
  }, []);

  const toggleVertexFixed = useCallback((vertexId: string) => {
    setSketch((prev) => ({
      ...prev,
      vertices: prev.vertices.map((v) =>
        v.id === vertexId ? { ...v, is_fixed: !v.is_fixed } : v
      ),
    }));
  }, []);

  // ===== SEGMENT OPERATIONS =====

  const createSegment = useCallback((fromId: string, toId: string) => {
    const newSegment: PathSegment = {
      id: generateId('segment'),
      from_vertex: fromId,
      to_vertex: toId,
      dimension_L: 10, // Default length
      dimension_S: sketch.profile_s_min,
      bend_angle: 0,
      plane_rotation: 0,
      selected_dimension: 'L',
    };

    setSketch((prev) => ({
      ...prev,
      segments: [...prev.segments, newSegment],
    }));

    triggerSolver();
    return newSegment.id;
  }, [sketch.profile_s_min]);

  const updateSegmentDimension = useCallback(
    (segmentId: string, type: DimensionType, value: number) => {
      setSketch((prev) => ({
        ...prev,
        segments: prev.segments.map((s) =>
          s.id === segmentId
            ? {
                ...s,
                ['dimension_' + type]: value,
                selected_dimension: type,
              }
            : s
        ),
      }));

      // Debounced solver trigger
      if (solverDebounceTimer) clearTimeout(solverDebounceTimer);
      setSolverDebounceTimer(
        setTimeout(() => {
          triggerSolver();
        }, DIMENSION_EDIT_DEBOUNCE)
      );
    },
    [solverDebounceTimer]
  );

  const deleteSegment = useCallback((segmentId: string) => {
    setSketch((prev) => ({
      ...prev,
      segments: prev.segments.filter((s) => s.id !== segmentId),
    }));
  }, []);

  // ===== CONSTRAINT OPERATIONS =====

  const addConstraint = useCallback(
    (type: ConstraintType, entityIds: string[], value?: number) => {
      const newConstraint: Constraint = {
        id: generateId('constraint'),
        type,
        entity_ids: entityIds,
        value,
        is_satisfied: true,
        error: 0,
      };

      setSketch((prev) => ({
        ...prev,
        constraints: [...prev.constraints, newConstraint],
      }));

      triggerSolver();
      return newConstraint.id;
    },
    []
  );

  const deleteConstraint = useCallback((constraintId: string) => {
    setSketch((prev) => ({
      ...prev,
      constraints: prev.constraints.filter((c) => c.id !== constraintId),
    }));
  }, []);

  // ===== SOLVER =====

  const triggerSolver = useCallback(() => {
    // In real implementation, call C++ solver via IPC
    // For now, simulate with simple validation
    if (onCommand) {
      onCommand('solve-constraints', {
        sketch_id: sketch.sketch_id,
      });
    }

    // Validate after solving
    validateSketch();
  }, [sketch.sketch_id, onCommand]);

  // ===== VALIDATION =====

  const validateSketch = useCallback(() => {
    const errors = validator.validateSegments(sketch.segments);
    setValidationErrors(errors);

    if (onValidationChanged) {
      onValidationChanged(errors);
    }
  }, [sketch.segments, validator, onValidationChanged]);

  // ===== RENDERING =====

  const renderVertex = useCallback(
    (vertex: PathVertex) => {
      if (!vertexGroupRef.current) return;

      const geometry = new THREE.SphereGeometry(
        vertex.is_selected ? SELECTED_VERTEX_RADIUS : VERTEX_RADIUS,
        16,
        16
      );

      let color = VERTEX_COLOR;
      if (vertex.is_fixed) {
        color = VERTEX_FIXED_COLOR;
      } else if (vertex.is_selected) {
        color = VERTEX_SELECTED_COLOR;
      }

      const material = new THREE.MeshPhongMaterial({ color });
      const mesh = new THREE.Mesh(geometry, material);
      mesh.position.copy(vector3ToThree(vertex.position));
      mesh.userData = { id: vertex.id, type: 'vertex', data: vertex };

      vertexGroupRef.current.add(mesh);
    },
    []
  );

  const renderSegment = useCallback((segment: PathSegment, vertices: PathVertex[]) => {
    if (!segmentGroupRef.current) return;

    const fromVertex = vertices.find((v) => v.id === segment.from_vertex);
    const toVertex = vertices.find((v) => v.id === segment.to_vertex);

    if (!fromVertex || !toVertex) return;

    const start = vector3ToThree(fromVertex.position);
    const end = vector3ToThree(toVertex.position);
    const mid = start.clone().add(end).divideScalar(2);
    const length = start.distanceTo(end);

    // Draw segment as a tube
    const direction = end.clone().sub(start).normalize();
    const geometry = new THREE.CylinderGeometry(SEGMENT_RADIUS, SEGMENT_RADIUS, length, 8);
    const material = new THREE.MeshPhongMaterial({ color: SEGMENT_COLOR });
    const mesh = new THREE.Mesh(geometry, material);

    mesh.position.copy(mid);
    mesh.lookAt(end);
    mesh.rotateX(Math.PI / 2);
    mesh.userData = { id: segment.id, type: 'segment', data: segment };

    segmentGroupRef.current.add(mesh);
  }, []);

  const redrawSketch = useCallback(() => {
    // Clear groups
    if (vertexGroupRef.current) vertexGroupRef.current.clear();
    if (segmentGroupRef.current) segmentGroupRef.current.clear();
    if (dimensionGroupRef.current) dimensionGroupRef.current.clear();

    // Render segments first (so they appear behind vertices)
    sketch.segments.forEach((segment) => {
      renderSegment(segment, sketch.vertices);
    });

    // Render vertices
    sketch.vertices.forEach((vertex) => {
      renderVertex(vertex);
    });

    // Render dimensions
    if (dimensionRendererRef.current) {
      sketch.segments.forEach((segment) => {
        const fromVertex = sketch.vertices.find((v) => v.id === segment.from_vertex);
        const toVertex = sketch.vertices.find((v) => v.id === segment.to_vertex);
        if (fromVertex && toVertex) {
          dimensionRendererRef.current?.renderDimension(
            segment,
            fromVertex,
            toVertex,
            dimensionGroupRef.current!
          );
        }
      });
    }
  }, [sketch, renderVertex, renderSegment]);

  // ===== INITIALIZATION =====

  // ===== DELETE HANDLER =====
  const deleteSelected = useCallback(() => {
    if (selectedSegmentId) {
      setSketch((prev) => ({
        ...prev,
        segments: prev.segments.filter((s) => s.id !== selectedSegmentId),
      }));
      setSelectedSegmentId(null);
    } else if (selectedVertexId) {
      setSketch((prev) => ({
        ...prev,
        vertices: prev.vertices.filter((v) => v.id !== selectedVertexId),
        segments: prev.segments.filter(
          (s) => s.from_vertex !== selectedVertexId && s.to_vertex !== selectedVertexId
        ),
      }));
      setSelectedVertexId(null);
    }
  }, [selectedSegmentId, selectedVertexId]);

  // ===== KEYBOARD HANDLER =====
  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      if (e.key === 'Delete') {
        e.preventDefault();
        deleteSelected();
      }
    };
    document.addEventListener('keydown', handleKeyDown);
    return () => document.removeEventListener('keydown', handleKeyDown);
  }, [deleteSelected]);

  useEffect(() => {
    if (!containerRef.current || externalRenderer) return;

    // Create scene
    const scene = new THREE.Scene();
    scene.background = new THREE.Color(0xfafafa);
    sceneRef.current = scene;

    // Create renderer
    const renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true });
    const width = containerRef.current.clientWidth || 800;
    const height = containerRef.current.clientHeight || 600;
    renderer.setSize(width, height);
    renderer.setPixelRatio(window.devicePixelRatio);
    renderer.shadowMap.enabled = true;
    containerRef.current.appendChild(renderer.domElement);
    rendererRef.current = renderer;

    // Create camera
    const camera = externalCamera || new OrbitCamera(width / height);
    cameraRef.current = camera;

    // Add lights
    const ambientLight = new THREE.AmbientLight(0xffffff, 0.6);
    scene.add(ambientLight);

    const directionalLight = new THREE.DirectionalLight(0xffffff, 0.8);
    directionalLight.position.set(10, 20, 10);
    directionalLight.castShadow = true;
    scene.add(directionalLight);

    // Add grid
    const gridHelper = new THREE.GridHelper(100, 10, 0xcccccc, 0xeeeeee);
    scene.add(gridHelper);

    // Add axes
    const axesHelper = new THREE.AxesHelper(50);
    scene.add(axesHelper);

    // Create groups for different element types
    const vertexGroup = new THREE.Group();
    vertexGroup.name = 'vertices';
    scene.add(vertexGroup);
    vertexGroupRef.current = vertexGroup;

    const segmentGroup = new THREE.Group();
    segmentGroup.name = 'segments';
    scene.add(segmentGroup);
    segmentGroupRef.current = segmentGroup;

    const constraintGroup = new THREE.Group();
    constraintGroup.name = 'constraints';
    scene.add(constraintGroup);
    constraintGroupRef.current = constraintGroup;

    const dimensionGroup = new THREE.Group();
    dimensionGroup.name = 'dimensions';
    scene.add(dimensionGroup);
    dimensionGroupRef.current = dimensionGroup;

    // Initialize dimension renderer
    dimensionRendererRef.current = new DimensionRenderer(camera);

    // Initialize validator
    validatorRef.current = validator;

    // Handle window resize
    const handleResize = () => {
      const newWidth = containerRef.current?.clientWidth || width;
      const newHeight = containerRef.current?.clientHeight || height;
      renderer.setSize(newWidth, newHeight);
      camera.handleWindowResize(newWidth, newHeight);
    };

    window.addEventListener('resize', handleResize);

    // Mouse event handling
    const handleMouseDown = (e: MouseEvent) => {
      const rect = renderer.domElement.getBoundingClientRect();
      const x = ((e.clientX - rect.left) / rect.width) * 2 - 1;
      const y = -((e.clientY - rect.top) / rect.height) * 2 + 1;

      const raycaster = new THREE.Raycaster();
      raycaster.setFromCamera(new THREE.Vector2(x, y), camera.getCamera());

      if (isAddingVertex) {
        // Add vertex mode - click to place
        const plane = new THREE.Plane(new THREE.Vector3(0, 1, 0), 0);
        const target = new THREE.Vector3();
        raycaster.ray.intersectPlane(plane, target);
        const verts = sketchRef.current?.vertices ?? [];
        const last = verts.length ? verts[verts.length - 1].position : null;
        const ray = raycaster.ray;
        const p = lockedPoint(
          last,
          lockedAxisRef.current,
          { x: ray.origin.x, y: ray.origin.y, z: ray.origin.z },
          { x: ray.direction.x, y: ray.direction.y, z: ray.direction.z },
          { x: target.x, y: target.y, z: target.z },
        );
        addVertex(p);
        return;
      }

      // Try to select/interact with objects
      const objects: THREE.Object3D[] = [];
      scene.traverse((obj) => {
        if (obj instanceof THREE.Mesh && obj.userData.id) {
          objects.push(obj);
        }
      });

      const intersects = raycaster.intersectObjects(objects);

      if (intersects.length > 0) {
        const obj = intersects[0].object;
        const { type, data } = obj.userData;

        if (type === 'vertex') {
          if (selectedVertexForSegment && selectedVertexForSegment !== data.id) {
            // Create segment if we have a starting vertex
            createSegment(selectedVertexForSegment, data.id);
            setSelectedVertexForSegment(null);
          } else if (e.ctrlKey || e.metaKey) {
            selectVertex(data.id, true);
          } else {
            if (selectedVertexForSegment === data.id) {
              setSelectedVertexForSegment(null);
            } else {
              setSelectedVertexForSegment(data.id);
              selectVertex(data.id, false);
            }
          }
          setDraggedVertex(data.id);
        } else if (type === 'segment') {
          setEditingSegment(data.id);
        }
      }
    };

    const handleMouseMove = (e: MouseEvent) => {
      if (!draggedVertex) return;

      const rect = renderer.domElement.getBoundingClientRect();
      const x = ((e.clientX - rect.left) / rect.width) * 2 - 1;
      const y = -((e.clientY - rect.top) / rect.height) * 2 + 1;

      const raycaster = new THREE.Raycaster();
      raycaster.setFromCamera(new THREE.Vector2(x, y), camera.getCamera());

      const plane = new THREE.Plane(new THREE.Vector3(0, 1, 0), 0);
      const target = new THREE.Vector3();
      raycaster.ray.intersectPlane(plane, target);

      updateVertex(draggedVertex, { x: target.x, y: target.y, z: target.z });
    };

    const handleMouseUp = () => {
      setDraggedVertex(null);
    };

    const handleContextMenu = (e: MouseEvent) => {
      e.preventDefault();
      // TODO: Show context menu with delete option
    };

    const handleDoubleClick = (e: MouseEvent) => {
      const rect = renderer.domElement.getBoundingClientRect();
      const x = ((e.clientX - rect.left) / rect.width) * 2 - 1;
      const y = -((e.clientY - rect.top) / rect.height) * 2 + 1;

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
        const obj = intersects[0].object;
        if (obj.userData.type === 'vertex') {
          toggleVertexFixed(obj.userData.data.id);
        }
      }
    };

    renderer.domElement.addEventListener('mousedown', handleMouseDown);
    renderer.domElement.addEventListener('mousemove', handleMouseMove);
    renderer.domElement.addEventListener('mouseup', handleMouseUp);
    renderer.domElement.addEventListener('contextmenu', handleContextMenu);
    renderer.domElement.addEventListener('dblclick', handleDoubleClick);

    // Render loop
    const renderLoop = () => {
      rafIdRef.current = requestAnimationFrame(renderLoop);
      camera.updateAnimation();
      renderer.render(scene, camera.getCamera());
    };

    renderLoop();

    // Notify of changes
    if (onSketchChanged) {
      onSketchChanged(sketch);
    }

    return () => {
      window.removeEventListener('resize', handleResize);
      renderer.domElement.removeEventListener('mousedown', handleMouseDown);
      renderer.domElement.removeEventListener('mousemove', handleMouseMove);
      renderer.domElement.removeEventListener('mouseup', handleMouseUp);
      renderer.domElement.removeEventListener('contextmenu', handleContextMenu);
      renderer.domElement.removeEventListener('dblclick', handleDoubleClick);

      if (rafIdRef.current) {
        cancelAnimationFrame(rafIdRef.current);
      }
      if (containerRef.current && renderer.domElement.parentNode === containerRef.current) {
        containerRef.current.removeChild(renderer.domElement);
      }
      renderer.dispose();
    };
  }, []);

  // Redraw when sketch changes
  useEffect(() => {
    redrawSketch();
    validateSketch();
    if (onSketchChanged) {
      onSketchChanged(sketch);
    }
  }, [sketch, redrawSketch, validateSketch, onSketchChanged]);

  // ===== RENDER =====

  sketchRef.current = sketch;

  return (
    <div className="path-sketch-3d" style={{ position: 'relative' }}>
      <div className="path-sketch-viewport" ref={containerRef} />
      {(onFinish || onCancel) && (
        <SketchConfirmCorner onFinish={() => onFinish?.(sketch)} onCancel={() => onCancel?.()} />
      )}
      <AxisTriad locked={lockedAxis} onChange={setLockedAxis} listenKeys={!editingSegment} />

      <div className="path-sketch-toolbar">
        <button
          className={`path-sketch-btn ${isAddingVertex ? 'active' : ''}`}
          onClick={() => setIsAddingVertex(!isAddingVertex)}
          title="Add Vertex (click in viewport)"
        >
          Add Vertex
        </button>

        <button
          className="path-sketch-btn"
          onClick={() => {
            setSelectedVertexForSegment(null);
            setEditingSegment(null);
          }}
          title="Clear selection"
        >
          Clear Selection
        </button>

        <button
          className="path-sketch-btn"
          onClick={() => triggerSolver()}
          title="Solve constraints"
        >
          Solve
        </button>

        <button
          className="path-sketch-btn"
          onClick={() => validateSketch()}
          title="Validate sketch"
        >
          Validate
        </button>
      </div>

      <div className="path-sketch-info">
        <div>Vertices: {sketch.vertices.length}</div>
        <div>Segments: {sketch.segments.length}</div>
        <div>Constraints: {sketch.constraints.length}</div>
        {validationErrors.length > 0 && (
          <div className="validation-errors">
            {validationErrors.map((err) => (
              <div key={err.id} className={`error-${err.severity}`}>
                {err.message}
              </div>
            ))}
          </div>
        )}
      </div>

      {editingSegment && (
        <SegmentDimensionEditor
          segment={sketch.segments.find((s) => s.id === editingSegment)!}
          onUpdate={(type, value) => {
            updateSegmentDimension(editingSegment, type, value);
          }}
          onClose={() => setEditingSegment(null)}
        />
      )}
    </div>
  );
};

// ============= SEGMENT DIMENSION EDITOR MODAL =============

interface SegmentDimensionEditorProps {
  segment: PathSegment;
  onUpdate: (type: DimensionType, value: number) => void;
  onClose: () => void;
}

const SegmentDimensionEditor: React.FC<SegmentDimensionEditorProps> = ({
  segment,
  onUpdate,
  onClose,
}) => {
  const [dimension_L, setDimensionL] = useState(segment.dimension_L || 10);
  const [dimension_S, setDimensionS] = useState(segment.dimension_S || 2);
  const [bend_angle, setBendAngle] = useState(segment.bend_angle || 0);
  const [plane_rotation, setPlaneRotation] = useState(segment.plane_rotation || 0);

  return (
    <div className="segment-editor-modal">
      <div className="segment-editor-content">
        <h3>Edit Segment Dimensions</h3>

        <div className="editor-group">
          <label>Length (L): {dimension_L.toFixed(2)}</label>
          <input
            type="range"
            min="1"
            max="100"
            step="0.5"
            value={dimension_L}
            onChange={(e) => {
              const val = parseFloat(e.target.value);
              setDimensionL(val);
              onUpdate('L', val);
            }}
          />
        </div>

        <div className="editor-group">
          <label>Straight (S): {dimension_S.toFixed(2)}</label>
          <input
            type="range"
            min="0.5"
            max="50"
            step="0.5"
            value={dimension_S}
            onChange={(e) => {
              const val = parseFloat(e.target.value);
              setDimensionS(val);
              onUpdate('S', val);
            }}
          />
        </div>

        <div className="editor-group">
          <label>Bend Angle (B): {bend_angle.toFixed(1)}°</label>
          <input
            type="range"
            min="0"
            max="180"
            step="1"
            value={bend_angle}
            onChange={(e) => {
              const val = parseFloat(e.target.value);
              setBendAngle(val);
              onUpdate('B', val);
            }}
          />
        </div>

        <div className="editor-group">
          <label>Plane Rotation (A): {plane_rotation.toFixed(1)}°</label>
          <input
            type="range"
            min="0"
            max="360"
            step="1"
            value={plane_rotation}
            onChange={(e) => {
              const val = parseFloat(e.target.value);
              setPlaneRotation(val);
              onUpdate('A', val);
            }}
          />
        </div>

        <div className="editor-buttons">
          <button onClick={onClose}>Done</button>
        </div>
      </div>
    </div>
  );
};

export default PathSketch3D;
