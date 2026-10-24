/**
 * BendCore FEA Simulation API Routes
 */

import express, { Router, Request, Response } from 'express';
import { FEAEngine, MATERIALS, MeshGenerator, FEAMesh, FEANode, FEAElement, BoundaryCondition, Load } from '../fea/FEAEngine';

const router: Router = express.Router();

/**
 * POST /api/simulation/analyze
 * Run FEA analysis on tube geometry
 */
router.post('/analyze', async (req: Request, res: Response): Promise<void> => {
  try {
    const {
      geometry,
      material = 'steel',
      loads: loadData,
      constraints: constraintData,
      meshDensity = 1,
    } = req.body;

    if (!geometry || !geometry.outerRadius || !geometry.innerRadius || !geometry.length) {
      res.status(400).json({
        error: true,
        message: 'Missing required geometry parameters: outerRadius, innerRadius, length',
      });
      return;
    }

    // Validate material
    if (!MATERIALS[material]) {
      res.status(400).json({
        error: true,
        message: `Unknown material: ${material}. Available: ${Object.keys(MATERIALS).join(', ')}`,
      });
      return;
    }

    // Generate mesh
    const mesh = MeshGenerator.generateTubeMesh(
      geometry.outerRadius,
      geometry.innerRadius,
      geometry.length,
      meshDensity
    );

    // Set material
    mesh.material = MATERIALS[material];

    // Apply loads
    if (loadData) {
      for (const load of loadData) {
        mesh.loads.push({
          nodeId: load.nodeId || 0,
          dofId: load.dofId || 2, // Default: Z direction
          value: load.value || 1000,
        });
      }
    }

    // Apply boundary conditions
    if (constraintData) {
      for (const constraint of constraintData) {
        mesh.boundaryConditions.push({
          nodeId: constraint.nodeId || 0,
          dofId: constraint.dofId || 2,
          value: constraint.value || 0,
        });
      }
    }

    // Run FEA analysis
    const engine = new FEAEngine(mesh);
    const result = engine.analyze();

    res.json({
      success: true,
      result,
      meshInfo: {
        nodes: mesh.nodes.length,
        elements: mesh.elements.length,
        material: MATERIALS[material].name,
        outerRadius: geometry.outerRadius,
        innerRadius: geometry.innerRadius,
        length: geometry.length,
      },
    });
  } catch (error) {
    console.error('FEA analysis error:', error);
    res.status(500).json({
      error: true,
      message: error instanceof Error ? error.message : 'FEA analysis failed',
    });
  }
});

/**
 * POST /api/simulation/stress
 * Detailed stress analysis
 */
router.post('/stress', async (req: Request, res: Response): Promise<void> => {
  try {
    const {
      geometry,
      material = 'steel',
      appliedLoad = 1000,
      supportType = 'fixed', // fixed, cantilever, supported
    } = req.body;

    if (!geometry) {
      res.status(400).json({ error: true, message: 'Geometry required' });
      return;
    }

    // Generate mesh
    const mesh = MeshGenerator.generateTubeMesh(
      geometry.outerRadius,
      geometry.innerRadius,
      geometry.length,
      1.5
    );

    mesh.material = MATERIALS[material];

    // Setup boundary conditions
    if (supportType === 'fixed') {
      // Fix all nodes at one end
      const fixedNodeCount = Math.ceil(mesh.nodes.length / 10);
      for (let i = 0; i < fixedNodeCount; i++) {
        for (let dof = 0; dof < 3; dof++) {
          mesh.boundaryConditions.push({
            nodeId: i,
            dofId: dof,
            value: 0,
          });
        }
      }
    } else if (supportType === 'cantilever') {
      // Fix nodes at one end
      const fixedNodeCount = Math.ceil(mesh.nodes.length / 8);
      for (let i = 0; i < fixedNodeCount; i++) {
        for (let dof = 0; dof < 3; dof++) {
          mesh.boundaryConditions.push({
            nodeId: i,
            dofId: dof,
            value: 0,
          });
        }
      }
      // Apply load at opposite end
      const lastNodeIdx = mesh.nodes.length - 1;
      mesh.loads.push({
        nodeId: lastNodeIdx,
        dofId: 2,
        value: appliedLoad,
      });
    }

    // Run analysis
    const engine = new FEAEngine(mesh);
    const result = engine.analyze();

    res.json({
      success: true,
      analysisType: 'stress',
      result,
      interpretation: {
        isSafe: result.safetyFactor > 1.5,
        safetyFactor: result.safetyFactor.toFixed(2),
        yieldStrength: MATERIALS[material].yieldStrength,
        maxAllowableStress: (MATERIALS[material].yieldStrength / 1.5).toFixed(2),
        recommendation:
          result.safetyFactor > 1.5 ? 'Design is safe' : 'Design needs reinforcement',
      },
    });
  } catch (error) {
    console.error('Stress analysis error:', error);
    res.status(500).json({
      error: true,
      message: error instanceof Error ? error.message : 'Stress analysis failed',
    });
  }
});

/**
 * POST /api/simulation/displacement
 * Displacement analysis
 */
router.post('/displacement', async (req: Request, res: Response): Promise<void> => {
  try {
    const { geometry, material = 'steel', appliedLoad = 1000 } = req.body;

    if (!geometry) {
      res.status(400).json({ error: true, message: 'Geometry required' });
      return;
    }

    const mesh = MeshGenerator.generateTubeMesh(
      geometry.outerRadius,
      geometry.innerRadius,
      geometry.length
    );

    mesh.material = MATERIALS[material];

    // Fix base
    for (let i = 0; i < Math.ceil(mesh.nodes.length / 10); i++) {
      for (let dof = 0; dof < 3; dof++) {
        mesh.boundaryConditions.push({
          nodeId: i,
          dofId: dof,
          value: 0,
        });
      }
    }

    // Apply load
    const lastNode = mesh.nodes.length - 1;
    mesh.loads.push({
      nodeId: lastNode,
      dofId: 2,
      value: appliedLoad,
    });

    const engine = new FEAEngine(mesh);
    const result = engine.analyze();

    res.json({
      success: true,
      analysisType: 'displacement',
      result,
      buckling: {
        criticalLoad: geometry.length < 100 ? appliedLoad * 2 : appliedLoad * 0.5,
        bucklingMode: geometry.length > 200 ? 'global' : 'local',
        riskLevel: result.maxDisplacement > 5 ? 'high' : 'low',
      },
    });
  } catch (error) {
    console.error('Displacement analysis error:', error);
    res.status(500).json({
      error: true,
      message: error instanceof Error ? error.message : 'Displacement analysis failed',
    });
  }
});

/**
 * GET /api/simulation/materials
 * Get available materials library
 */
router.get('/materials', (req: Request, res: Response): void => {
  const materials = Object.entries(MATERIALS).map(([key, material]) => ({
    id: key,
    name: material.name,
    elasticModulus: material.elasticModulus,
    poissonsRatio: material.poissonsRatio,
    density: material.density,
    yieldStrength: material.yieldStrength,
    ultimateStrength: material.ultimateStrength,
  }));

  res.json({
    success: true,
    materials,
    count: materials.length,
  });
});

/**
 * POST /api/simulation/buckling
 * Buckling analysis (Euler formula)
 */
router.post('/buckling', (req: Request, res: Response): void => {
  try {
    const { geometry, material = 'steel', supportCondition = 'both-fixed' } = req.body;

    if (!geometry) {
      res.status(400).json({ error: true, message: 'Geometry required' });
      return;
    }

    const E = MATERIALS[material].elasticModulus;
    const L = geometry.length;
    const r = geometry.outerRadius;
    const t = geometry.outerRadius - geometry.innerRadius;

    // Calculate moment of inertia (for hollow cylinder)
    const I =
      (Math.PI / 64) *
      (Math.pow(geometry.outerRadius * 2, 4) - Math.pow(geometry.innerRadius * 2, 4));

    // Euler buckling load with support condition coefficient
    const coefficients: Record<string, number> = {
      'both-free': 1,
      'both-fixed': 16,
      'fixed-free': 1,
      'fixed-supported': 4,
      'both-supported': 4,
    };

    const K = coefficients[supportCondition] || 4;
    const Pcr = (K * Math.PI * Math.PI * E * I) / (L * L);

    res.json({
      success: true,
      analysisType: 'buckling',
      buckling: {
        criticalLoad: Pcr,
        slendernessRatio: L / r,
        momentOfInertia: I,
        supportCondition,
        riskLevel: L / r > 100 ? 'high' : L / r > 50 ? 'medium' : 'low',
        recommendation:
          L / r > 100
            ? 'High risk of buckling - increase diameter or reduce length'
            : 'Acceptable for most applications',
      },
    });
  } catch (error) {
    console.error('Buckling analysis error:', error);
    res.status(500).json({
      error: true,
      message: error instanceof Error ? error.message : 'Buckling analysis failed',
    });
  }
});

/**
 * POST /api/simulation/bending
 * Bending stress analysis
 */
router.post('/bending', (req: Request, res: Response): void => {
  try {
    const { geometry, material = 'steel', bendRadius = 100, bendAngle = 90 } = req.body;

    if (!geometry) {
      res.status(400).json({ error: true, message: 'Geometry required' });
      return;
    }

    const matl = MATERIALS[material];
    const E = matl.elasticModulus;

    // Bend radius / tube radius ratio
    const D = geometry.outerRadius * 2;
    const ratioRd = bendRadius / D;

    // Wipeout factor (for tight bends)
    const wipeoutFactor = ratioRd < 1.5 ? 1 - (1.5 - ratioRd) * 0.2 : 1;

    // Thinning percentage
    const thinning = (1 - wipeoutFactor) * 100;

    // Springback angle
    const springbackAngle = bendAngle * 0.05; // Approximate

    res.json({
      success: true,
      analysisType: 'bending',
      bending: {
        bendRadius,
        bendAngle,
        radioDt: ratioRd,
        wipeoutPercentage: thinning.toFixed(2),
        materialThinning: thinning > 15 ? 'high' : 'acceptable',
        springbackAngle: springbackAngle.toFixed(2),
        recommendation:
          thinning > 15
            ? 'Consider larger bend radius or softer material'
            : 'Bend parameters acceptable',
      },
    });
  } catch (error) {
    res.status(500).json({
      error: true,
      message: error instanceof Error ? error.message : 'Bending analysis failed',
    });
  }
});

export default router;
