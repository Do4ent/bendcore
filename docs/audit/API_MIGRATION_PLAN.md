# API Code Migration Plan

## Status: LEGACY CODE - DEPRECATION REQUIRED

### Finding
`src/api/` contains Express/Node.js API routes that:
- Require `express` and `multer` packages
- Are located in the frontend codebase (`src/`) but have backend dependencies
- Are not imported or used by any frontend component
- Have no corresponding frontend integration tests

### Root Cause
The API routes were likely:
1. Copied from an earlier backend implementation
2. Left in the frontend source tree when backend was separated
3. Never properly integrated or removed

### Evidence
- `src/api/formula_routes.ts`, `import_routes.ts`, etc. import express/multer
- `backend/package.json` has express/multer; main `package.json` does not
- No imports of src/api in any `.ts` or `.tsx` files in `src/ui/` or `src/features/`
- No test coverage for API routes
- tsconfig.json excludes src/api from main compilation

### Resolution Options

**Option 1: Move to backend** (if code is actually used)
- Move `src/api/` → `backend/src/api/`
- Keep in backend TypeScript project
- Add proper tests in backend
- Document backend API in backend README

**Option 2: Delete** (if code is truly obsolete)
- Remove `src/api/` directory
- Remove `tsconfig.api.json`
- Remove `npm run typecheck:api` script
- Update documentation

### Current Status
- TypeScript check for API excluded from main `npm run typecheck`
- Separate `npm run typecheck:api` exists but fails (can be skipped in CI)
- No CI requirement to pass API checks

### Recommendation
Until a decision is made:
1. ✅ Keep `tsconfig.api.json` with Node.js types (for developers who want to check)
2. ✅ Keep `npm run typecheck:api` script (optional, documented as legacy)
3. ✅ Do NOT block CI on API TypeScript errors
4. ⚠️ Document that src/api is legacy code awaiting migration/retirement
5. 📋 Create GitHub issue to track resolution

### Acceptance Criteria for Closure
- [ ] Decide: migrate to backend or delete?
- [ ] If migrate: move files and tests
- [ ] If delete: remove directory and scripts
- [ ] Update documentation to reflect decision
- [ ] CI reflects the decision (pass or fail appropriately)

