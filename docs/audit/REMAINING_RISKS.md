# Remaining Risks Assessment

**Last Updated**: October 31, 2026  
**Status**: Independent audit findings  
**Baseline**: Commit 1dd73d9

---

## Risk Matrix

| Risk | Priority | Probability | Impact | Effort to Fix | Status |
|------|----------|-------------|--------|---------------|--------|
| Windows build untested | P1 | High | High | Medium | UNRESOLVED |
| E2E tests blocked | P1 | High | Medium | Low | UNRESOLVED |
| CAD file acceptance untested | P1 | High | Medium | High | UNRESOLVED |
| Code signing missing | P1 | Medium | Medium | Medium | UNRESOLVED |
| API code undefined | P1 | Low | Low | Medium | DOCUMENTED |
| Bundle size warnings | P2 | Low | Low | High | DOCUMENTED |

---

## Detailed Risk Assessment

### 1. Windows Build Untested [P1 - HIGH RISK]

**Description**
Electron application for Windows has never been successfully built and launched in production.

**Evidence**
- electron-builder configuration verified as syntactically correct
- GitHub Actions workflow structure is proper
- BUT: Cloud Linux environment cannot execute Windows builds
- No evidence that .exe/.msi files are actually generated
- No smoke test confirming Windows launch

**Failure Scenarios**
1. electron-builder fails silently in Windows runner
   - Artifacts not created
   - CI shows "success" with empty release/ directory

2. .exe generated but doesn't launch
   - Missing dependencies (node_modules distribution)
   - Incorrect main.js path
   - Missing dist/web/index.html

3. NSIS installer fails
   - Icon files missing
   - Registry issues
   - Shortcut creation failures

4. Portable executable doesn't work
   - Incorrect file extraction
   - Dependency resolution issues

**Mitigation**
✓ Configuration validated
✓ Artifact upload uses v4 (reliable)
⏸️ REQUIRES Windows runner execution before release

**Resolution Path**
1. Trigger Windows build on this PR (via GitHub Actions)
2. Monitor Windows job for success/failure
3. Download artifacts and test locally on Windows machine
4. Confirm application launches and basic functionality works
5. Sign off Windows build validation

**Timeline**: Before release to production
**Owner**: DevOps/Release team

---

### 2. E2E Tests Blocked [P1 - MEDIUM RISK]

**Description**
End-to-end tests are defined but cannot run due to Playwright configuration error.

**Current State**
```
npm run test:e2e
Error: Process from config.webServer was not able to start
npm error: No workspaces found: --workspace=@bendcore/backend
```

**Evidence**
- 4 E2E test suites exist: parallel-offset, combined-3d, offset-integration, production
- Playwright config expects npm workspaces (doesn't exist)
- Backend is separate directory, not workspace
- E2E tests have never been executed

**Impact**
- No validation of complete CAD workflows
- GUI interactions untested
- Real browser rendering untested
- Complex offset calculations untested at integration level

**Failure Scenarios**
1. Offset calculations produce wrong results in GUI
   - Unit tests pass but GUI has bugs
   - User perceives "broken" functionality

2. UI state management issues
   - Feature tree out of sync
   - Properties panel shows stale data
   - Undo/redo doesn't work correctly

3. Browser compatibility issues
   - Works locally, fails in CI Chrome
   - Timing issues with async operations
   - Race conditions in state updates

**Mitigation**
✓ Unit tests pass (1217 tests)
⏸️ E2E tests need playwright config fix
⏸️ Simple fix: Update webServer command from `--workspace=@bendcore/backend` to `cd backend && npm run dev`

**Resolution Path**
1. Update playwright.config.ts webServer commands
2. Ensure both frontend and backend dev servers start
3. Run E2E tests: `npm run test:e2e`
4. Fix any test failures
5. Add to CI pipeline

**Timeline**: Next sprint
**Effort**: Low (config fix) + Medium (fix any failing tests)
**Owner**: QA/Test team

---

### 3. CAD File Acceptance Untested [P1 - MEDIUM RISK]

**Description**
Real CAD files have never been imported and validated in the application.

**Current Testing**
- ✓ Unit tests with mock data: 1217 tests pass
- ✗ Real .bdc file import: Not tested
- ✗ Real STEP/IGES/DXF import: Status unknown
- ✗ Geometry preservation on save/reload: Tested with mocks only
- ✗ Complex parametric designs: Not tested
- ✗ Large designs (>1000 features): Not tested

**Failure Scenarios**
1. File format parsing issue
   - Legacy file format incompatibility
   - Newer format changes break old files
   - Import silently corrupts geometry

2. Geometry discontinuity
   - Centerline breaks on import
   - Bend angles calculated incorrectly
   - Tangency lost on save/reload

3. Constraint solver issues
   - Complex designs cause infinite loops
   - Performance degradation (hours to compute)
   - Numerical stability issues with small/large coordinates

4. UI state issues with real data
   - Tree panel doesn't render 1000+ items
   - Property updates lag
   - Memory leaks with large files

**Mitigation**
✓ Unit geometry tests pass (comprehensive)
✗ Real file acceptance tests missing
✗ No production sample files defined

**Resolution Path**
1. Collect or create production test CAD files
   - Simple: single straight tube
   - Complex: multi-segment with bends
   - Large: 1000+ feature design (if applicable)

2. Create acceptance test suite
   - Import file via UI
   - Verify geometry (centerline, angles, radius)
   - Edit properties, save, reload
   - Verify undo/redo
   - Export and re-import (if supported)

3. Automate in CI
   - Run tests on every commit
   - Track geometry validation metrics
   - Alert on regressions

**Timeline**: Before 1.0 release
**Effort**: High (create test files + test scenarios)
**Owner**: CAD engineering team

---

### 4. Code Signing Missing [P1 - MEDIUM RISK]

**Description**
Windows executables are unsigned, causing SmartScreen warnings and user trust issues.

**Current State**
```
"win": {
  "certificateFile": null,
  "certificatePassword": null,
  "sign": null
}
```

**Impact**
- Users see "Unknown publisher" warning
- SmartScreen may block execution
- Enterprise deployments may reject
- Appears unprofessional/untrustworthy

**Failure Scenarios**
1. User unwilling to click "Run anyway"
   - Reduced adoption
   - Support burden explaining warning

2. Enterprise security blocks executable
   - Cannot deploy in corporate environments
   - Lost market segment

3. Malware-like perception
   - Even though app is legitimate
   - Damages brand/trust

**Mitigation**
✓ App is legitimate (open source)
✗ No code signing certificate
✗ Not required for testing, only production

**Resolution Path**
1. Acquire EV (Extended Validation) code signing certificate
   - Cost: $300-500/year
   - Authority: DigiCert, Sectigo, etc.
   - Validation time: 1-3 days

2. Update electron-builder config
   ```javascript
   "win": {
     "certificateFile": "path/to/certificate.pfx",
     "certificatePassword": process.env.WINDOWS_CERT_PASSWORD,
     "signingHashAlgorithms": ["sha256"]
   }
   ```

3. Configure signing in CI
   - Store certificate securely in GitHub Secrets
   - Sign during Windows build step
   - Verify signature in release artifacts

**Timeline**: Before 1.0 release
**Effort**: Medium (cert acquisition + CI setup)
**Cost**: $300-500/year
**Owner**: DevOps/Release team

---

### 5. API Code Status Undefined [P1 - LOW RISK]

**Description**
`src/api/` contains backend code in frontend repository with unresolved status.

**Current State**
- Directory: `src/api/` (Express routes, multer handlers)
- Dependencies: express, multer (not in frontend package.json)
- Used by: No frontend components
- Tests: None
- TypeScript: 26 errors

**Options**
1. **Migrate to backend**: Move `src/api/` → `backend/src/api/`
   - Effort: Low (copy + update imports)
   - Benefit: Proper organization
   - Risk: None (nothing uses it)

2. **Delete**: Remove `src/api/` entirely
   - Effort: Low (delete + remove scripts)
   - Benefit: Cleaner codebase
   - Risk: Might be historical code someone needs

3. **Keep**: Document as deprecated, don't enforce TypeScript checks
   - Effort: None (already done)
   - Benefit: Preserves history
   - Risk: Code rot, confusion

**Current Status**: OPTION 3 (documented deprecation)

**Resolution Path**
1. Decide: Is API code needed?
   - Review git history to understand origin
   - Check if anyone has requirements for it
   - Discuss with team

2. If delete:
   - Remove `src/api/` directory
   - Remove `tsconfig.api.json`
   - Remove `npm run typecheck:api` script
   - Update documentation

3. If migrate:
   - Move to backend
   - Update package.json dependencies
   - Add backend tests
   - Update documentation

4. If keep:
   - Document as deprecated
   - Explain why it's not deleted
   - Plan for future decision

**Timeline**: Before 1.0 release (decision only)
**Effort**: Low (once decision made)
**Owner**: Technical lead

---

### 6. Bundle Size Warnings [P2 - LOW RISK]

**Description**
Webpack generates 3 performance warnings about bundle size.

**Current State**
```
WARNING: vendor bundle exceeds 500 KiB (599 KiB)
WARNING: entrypoint exceeds 500 KiB (1.16 MiB)
Main culprit: three.js (1.09 MiB, essential for 3D rendering)
```

**Assessment**
- **Normal for 3D CAD**: three.js is necessary
- **Unavoidable**: 3D graphics libraries are large
- **Acceptable**: 1.16 MiB total is reasonable for CAD software

**Could Be Reduced By**
1. Lazy-load three.js
   - Only load when 3D viewport needed
   - Effort: High (major refactoring)
   - Benefit: Faster initial load
   - Trade-off: More complex code

2. Three.js alternatives
   - Babylon.js (similar size)
   - Custom WebGL (massive effort, not recommended)
   - Result: No improvement likely

3. Code splitting
   - Already optimized by webpack
   - Marginal gains possible

**Recommendation**: ACCEPT as necessary
- 1.16 MiB for a 3D CAD application is very reasonable
- Modern browsers/network handle this fine
- Optimization would add complexity without significant benefit

**Timeline**: No action required
**Risk Level**: Very Low

---

## Summary of Unresolved Issues

### Before Release Checklist
- [ ] Windows build executed and tested on Windows runner
- [ ] E2E tests fixed and all passing
- [ ] CAD file acceptance tests created and passing
- [ ] Code signing certificate acquired and configured
- [ ] API code status decided (migrate, delete, or document)

### Test Coverage Summary
| Component | Unit Tests | E2E Tests | Acceptance |
|-----------|-----------|-----------|-----------|
| Frontend UI | ✓ 1217 | ⚠️ BLOCKED | ❌ NO |
| Backend API | ✓ 130 | ⚠️ BLOCKED | ⚠️ PARTIAL |
| 3D Rendering | ✓ PASS | ⚠️ BLOCKED | ❌ NO |
| CAD Geometry | ✓ PASS | ⚠️ BLOCKED | ❌ NO |
| User Workflows | ⚠️ LIMITED | ⚠️ BLOCKED | ❌ NO |

---

## Confidence Assessment

**Unit Testing**: HIGH ✓
- 1217 frontend tests pass
- 130 backend tests pass
- Good coverage of core components

**Integration Testing**: MEDIUM ⚠️
- E2E tests blocked
- No cross-component workflow validation
- API integration not tested

**Acceptance Testing**: LOW ❌
- No real CAD files tested
- No production workflows validated
- No performance testing

**Production Readiness**: LOW ⚠️
- Cannot deploy to Windows without verification
- Cannot claim CAD correctness without real file testing
- Signing missing

---

## Recommendations

**Immediate (This PR)**
✓ Apply P0/P1 fixes
✓ Document gaps
✓ Continue development

**Next Phase (1-2 weeks)**
- [ ] Fix Playwright config and run E2E tests
- [ ] Test Windows build on GitHub Actions
- [ ] Acquire code signing certificate

**Before Release (4-8 weeks)**
- [ ] Complete CAD acceptance test suite
- [ ] Validate on real production files
- [ ] Sign Windows executables
- [ ] Performance test on large designs
- [ ] Complete API code decision

---

## Conclusion

**The application is not yet production-ready**, but:
- ✓ Core functionality is well-tested (unit level)
- ✓ Critical CI defects have been fixed
- ✓ Gaps are documented and manageable
- ✓ Clear path to resolution exists

**Recommendation**: Merge current fixes, continue work on remaining items via separate PRs/tasks.
