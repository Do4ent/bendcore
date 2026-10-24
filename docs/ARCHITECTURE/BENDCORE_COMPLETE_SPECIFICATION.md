# BENDCORE CAD - COMPLETE SPECIFICATION (FINAL)
**Status:** 🔒 FIXED AND VERIFIED  
**Date:** 2026-10-22  
**Reference Interface:** Assembly-Color.dc.html (session 98d0d30b turn 312)  
**Total Functions:** ~120 across 6 functional points + 11 color categories  

---

## OVERVIEW

This document consolidates the **complete specification of BendCore CAD** verified across all discussion sessions:

- **Primary session:** 98d0d30b-2505-430b-bd26-351d298f363f (1202 turns, 2026-10-20 to 2026-10-22)
- **Secondary session:** 0c7f9aed (development phases, aligned)
- **Reference interface:** Assembly-Color.dc.html (turn 312, artifact V4JBXkifhgoocMHMkyub4J)

---

## SECTION 1: PATH (3D-SKETCH EDITOR) - 18 Functions

| # | Function | Source Turn | Description |
|---|----------|------------|-------------|
| 1.1 | Data Model | 5-7 | Sharp vertices P0…Pn connected by straight segments; bend radius R defined in Tube element, not sketch |
| 1.2 | Line/Polyline Command | 5 | Segment construction with states: await point → rubber line → Enter/RMB/Esc completes |
| 1.3 | Point Command | 5 | Free vertex insertion with interactive placement |
| 1.4 | Dimension Command | 5 | Segment length, bend angle B, distances, angle to plane |
| 1.5 | Constraint System | 5 | Coincidence, fixed, ∥, ⊥, horizontal/vertical, coplanar, equal, symmetry (8+ types) |
| 1.6 | Trim/Extend Command | 5 | Trim segments to geometry |
| 1.7 | Mirror & Project Edges | 5 | Operations on chain |
| 1.8 | Solver Algorithm | 5 | Iterative (Newton/Levenberg-Marquardt), priority: user dimensions > explicit > auto > form |
| 1.9 | Diagnostics | 5 | DoF counter, conflicts, color coding (blue under-constrained, black defined, red conflict, violet reference) |
| 1.10 | Vertex Editing | 5 | Drag in plane/along axis; live validation: segment < min straight → red |
| 1.11 | Auto-Constraints | 5 | Horizontal, vertical, ∥, ⊥ enabled by default, toggleable |
| 1.12 | Cursor Fields | 5 | Tab switches: Length, Angle B, Rotation A (L-R-A logic) |
| 1.13 | Exit Checks | 5-6 | No zero segments, chain connected, colinear merge |
| 1.14 | **TECHNOLOGY CONSTRAINTS** | **6** | **R = f(diameter, instrument type): manual / mechanized (CNC — out of v1); Minimum straight between bends; N_start, N_end for clamping** |
| 1.15 | Editor States | 5-6 | Idle → Tool active → Value input → Drag → Conflict |
| 1.16 | Keyboard Shortcuts | 5 | L (line), D (dimension), Tab, Enter, Esc, Ctrl+Z |
| 1.17 | Self-Intersection Check | 5-6 | Before exit; error on intersection |
| 1.18 | Exit Commands | 5 | ✓ Finish Sketch / Esc |

---

## SECTION 2: TUBE ELEMENT - 19 Functions

| # | Function | Source Turn | Description |
|---|----------|------------|-------------|
| 2.1 | Bending Profile (Library) | 7, 17 | Table by diameter: ∅, R axis, min straight, N_start, N_end, max angle |
| 2.2 | Profile Management | 7 | Create, copy, edit, import/export CSV; separate from part; link + snapshot |
| 2.3 | Dialog: Path Tab | 7 | Sketch selection, direction flip |
| 2.4 | Dialog: Instrument Tab | 7 | Profile list; R read-only |
| 2.5 | Dialog: Section Tab | 7, 17 | ∅ from profile only; wall thickness numeric or from list |
| 2.6 | Dialog: Ends Tab | 7, 17 | Straight, chamfer, flange, swage, cone |
| 2.7 | Dialog: Material Tab | 17 | From standards or custom |
| 2.8 | Path Source Mode | 13 | **"Sketch"** (table read-only) OR **"Table"** (editable) |
| 2.9 | Nomenclature Assignment (Variant 1) | 17 | From catalog with filters (standard, ∅, wall, material) |
| 2.10 | Custom Assignment (Variant 2) | 17 | Manual ∅, wall, material, density; "non-standard" icon in tree |
| 2.11 | Build Algorithm | 7 | T = R·tan(B/2); S_i = L_i − T_left − T_right; body: cylinders + tori |
| 2.12 | Trim Allowance | 9-11 | Δ_start = max(0, N_start − S_first); Δ_end similar; shown on drawing only |
| 2.13 | Finished Tube Length | 9-11 | = ΣS + ΣR·B (no allowances) |
| 2.14 | Blank Length | 9-11 | = finished + Δ_start + Δ_end |
| 2.15 | States | 7 | Normal → Warning → Technology error → Geometry error |
| 2.16 | Exit Blocking | 7 | Geometry error: no body; Technology error: blocks export & drawing |
| 2.17 | 3D Preview | 7 | In dialog |
| 2.18 | Buttons | 7 | OK / Apply / Cancel |
| 2.19 | Instrument/∅ Change | 7 | Vertices stay, R & arcs recalc, checks re-run |

---

## SECTION 3: BEND TABLE & MACHINE - 20 Functions

| # | Function | Source Turn | Description |
|---|----------|------------|-------------|
| 3.1 | Table Structure | 13 | Columns: №, L (straight before bend), A (plane rotation), B (bend angle), reference arc length |
| 3.2 | Summary Row | 13 | Last S and finished tube length |
| 3.3 | A (Rotation) Calculation | 13 | Angle between bend planes; bend 1 has A=0 |
| 3.4 | 3D Link | 13 | Click row → highlight bend; click bend → highlight row |
| 3.5 | Cell Status | 13 | Violation = red, tooltip shows reason |
| 3.6 | Source of Truth | 13 | **"Sketch"** → table read-only; **"Table"** → editable |
| 3.7 | Mode Switch | 13 | Preserves geometry; constraints not ported (warning) |
| 3.8 | Output Profile | 13 | Column set, mark base, turn sign, units, file template |
| 3.9 | Instrument Presets | 13 | Manual (marks), Mechanized (feed); CNC — out of v1 (decision 2026-10-23); XYZ/LRA CSV for measuring systems (3.6) |
| 3.10 | Unfolding Drawing | 11 | **Blank map:** real blank extended to straight, length L_blank |
| 3.11 | Drawing Dimensions | 11 | Sections with allowances, between bends, arc lengths, totals |
| 3.12 | Allowance Display | 11 | Hatching, cut line, "trim" label with size Δ |
| 3.13 | Drawing Dialog | 11 | Scale, orientation, dimension base, checkboxes: B/A, numbering, arc lengths, allowances, break |
| 3.14 | Bending Sheet | 29 | **One calculation, two formats:** operator worksheet |
| 3.15 | Sheet Header | 29 | Position, ∅×wall, material, machine, feed end, blank length |
| 3.16 | Mark Accounting | 29 | From blank end to trim (includes initial allowance) |
| 3.17 | First Section on Machine | 13 | = S_first + Δ_start |
| 3.18 | Machine Export | 13 | Dialog: profile, format, filename, preview; blocked on error |
| 3.19 | Precision | 13 | Rounding on output only; calculate exactly |
| 3.20 | Table Import | 13 | Command "Tube from Table": XYZ or L–A–B from CSV → "Table" mode |

---

## SECTION 4: ANALYSIS - 19 Functions

| # | Function | Source Turn | Description |
|---|----------|------------|-------------|
| 4.1 | Check All | 15 | Collects violations in one table |
| 4.2 | Collisions | 15 | Self & environment detection |
| 4.3 | Gap Analysis | 15 | Distance to components |
| 4.4 | Measure | 15 | Distances, angles, lengths along centerline |
| 4.5 | Mass | 15 | Finished tube from material & wall |
| 4.6 | Report | 15 | Length, bends, ∅, wall, mass, profile; export PDF/CSV |
| 4.7 | Check Panel | 15 | Table: type, location, message, status |
| 4.8 | Panel Navigation | 15 | Click row → jump to section in 3D & bend table |
| 4.9 | Collected Checks | 15 | Limits between bends, bend angle, ∅ outside profile |
| 4.10 | Check States | 15 | Unchecked → checking → passed/violations → stale |
| 4.11 | Element Icon | 15 | Worst status (green/yellow/red) |
| 4.12 | Self-Intersection | 15 | Non-adjacent sections: distance < outer ∅; adjacent through bend skipped |
| 4.13 | SI Algorithm | 15 | Coarse (bounding boxes) → exact (min segment–segment, arc–segment, arc–arc) |
| 4.14 | Environment Collisions | 15 | Dialog: select components, mode (intersection/gap), value mm |
| 4.15 | Collision Precision | 15 | Exact per B-rep of imported bodies |
| 4.16 | Exclusions | 15 | Body with port auto-excluded in port zone |
| 4.17 | Collision Result | 15 | Zones red in 3D, list in panel |
| 4.18 | Check Modes | 15 | Quick (limits) instant; collisions by command or background with delay |
| 4.19 | SI Classification | 15 | **Technology error** (blocks export) |

---

## SECTION 5: ASSEMBLY WITH IMPORT - 21 Functions

| # | Function | Source Turn | Description |
|---|----------|------------|-------------|
| 5.1 | Import Formats | 3 | STEP, Parasolid, SAT, IGES; environment as Ground (fixed) |
| 5.2 | Recognition on Import | 3 | Groups: Tubes, Environment (port candidates, support geometry, fittings) |
| 5.3 | Recognize All Checkbox | 3 | Single option for all groups |
| 5.4 | Recognize Command | 3 | Post-import: Tubes / Environment / All; on any tree/3D selection |
| 5.5 | Recognition Results | 3 | Table with checkboxes, like import dialog |
| 5.6 | Port (End Point) | 3 | Connection type, ∅, direction, **minimum straight offset** |
| 5.7 | Port Dialog | 3 | Tabbed parameters |
| 5.8 | Offset Validation | 3 | **Offset < minimum → technology error**; new port offset = 0 |
| 5.9 | Port Candidates | 3 | Recognized in environment; click mark creates port |
| 5.10 | Trace Command | 3 | "Route Tube": start port → cursor leads → snap to axes/edges/planes |
| 5.11 | Trace Snap | 3 | Hole axes, edges, planes; Tab switches axis |
| 5.12 | Trace Goal | 3 | Second port |
| 5.13 | Supports | 3 | Clamps, brackets with gap zones |
| 5.14 | Gap Checking | 3 | Distance between tube and support |
| 5.15 | Associativity | 3 | Path references environment geometry |
| 5.16 | Update on Import | 3 | Tube recalculates automatically |
| 5.17 | Link Loss | 3 | "Recover Link" dialog |
| 5.18 | Link States | 3 | Valid / stale / lost; adaptive / frozen |
| 5.19 | Support Geometry | 3 | Planes (walls), cylinder axes, beams with axes & shelves |
| 5.20 | Fitting Bodies | 3 | Tees, elbows with own ports for connections |
| 5.21 | On-the-Fly Fitting | 3 | Axis/plane locally fitted to grid during trace |

---

## SECTION 6: UI SHELL - 24 Functions + 11 Color Categories

### 6.1 Window Layout & Components

| # | Function | Source Turn | Description |
|---|----------|------------|-------------|
| 6.1 | Layout (top-to-bottom) | 312+ | Title bar → Ribbon → Work area → Document tabs → Status bar |
| 6.2 | Ribbon (Context Tabs) | 312+ | Home, Part, Assembly, Analysis, Drawing |
| 6.3 | Home Tab | 312+ | New document, open, save, print, options |
| 6.4 | Part Tab | 312+ | Sketch, Tube (new, edit), auxiliary geometry |
| 6.5 | Assembly Tab | 312 | Insert, import, port, trace, supports, associativity, recognize |
| 6.6 | Analysis Tab | 312+ | Check, collisions, measure, mass, report |
| 6.7 | Drawing Tab | 312+ | New sheet, view, tables, unfolding, annotations |
| 6.8 | Property Panel (right) | 312+ | No selection → document; with selection → by type (tube, vertex, component, etc.) |
| 6.9 | Multiple Selection | 312+ | Edit applies to all in one transaction |
| 6.10 | Overridden Values | 312+ | Icon badge; reset button "Reset to inherited" |
| 6.11 | Unit Input | 312+ | Alternative units recalculated (e.g., "1in") |
| 6.12 | Command Parameters | 312+ | Panel shows command params during operation |
| 6.13 | Build Tree (left) | 312+ | State icons, rollback bar, suppression, drag reorder, context menu |
| 6.14 | State Icons | 312+ | Normal / warning / error (color badges) |
| 6.15 | Rollback Bar | 312+ | Revert to any build stage |
| 6.16 | Suppression | 312+ | Hide without delete |
| 6.17 | Tree Drag | 312+ | Reorder elements |
| 6.18 | Tree Context Menu | 312+ | Create, edit, delete, properties |
| 6.19 | Selection Filters | 312+ | Vertex, segment, bend, body, port, auxiliary |
| 6.20 | Pre-Highlight | 312+ | On cursor hover, object highlights |
| 6.21 | Dual Window Selection | 312+ | Independent; "Synchronize" toggle (default off) |
| 6.22 | Undo/Redo | 312+ | One operation = one step |
| 6.23 | File Menu | 312+ | Full-screen page: New, Open, Save, Save As, Save Copy |
| 6.24 | Multi-Window | 312+ | Document windows, auxiliary windows, floating panels, drag as copy |

### 6.2 Icon Color Scheme (11 Categories)

| # | Category | Hex Color | Used For | Example Commands |
|---|----------|-----------|----------|-------------------|
| 1 | **Tube** | #1F6FC5 (Blue) | Tube body, main elements | New Tube, Finished Tube Length |
| 2 | **Create** | #2E9E5B (Green+) | Object creation | New Sketch, Insert Component |
| 3 | **Delete/Prohibition** | #D93A3A (Red) | Delete, prohibition, errors | Delete Element, Clear |
| 4 | **Import** | #0E97A3 (Cyan) | Imported data, mesh | Import STEP, Recognize Tubes |
| 5 | **Environment Links** | #8A4FD8 (Violet) | References to environment | Port, Support, Trace |
| 6 | **Edit/Move** | #E8820C (Orange) | Parameter change, movement | Edit Tube, Move Component |
| 7 | **Update** | #F5B517 (Yellow) | Recalculate, synchronize | Refresh, Update Import |
| 8 | **Freeze** | #2D9CDB (Ice Blue) | Lock, freeze state | Freeze Link, Lock Parameter |
| 9 | **Auxiliary** | #C9920A (Ochre) | Help geometry | Coordinate System, Reference Plane |
| 10 | **RGB Axes** | RGB (#FF0000/#00FF00/#0000FF) | Coordinate axes | X-axis, Y-axis, Z-axis |
| 11 | **Neutral** | #757575 (Gray) | Outlines, shadows | Contour, Background |

---

## TECHNOLOGY CONSTRAINTS (Critical)

From **Turn 6:**

1. **Bend Radius R** depends on instrument (manual/mechanized; CNC out of v1) and diameter — one value for each pair
2. **Minimum straight between bends** — profile-specified limit
3. **Minimum initial/final blank sections** (N_start, N_end) for clamping and feed
4. **Trim allowance optional** — calculated if section shorter than minimum
5. **Real geometry NOT modeled:** ideal cylinders and tori only, no springback, thinning, or ovality

---

## SUMMARY STATISTICS

| Point | Functions | Key Turns | Coverage |
|-------|-----------|-----------|----------|
| 1. Path | 18 | 5-7, 18+ | Sketch, solver, constraints, tech limits |
| 2. Tube | 19 | 7-18, 200+ | Profile, dialog, algorithm, states |
| 3. Table | 20 | 13-14, 29+ | L-R-A, modes, unfolding, export |
| 4. Analysis | 19 | 15, 29+ | Checks, collisions, diagnostics |
| 5. Assembly | 21 | 3, 29+ | Import, ports, trace, associativity |
| 6. UI | 24+11 | 200+, 312 | Ribbon, tree, panels, colors |
| **TOTAL** | **~120+11** | **All** | **Complete specification** |

---

## REFERENCE INTERFACE

**Assembly-Color.dc.html**
- **Session:** 98d0d30b turn 312
- **Artifact:** https://claude.ai/artifact/V4JBXkifhgoocMHMkyub4J
- **Status:** ✅ PRODUCTION STANDARD
- **Color Palette:** 11 semantic categories
- **Requirement:** All implementations must conform to this design

---

## VERIFICATION CHECKLIST

- ✅ Reference interface locked (Assembly-Color.dc.html)
- ✅ All 6 functional points specified
- ✅ ~120 functions enumerated with turns
- ✅ 11 color categories defined with hex codes
- ✅ Technology constraints documented
- ✅ Secondary sessions reviewed (aligned, no contradictions)
- ✅ Specification ready for implementation

---

## SECTION 7: IMPLEMENTATION MODULES (Phase 23-24 Extensions from Session 0c7f9aed)

### 7.1 Positioning Module (bendcore::positioning)
**From realization Phase 23 Stage 3**
- Frame transformation: local path coordinates → assembly coordinates
- Port placement: `place_from_port(path, port, roll_deg)` with end-point verification
- Roll solving: golden-section search to minimize end B connection error
- Clearance checking: gap control between tube centerlines (5mm default)
- End connection validation: diameter, offset, position, direction tolerances
- Functions: `frame_from_port`, `solve_roll`, `check_clearance`, segment/polyline distance
- Status: ✅ Complete (commit 79e2afb)

### 7.2 CAM Module (bendcore::cam)
**From Phase 24 Stage 5**
- Production job generation: bend operation lists for manual and mechanized benders
- G-code generation: abstract macro format with parameters and comments
- Operation timing: linear model (~20°/sec + feed time)
- Batch planning: group operations by time and resources
- Structures: `BendOperation`, `ProductionJob`, `RollSolution`
- Status: ✅ Stub ready for extension (commit 154b9ef)

### 7.3 Web Viewer (Three.js Interactive Visualization)
- Scene loading from scene.json or live JSON
- Centerline rendering as polylines (64-point arcs for 90°)
- Port visualization with direction vectors
- Color-coded state: normal (green), warning (yellow), error (red), gap marked
- Camera control: fit-all, fit-selected, arbitrary rotation with mouse
- Independent selection in multiple viewports

### 7.4 Auxiliary Geometry (Extension to Section 6: UI Shell)
**New command group in Ribbon tabs: Part, Assembly**
- Planes: by 3 points, plane + axis, bend plane, normal to segment
- Axes: through 2 points, along edge, from cylinder/hole, intersection of planes
- Points: vertex, circle center, on curve by distance, bend touch, arc center
- Coordinate systems: by 3 points, plane + axis, port frame
- Associativity: recalculate on reference update, frozen values for non-live tubes
- Storage: in part (from tube) or assembly (from environment)
- UI: tree node "Auxiliary Geometry" with state icons, full context menu

---

## INTEGRATION NOTES

**Phases Covered:**
- **Main specification (98d0d30b):** Phases 1-27 (complete CAD system foundation)
- **Implementation details (0c7f9aed):** Phase 23 Stage 3 (positioning), Phase 24 Stage 5 (CAM stub)
- **No contradictions:** Extensions align with core specification

**Known Limitations (as of 2026-10-22):**
1. Assembly list hierarchy (multi-component documents) — Phase 25+ future
2. Solid-geometry environment collision — requires B-rep library
3. Python reference implementation for positioning — pending
4. Spatial indexing for clearance checks — O(n²) current, needs BVH for 1000+ tubes
5. WASM bindings for performance — roadmap item

**Scalability Path:**
- Current: AABB culling + pairwise comparison
- Recommended: BVH (Bounding Volume Hierarchy) or k-d tree for production use

---

**CLASSIFICATION: OFFICIAL BENDCORE CAD SPECIFICATION**  
*Locked for Production | Last Updated: 2026-10-22*  
*Coverage: Main specification (98d0d30b, 1202 turns) + Implementation details (0c7f9aed, Phases 23-24)*
