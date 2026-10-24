/**
 * BendCore Search API Routes - Q3 2027 Task 3
 * Express routes for advanced search, filtering, and saved searches
 * Integrates C++ search engine with React frontend
 */

import express, { Router, Request, Response, NextFunction } from "express";

const router = Router();

/**
 * Search request interface
 */
interface SearchRequest {
  query?: string;
  filters?: {
    designation?: string;
    material?: string[];
    od_min?: number;
    od_max?: number;
    wall_min?: number;
    wall_max?: number;
    length_min?: number;
    length_max?: number;
    cost_min?: number;
    cost_max?: number;
    manufacturing_method?: string;
    date_from?: string;
    date_to?: string;
    author?: string;
  };
  sort_by?: string;
  sort_ascending?: boolean;
  limit?: number;
  offset?: number;
}

/**
 * Search response interface
 */
interface TubeRecord {
  id: string;
  designation: string;
  material: string;
  od: number;
  wall_thickness: number;
  length?: number;
  manufacturing_method?: string;
  cost_usd?: number;
  created_date: string;
  author: string;
}

interface SearchResponse {
  success: boolean;
  tubes?: TubeRecord[];
  total_count?: number;
  query_time_ms?: number;
  message?: string;
}

interface SavedSearch {
  id: string;
  name: string;
  query: SearchRequest;
  created_at: string;
  created_by?: string;
  last_used_at?: string;
}

/**
 * In-memory storage for saved searches (in production, use database)
 */
const savedSearches: Map<string, SavedSearch> = new Map();

/**
 * Mock database of tubes (in production, query from database)
 */
const mockTubes: TubeRecord[] = [
  {
    id: "tube-1",
    designation: "HY-25",
    material: "Steel",
    od: 25.4,
    wall_thickness: 1.5,
    length: 200,
    manufacturing_method: "Cold Drawn",
    cost_usd: 45.50,
    created_date: "2024-01-15",
    author: "john",
  },
  {
    id: "tube-2",
    designation: "HY-32",
    material: "Steel",
    od: 31.75,
    wall_thickness: 2.0,
    length: 300,
    manufacturing_method: "Hot Rolled",
    cost_usd: 75.00,
    created_date: "2024-02-20",
    author: "jane",
  },
  {
    id: "tube-3",
    designation: "AL-20",
    material: "Aluminum",
    od: 20.0,
    wall_thickness: 1.0,
    length: 150,
    manufacturing_method: "Extruded",
    cost_usd: 25.00,
    created_date: "2024-03-10",
    author: "john",
  },
  {
    id: "tube-4",
    designation: "TI-50",
    material: "Titanium",
    od: 50.8,
    wall_thickness: 3.0,
    length: 500,
    manufacturing_method: "Seamless",
    cost_usd: 150.00,
    created_date: "2024-01-25",
    author: "mike",
  },
  {
    id: "tube-5",
    designation: "CU-15",
    material: "Copper",
    od: 15.875,
    wall_thickness: 0.8,
    length: 100,
    manufacturing_method: "Drawn",
    cost_usd: 35.00,
    created_date: "2024-04-05",
    author: "jane",
  },
];

/**
 * Filter and search tubes
 */
function filterTubes(tubes: TubeRecord[], filters: SearchRequest["filters"]): TubeRecord[] {
  return tubes.filter((tube) => {
    // Designation filter (fuzzy match)
    if (filters?.designation) {
      const distance = levenshteinDistance(
        filters.designation.toLowerCase(),
        tube.designation.toLowerCase()
      );
      if (distance > 3) return false;
    }

    // Material filter
    if (filters?.material && filters.material.length > 0) {
      if (!filters.material.includes(tube.material)) return false;
    }

    // OD range filter
    if (filters?.od_min !== undefined && tube.od < filters.od_min) return false;
    if (filters?.od_max !== undefined && tube.od > filters.od_max) return false;

    // Wall thickness range filter
    if (filters?.wall_min !== undefined && tube.wall_thickness < filters.wall_min) return false;
    if (filters?.wall_max !== undefined && tube.wall_thickness > filters.wall_max) return false;

    // Length range filter
    if (tube.length !== undefined) {
      if (filters?.length_min !== undefined && tube.length < filters.length_min) return false;
      if (filters?.length_max !== undefined && tube.length > filters.length_max) return false;
    }

    // Cost range filter
    if (tube.cost_usd !== undefined) {
      if (filters?.cost_min !== undefined && tube.cost_usd < filters.cost_min) return false;
      if (filters?.cost_max !== undefined && tube.cost_usd > filters.cost_max) return false;
    }

    // Manufacturing method filter
    if (filters?.manufacturing_method && tube.manufacturing_method) {
      if (
        !tube.manufacturing_method
          .toLowerCase()
          .includes(filters.manufacturing_method.toLowerCase())
      )
        return false;
    }

    // Date range filter
    if (filters?.date_from) {
      if (tube.created_date < filters.date_from) return false;
    }
    if (filters?.date_to) {
      if (tube.created_date > filters.date_to) return false;
    }

    // Author filter (exact match)
    if (filters?.author) {
      if (tube.author !== filters.author) return false;
    }

    return true;
  });
}

/**
 * Calculate Levenshtein distance for fuzzy matching
 */
function levenshteinDistance(s1: string, s2: string): number {
  const d: number[][] = [];

  for (let i = 0; i <= s1.length; i++) {
    d[i] = [i];
  }
  for (let j = 0; j <= s2.length; j++) {
    d[0][j] = j;
  }

  for (let i = 1; i <= s1.length; i++) {
    for (let j = 1; j <= s2.length; j++) {
      const cost = s1[i - 1] === s2[j - 1] ? 0 : 1;
      d[i][j] = Math.min(
        d[i - 1][j] + 1, // deletion
        d[i][j - 1] + 1, // insertion
        d[i - 1][j - 1] + cost // substitution
      );
    }
  }

  return d[s1.length][s2.length];
}

/**
 * Sort results by field
 */
function sortResults(
  tubes: TubeRecord[],
  sortBy?: string,
  ascending: boolean = true
): TubeRecord[] {
  if (!sortBy) return tubes;

  const sorted = [...tubes].sort((a, b) => {
    const key = sortBy as keyof TubeRecord;
    const aVal = a[key];
    const bVal = b[key];

    if (typeof aVal === "number" && typeof bVal === "number") {
      return ascending ? aVal - bVal : bVal - aVal;
    }

    if (typeof aVal === "string" && typeof bVal === "string") {
      return ascending ? aVal.localeCompare(bVal) : bVal.localeCompare(aVal);
    }

    return 0;
  });

  return sorted;
}

/**
 * Generate UUID for saved searches
 */
function generateUUID(): string {
  return "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx".replace(/[xy]/g, function (c) {
    const r = (Math.random() * 16) | 0;
    const v = c === "x" ? r : (r & 0x3) | 0x8;
    return v.toString(16);
  });
}

// ===== ROUTES =====

/**
 * POST /api/search
 * Execute a search query with filters
 * Request: SearchRequest
 * Response: SearchResponse
 */
router.post("/search", (req: Request, res: Response) => {
  try {
    const searchReq: SearchRequest = req.body;
    const startTime = performance.now();

    // Filter tubes
    let results = filterTubes(mockTubes, searchReq.filters);

    // Sort results
    results = sortResults(results, searchReq.sort_by, searchReq.sort_ascending !== false);

    // Apply pagination
    const offset = searchReq.offset || 0;
    const limit = searchReq.limit || 100;
    const paginatedResults = results.slice(offset, offset + limit);

    const endTime = performance.now();
    const queryTime = endTime - startTime;

    res.json({
      success: true,
      tubes: paginatedResults,
      total_count: results.length,
      query_time_ms: queryTime,
    } as SearchResponse);
  } catch (error) {
    console.error("Search error:", error);
    res.status(500).json({
      success: false,
      message: "Search failed: " + String(error),
    } as SearchResponse);
  }
});

/**
 * GET /api/search/saved
 * Get all saved searches for the user
 * Response: SavedSearch[]
 */
router.get("/search/saved", (req: Request, res: Response) => {
  try {
    const searches = Array.from(savedSearches.values());
    res.json({
      success: true,
      searches,
    });
  } catch (error) {
    console.error("Error fetching saved searches:", error);
    res.status(500).json({
      success: false,
      message: "Failed to fetch saved searches",
    });
  }
});

/**
 * POST /api/search/saved
 * Save a new search query
 * Request: { name: string; query: SearchRequest; created_by?: string }
 * Response: { success: boolean; id?: string }
 */
router.post("/search/saved", (req: Request, res: Response) => {
  try {
    const { name, query, created_by } = req.body;

    if (!name || !query) {
      return res.status(400).json({
        success: false,
        message: "Missing required fields: name, query",
      });
    }

    const id = generateUUID();
    const savedSearch: SavedSearch = {
      id,
      name,
      query,
      created_at: new Date().toISOString(),
      created_by: created_by || "anonymous",
    };

    savedSearches.set(id, savedSearch);

    res.json({
      success: true,
      id,
    });
  } catch (error) {
    console.error("Error saving search:", error);
    res.status(500).json({
      success: false,
      message: "Failed to save search",
    });
  }
});

/**
 * DELETE /api/search/saved/:id
 * Delete a saved search
 * Response: { success: boolean }
 */
router.delete("/search/saved/:id", (req: Request, res: Response) => {
  try {
    const { id } = req.params;

    if (!savedSearches.has(id)) {
      return res.status(404).json({
        success: false,
        message: "Saved search not found",
      });
    }

    savedSearches.delete(id);

    res.json({
      success: true,
    });
  } catch (error) {
    console.error("Error deleting saved search:", error);
    res.status(500).json({
      success: false,
      message: "Failed to delete saved search",
    });
  }
});

/**
 * PUT /api/search/saved/:id/mark-used
 * Mark a saved search as recently used
 * Response: { success: boolean }
 */
router.put("/search/saved/:id/mark-used", (req: Request, res: Response) => {
  try {
    const { id } = req.params;

    const search = savedSearches.get(id);
    if (!search) {
      return res.status(404).json({
        success: false,
        message: "Saved search not found",
      });
    }

    search.last_used_at = new Date().toISOString();
    savedSearches.set(id, search);

    res.json({
      success: true,
    });
  } catch (error) {
    console.error("Error updating saved search:", error);
    res.status(500).json({
      success: false,
      message: "Failed to update saved search",
    });
  }
});

/**
 * POST /api/search/export
 * Export search results to CSV
 * Request: SearchRequest
 * Response: CSV file
 */
router.post("/search/export", (req: Request, res: Response) => {
  try {
    const searchReq: SearchRequest = req.body;

    // Filter tubes
    let results = filterTubes(mockTubes, searchReq.filters);

    // Sort results
    results = sortResults(results, searchReq.sort_by, searchReq.sort_ascending !== false);

    // Generate CSV
    const headers = [
      "ID",
      "Designation",
      "Material",
      "OD (mm)",
      "Wall Thickness (mm)",
      "Length (mm)",
      "Manufacturing Method",
      "Cost (USD)",
      "Created Date",
      "Author",
    ];

    const rows = results.map((tube) => [
      tube.id,
      tube.designation,
      tube.material,
      tube.od.toFixed(2),
      tube.wall_thickness.toFixed(2),
      tube.length?.toFixed(2) || "-",
      tube.manufacturing_method || "-",
      tube.cost_usd?.toFixed(2) || "-",
      tube.created_date,
      tube.author,
    ]);

    const csvContent = [
      headers.join(","),
      ...rows.map((row) =>
        row
          .map((cell) =>
            typeof cell === "string" && cell.includes(",") ? `"${cell}"` : cell
          )
          .join(",")
      ),
    ].join("\n");

    res.setHeader("Content-Type", "text/csv");
    res.setHeader("Content-Disposition", 'attachment; filename="tubes_export.csv"');
    res.send(csvContent);
  } catch (error) {
    console.error("Error exporting search results:", error);
    res.status(500).json({
      success: false,
      message: "Failed to export search results",
    });
  }
});

export default router;
