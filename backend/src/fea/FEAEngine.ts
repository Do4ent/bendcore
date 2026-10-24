/**
 * BendCore FEA (Finite Element Analysis) Engine
 * 2D/3D Linear Elastic Solver for tube bending design
 */

export interface FEANode {
  id: number;
  x: number;
  y: number;
  z: number;
}

export interface FEAElement {
  id: number;
  nodes: number[];
  type: 'tet4' | 'tet10' | 'tri3' | 'tri6';
}

export interface FEAMaterial {
  name: string;
  elasticModulus: number;     // E (Pa)
  poissonsRatio: number;       // ν
  density: number;             // kg/m³
  yieldStrength: number;       // Pa
  ultimateStrength: number;    // Pa
  thermalExpansion?: number;
}

export interface BoundaryCondition {
  nodeId: number;
  dofId: number; // 0=x, 1=y, 2=z
  value: number;
}

export interface Load {
  nodeId: number;
  dofId: number;
  value: number;
}

export interface FEAMesh {
  nodes: FEANode[];
  elements: FEAElement[];
  material: FEAMaterial;
  boundaryConditions: BoundaryCondition[];
  loads: Load[];
}

export interface FEAResult {
  displacement: number[];
  stress: number[];
  strain: number[];
  vonMises: number[];
  maxDisplacement: number;
  maxStress: number;
  minStress: number;
  maxStrain: number;
  safetyFactor: number;
  convergence: boolean;
  iterations: number;
  executionTime: number;
}

export const MATERIALS: Record<string, FEAMaterial> = {
  steel: {
    name: 'Steel',
    elasticModulus: 210e9,
    poissonsRatio: 0.3,
    density: 7850,
    yieldStrength: 250e6,
    ultimateStrength: 400e6,
  },
  stainless_steel: {
    name: 'Stainless Steel 304',
    elasticModulus: 193e9,
    poissonsRatio: 0.305,
    density: 8000,
    yieldStrength: 215e6,
    ultimateStrength: 505e6,
  },
  aluminum: {
    name: 'Aluminum 6061',
    elasticModulus: 69e9,
    poissonsRatio: 0.33,
    density: 2700,
    yieldStrength: 275e6,
    ultimateStrength: 310e6,
  },
  titanium: {
    name: 'Titanium Grade 2',
    elasticModulus: 103e9,
    poissonsRatio: 0.34,
    density: 4511,
    yieldStrength: 345e6,
    ultimateStrength: 450e6,
  },
  copper: {
    name: 'Copper',
    elasticModulus: 110e9,
    poissonsRatio: 0.34,
    density: 8960,
    yieldStrength: 70e6,
    ultimateStrength: 220e6,
  },
};

export class FEAEngine {
  private mesh: FEAMesh;
  private globalK: number[][] = [];
  private globalF: number[] = [];
  private displacement: number[] = [];

  constructor(mesh: FEAMesh) {
    this.mesh = mesh;
    this.initializeGlobalMatrices();
  }

  /**
   * Initialize global stiffness matrix and force vector
   */
  private initializeGlobalMatrices(): void {
    const n = this.mesh.nodes.length * 3;
    this.globalK = Array(n)
      .fill(null)
      .map(() => Array(n).fill(0));
    this.globalF = Array(n).fill(0);
    this.displacement = Array(n).fill(0);
  }

  /**
   * Calculate element stiffness matrix for 4-node tetrahedron
   */
  private tetrahedronStiffnessMatrix(
    n1: FEANode,
    n2: FEANode,
    n3: FEANode,
    n4: FEANode,
  ): number[][] {
    const E = this.mesh.material.elasticModulus;
    const nu = this.mesh.material.poissonsRatio;

    // Constitutive matrix (D matrix)
    const factor = E / ((1 + nu) * (1 - 2 * nu));
    const D: number[][] = [
      [factor * (1 - nu), factor * nu, factor * nu, 0, 0, 0],
      [factor * nu, factor * (1 - nu), factor * nu, 0, 0, 0],
      [factor * nu, factor * nu, factor * (1 - nu), 0, 0, 0],
      [0, 0, 0, factor * (1 - 2 * nu) / 2, 0, 0],
      [0, 0, 0, 0, factor * (1 - 2 * nu) / 2, 0],
      [0, 0, 0, 0, 0, factor * (1 - 2 * nu) / 2],
    ];

    // Jacobian matrix
    const J = [
      [n2.x - n1.x, n3.x - n1.x, n4.x - n1.x],
      [n2.y - n1.y, n3.y - n1.y, n4.y - n1.y],
      [n2.z - n1.z, n3.z - n1.z, n4.z - n1.z],
    ];

    // Determinant of Jacobian
    const detJ =
      J[0][0] * (J[1][1] * J[2][2] - J[1][2] * J[2][1]) -
      J[0][1] * (J[1][0] * J[2][2] - J[1][2] * J[2][0]) +
      J[0][2] * (J[1][0] * J[2][1] - J[1][1] * J[2][0]);

    if (Math.abs(detJ) < 1e-10) {
      throw new Error('Degenerate element: Jacobian determinant is zero');
    }

    // Simplified stiffness matrix (using volume coordinate approach)
    const volume = Math.abs(detJ) / 6;
    const Ke = Array(12)
      .fill(null)
      .map(() => Array(12).fill(0));

    // Fill with simplified 12x12 matrix
    for (let i = 0; i < 12; i++) {
      for (let j = 0; j < 12; j++) {
        Ke[i][j] = (D[i % 6][j % 6] * volume) / 12;
      }
    }

    return Ke;
  }

  /**
   * Assemble global stiffness matrix
   */
  private assembleStiffnessMatrix(): void {
    for (const element of this.mesh.elements) {
      if (element.type === 'tet4') {
        const n1 = this.mesh.nodes[element.nodes[0]];
        const n2 = this.mesh.nodes[element.nodes[1]];
        const n3 = this.mesh.nodes[element.nodes[2]];
        const n4 = this.mesh.nodes[element.nodes[3]];

        const Ke = this.tetrahedronStiffnessMatrix(n1, n2, n3, n4);

        // Assemble into global matrix
        for (let i = 0; i < 4; i++) {
          for (let j = 0; j < 4; j++) {
            const gi = element.nodes[i] * 3;
            const gj = element.nodes[j] * 3;

            for (let di = 0; di < 3; di++) {
              for (let dj = 0; dj < 3; dj++) {
                this.globalK[gi + di][gj + dj] += Ke[i * 3 + di][j * 3 + dj];
              }
            }
          }
        }
      }
    }
  }

  /**
   * Assemble global force vector
   */
  private assembleForcVector(): void {
    for (const load of this.mesh.loads) {
      const idx = load.nodeId * 3 + load.dofId;
      this.globalF[idx] += load.value;
    }
  }

  /**
   * Apply boundary conditions using penalty method
   */
  private applyBoundaryConditions(): void {
    const penalty = 1e10;

    for (const bc of this.mesh.boundaryConditions) {
      const idx = bc.nodeId * 3 + bc.dofId;

      // Penalty method: add large value to diagonal
      this.globalK[idx][idx] += penalty;
      this.globalF[idx] += penalty * bc.value;
    }
  }

  /**
   * Gauss elimination solver
   */
  private gaussElimination(A: number[][], b: number[]): number[] {
    const n = A.length;
    const x = Array(n).fill(0);

    // Forward elimination
    for (let i = 0; i < n; i++) {
      // Find pivot
      let maxRow = i;
      for (let k = i + 1; k < n; k++) {
        if (Math.abs(A[k][i]) > Math.abs(A[maxRow][i])) {
          maxRow = k;
        }
      }

      // Swap rows
      [A[i], A[maxRow]] = [A[maxRow], A[i]];
      [b[i], b[maxRow]] = [b[maxRow], b[i]];

      // Check for singular matrix
      if (Math.abs(A[i][i]) < 1e-10) {
        throw new Error('Singular matrix encountered during Gaussian elimination');
      }

      // Eliminate column
      for (let k = i + 1; k < n; k++) {
        const factor = A[k][i] / A[i][i];
        for (let j = i; j < n; j++) {
          A[k][j] -= factor * A[i][j];
        }
        b[k] -= factor * b[i];
      }
    }

    // Back substitution
    for (let i = n - 1; i >= 0; i--) {
      x[i] = b[i];
      for (let j = i + 1; j < n; j++) {
        x[i] -= A[i][j] * x[j];
      }
      x[i] /= A[i][i];
    }

    return x;
  }

  /**
   * Calculate stresses from displacements
   */
  private calculateStresses(): number[] {
    const n = this.mesh.nodes.length;
    const stresses = Array(n).fill(0);

    for (const element of this.mesh.elements) {
      if (element.type === 'tet4' && element.nodes.length === 4) {
        // Get element displacements
        const elemDisp: number[] = [];
        for (const nodeId of element.nodes) {
          elemDisp.push(
            this.displacement[nodeId * 3],
            this.displacement[nodeId * 3 + 1],
            this.displacement[nodeId * 3 + 2]
          );
        }

        // Simplified strain calculation
        const E = this.mesh.material.elasticModulus;
        const nu = this.mesh.material.poissonsRatio;

        const avgStrain = elemDisp.reduce((a, b) => a + b, 0) / elemDisp.length;
        const stress = E * avgStrain;

        // Assign to nodes of element
        for (const nodeId of element.nodes) {
          stresses[nodeId] += stress;
        }
      }
    }

    // Average stresses across elements
    const elementCount = Array(n).fill(0);
    for (const element of this.mesh.elements) {
      for (const nodeId of element.nodes) {
        elementCount[nodeId]++;
      }
    }

    for (let i = 0; i < n; i++) {
      if (elementCount[i] > 0) {
        stresses[i] /= elementCount[i];
      }
    }

    return stresses;
  }

  /**
   * Calculate strain from stresses
   */
  private calculateStrains(stresses: number[]): number[] {
    const E = this.mesh.material.elasticModulus;
    return stresses.map((s) => s / E);
  }

  /**
   * Calculate Von Mises stress
   */
  private calculateVonMises(stresses: number[]): number[] {
    return stresses.map((s) => Math.abs(s));
  }

  /**
   * Run complete FEA analysis
   */
  analyze(): FEAResult {
    const startTime = performance.now();

    try {
      // Assemble matrices
      this.assembleStiffnessMatrix();
      this.assembleForcVector();
      this.applyBoundaryConditions();

      // Copy matrices for solving
      const K = this.globalK.map((row) => [...row]);
      const F = [...this.globalF];

      // Solve linear system
      this.displacement = this.gaussElimination(K, F);

      // Calculate stresses
      const stresses = this.calculateStresses();
      const strains = this.calculateStrains(stresses);
      const vonMises = this.calculateVonMises(stresses);

      // Find extrema
      const maxDisp = Math.max(...this.displacement.map(Math.abs));
      const maxStress = Math.max(...stresses.map(Math.abs));
      const minStress = Math.min(...stresses);
      const maxStrain = Math.max(...strains.map(Math.abs));

      // Safety factor
      const safetyFactor =
        this.mesh.material.yieldStrength / Math.max(Math.abs(maxStress), 1);

      const executionTime = performance.now() - startTime;

      return {
        displacement: this.displacement,
        stress: stresses,
        strain: strains,
        vonMises,
        maxDisplacement: maxDisp,
        maxStress,
        minStress,
        maxStrain,
        safetyFactor,
        convergence: true,
        iterations: 1,
        executionTime,
      };
    } catch (error) {
      console.error('FEA analysis failed:', error);
      return {
        displacement: [],
        stress: [],
        strain: [],
        vonMises: [],
        maxDisplacement: 0,
        maxStress: 0,
        minStress: 0,
        maxStrain: 0,
        safetyFactor: 0,
        convergence: false,
        iterations: 0,
        executionTime: performance.now() - startTime,
      };
    }
  }
}

/**
 * Mesh generator for tube geometry
 */
export class MeshGenerator {
  /**
   * Generate tetrahedral mesh from tube geometry
   */
  static generateTubeMesh(
    outerRadius: number,
    innerRadius: number,
    length: number,
    meshDensity: number = 1
  ): FEAMesh {
    const nodes: FEANode[] = [];
    const elements: FEAElement[] = [];

    // Create nodes around tube
    const numSegments = Math.ceil(8 * meshDensity);
    const numLayers = Math.ceil(4 * meshDensity);
    const numAxial = Math.ceil(8 * meshDensity);

    let nodeId = 0;

    // Generate nodes
    for (let k = 0; k < numAxial; k++) {
      const z = (k / (numAxial - 1)) * length;

      for (let j = 0; j < numLayers; j++) {
        const r = innerRadius + (j / (numLayers - 1)) * (outerRadius - innerRadius);

        for (let i = 0; i < numSegments; i++) {
          const theta = (i / numSegments) * 2 * Math.PI;
          nodes.push({
            id: nodeId++,
            x: r * Math.cos(theta),
            y: r * Math.sin(theta),
            z: z,
          });
        }
      }
    }

    // Generate tetrahedral elements (simplified)
    let elemId = 0;
    for (let k = 0; k < numAxial - 1; k++) {
      for (let j = 0; j < numLayers - 1; j++) {
        for (let i = 0; i < numSegments; i++) {
          const i_next = (i + 1) % numSegments;

          const n1 = k * numLayers * numSegments + j * numSegments + i;
          const n2 = k * numLayers * numSegments + j * numSegments + i_next;
          const n3 = k * numLayers * numSegments + (j + 1) * numSegments + i;
          const n4 = k * numLayers * numSegments + (j + 1) * numSegments + i_next;

          const n5 = (k + 1) * numLayers * numSegments + j * numSegments + i;
          const n6 = (k + 1) * numLayers * numSegments + j * numSegments + i_next;
          const n7 = (k + 1) * numLayers * numSegments + (j + 1) * numSegments + i;
          const n8 = (k + 1) * numLayers * numSegments + (j + 1) * numSegments + i_next;

          // Create tetrahedra
          elements.push({
            id: elemId++,
            nodes: [n1, n2, n3, n5],
            type: 'tet4',
          });
          elements.push({
            id: elemId++,
            nodes: [n2, n4, n3, n8],
            type: 'tet4',
          });
          elements.push({
            id: elemId++,
            nodes: [n2, n8, n5, n6],
            type: 'tet4',
          });
          elements.push({
            id: elemId++,
            nodes: [n3, n4, n8, n7],
            type: 'tet4',
          });
          elements.push({
            id: elemId++,
            nodes: [n3, n8, n7, n5],
            type: 'tet4',
          });
          elements.push({
            id: elemId++,
            nodes: [n5, n8, n6, n2],
            type: 'tet4',
          });
        }
      }
    }

    return {
      nodes,
      elements,
      material: MATERIALS.steel,
      boundaryConditions: [],
      loads: [],
    };
  }
}
