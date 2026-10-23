/**
 * Template API Routes
 * RESTful endpoints for template CRUD and matching operations
 * Task 2.5: API Endpoints (Subtask 5)
 */

import { Router, Request, Response } from "express";

// ============ INTERFACES ============

interface TemplateParameter {
  min: number;
  max: number;
  default: number;
  unit: string;
}

interface Template {
  id: string;
  name: string;
  description: string;
  category: string;
  outer_diameter: TemplateParameter;
  wall_thickness: TemplateParameter;
  length: TemplateParameter;
  use_case: string;
  compatible_materials: string[];
}

interface TemplateMatch {
  template_id: string;
  template_name: string;
  confidence: number;
}

// ============ IN-MEMORY TEMPLATE LIBRARY ============

let templateLibrary: Template[] = [
  {
    id: "std-90",
    name: "Standard 90° Bend",
    description: "Most common 90-degree bend template for general-purpose applications",
    category: "Standard",
    outer_diameter: { min: 6, max: 100, default: 25, unit: "mm" },
    wall_thickness: { min: 0.5, max: 5, default: 2, unit: "mm" },
    length: { min: 50, max: 800, default: 400, unit: "mm" },
    use_case: "General-purpose 90° bends, hydraulic lines, pneumatic systems",
    compatible_materials: ["steel-1020", "aluminum-6061", "brass-7030"],
  },
  {
    id: "rfd-small",
    name: "Reinforced Small Diameter",
    description: "Specialized template for small-diameter, high-stress applications",
    category: "Reinforced",
    outer_diameter: { min: 3, max: 25, default: 12, unit: "mm" },
    wall_thickness: { min: 1, max: 3, default: 1.8, unit: "mm" },
    length: { min: 20, max: 300, default: 150, unit: "mm" },
    use_case: "Instrumentation, precision assemblies, sensor tubing",
    compatible_materials: ["steel-4140", "ss-304", "titanium-grade2"],
  },
  {
    id: "lg-asm",
    name: "Large Assembly Profile",
    description: "Template for large-diameter assemblies and complex multi-bend structures",
    category: "Large-Assembly",
    outer_diameter: { min: 50, max: 200, default: 100, unit: "mm" },
    wall_thickness: { min: 2, max: 10, default: 4, unit: "mm" },
    length: { min: 200, max: 2000, default: 800, unit: "mm" },
    use_case: "Aircraft structures, automotive frames, industrial equipment",
    compatible_materials: ["aluminum-6061", "titanium-grade5", "composite-carbon"],
  },
];

// ============ HELPER FUNCTIONS ============

function calculateParameterScore(value: number, param: TemplateParameter): number {
  if (value >= param.min && value <= param.max) {
    return 1.0;
  }

  const distance = value < param.min ? param.min - value : value - param.max;
  const range = param.max - param.min;
  if (range === 0) return 0.0;

  return Math.max(0, 1.0 - distance / range);
}

function matchTemplatesToParameters(
  od: number,
  wall: number,
  length: number,
  numResults: number = 3
): TemplateMatch[] {
  const matches = templateLibrary
    .map((template) => {
      const odScore = calculateParameterScore(od, template.outer_diameter) * 0.3;
      const wallScore = calculateParameterScore(wall, template.wall_thickness) * 0.2;
      const lengthScore = calculateParameterScore(length, template.length) * 0.5;

      const totalScore = (odScore + wallScore + lengthScore) * 100;

      return {
        template_id: template.id,
        template_name: template.name,
        confidence: Math.min(100, Math.max(0, totalScore)),
      };
    })
    .sort((a, b) => b.confidence - a.confidence)
    .slice(0, numResults);

  return matches;
}

// ============ ROUTE HANDLERS ============

export const createTemplateRoutes = (): Router => {
  const router = Router();

  /**
   * GET /api/templates
   * Returns all templates (predefined + user)
   */
  router.get("/", (req: Request, res: Response) => {
    try {
      res.json({
        templates: templateLibrary,
        last_updated: new Date().toISOString(),
        count: templateLibrary.length,
      });
    } catch (error) {
      res.status(500).json({ error: "Failed to retrieve templates" });
    }
  });

  /**
   * GET /api/templates/:id
   * Returns a specific template by ID
   */
  router.get("/:id", (req: Request, res: Response) => {
    try {
      const template = templateLibrary.find((t) => t.id === req.params.id);
      if (!template) {
        return res.status(404).json({ error: "Template not found" });
      }
      res.json(template);
    } catch (error) {
      res.status(500).json({ error: "Failed to retrieve template" });
    }
  });

  /**
   * POST /api/templates/match
   * Match tube parameters to templates with confidence scoring
   * Request body: { od, wall, length, num_results? }
   */
  router.post("/match", (req: Request, res: Response) => {
    try {
      const { od, wall, length, num_results = 3 } = req.body;

      if (typeof od !== "number" || typeof wall !== "number" || typeof length !== "number") {
        return res.status(400).json({
          error: "Invalid request: od, wall, and length are required numbers",
        });
      }

      const matches = matchTemplatesToParameters(od, wall, length, num_results);

      res.json({
        matches,
        timestamp: new Date().toISOString(),
        parameters: { od, wall, length },
      });
    } catch (error) {
      res.status(500).json({ error: "Failed to match templates" });
    }
  });

  /**
   * POST /api/templates
   * Create a new template
   * Request body: { template: Template }
   */
  router.post("/", (req: Request, res: Response) => {
    try {
      const { template } = req.body;

      if (!template || !template.id || !template.name) {
        return res.status(400).json({
          error: "Invalid request: template must have id and name",
        });
      }

      // Check for duplicate ID
      if (templateLibrary.some((t) => t.id === template.id)) {
        return res.status(409).json({
          error: "Template with this ID already exists",
        });
      }

      // Validate template structure
      if (
        !template.outer_diameter ||
        !template.wall_thickness ||
        !template.length
      ) {
        return res.status(400).json({
          error: "Template must include outer_diameter, wall_thickness, and length parameters",
        });
      }

      templateLibrary.push(template);

      res.status(201).json({
        success: true,
        id: template.id,
        message: "Template created successfully",
      });
    } catch (error) {
      res.status(500).json({ error: "Failed to create template" });
    }
  });

  /**
   * PUT /api/templates/:id
   * Update an existing template
   * Request body: { template: Partial<Template> }
   */
  router.put("/:id", (req: Request, res: Response) => {
    try {
      const { template: updates } = req.body;
      const templateIndex = templateLibrary.findIndex((t) => t.id === req.params.id);

      if (templateIndex === -1) {
        return res.status(404).json({ error: "Template not found" });
      }

      // Update template (but don't allow ID changes)
      templateLibrary[templateIndex] = {
        ...templateLibrary[templateIndex],
        ...updates,
        id: req.params.id, // Prevent ID modification
      };

      res.json({
        success: true,
        message: "Template updated successfully",
        template: templateLibrary[templateIndex],
      });
    } catch (error) {
      res.status(500).json({ error: "Failed to update template" });
    }
  });

  /**
   * DELETE /api/templates/:id
   * Delete a user-created template (cannot delete predefined ones)
   */
  router.delete("/:id", (req: Request, res: Response) => {
    try {
      const predefinedIds = ["std-90", "rfd-small", "lg-asm"];

      if (predefinedIds.includes(req.params.id)) {
        return res.status(403).json({
          error: "Cannot delete predefined templates",
        });
      }

      const templateIndex = templateLibrary.findIndex((t) => t.id === req.params.id);

      if (templateIndex === -1) {
        return res.status(404).json({ error: "Template not found" });
      }

      const deleted = templateLibrary.splice(templateIndex, 1);

      res.json({
        success: true,
        message: "Template deleted successfully",
        deleted: deleted[0],
      });
    } catch (error) {
      res.status(500).json({ error: "Failed to delete template" });
    }
  });

  /**
   * GET /api/templates/categories/list
   * Returns all available template categories
   */
  router.get("/categories/list", (req: Request, res: Response) => {
    const categories = [...new Set(templateLibrary.map((t) => t.category))];
    res.json({ categories });
  });

  return router;
};

export default createTemplateRoutes;
