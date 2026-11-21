/**
 * Localized messages of the C++ kernel (docs/ui/APP_SHELL.md, "Сообщения ядра"). The kernel
 * reports an ErrorCode and an English text (KernelError { code, detail }, messages of feature
 * nodes). The interface shows:
 *   - for a text matching one of KERNEL_TEMPLATES — the localized text of the template with the
 *     values (names, numbers) of the original; terms ({op:op}, {kind:kind}) are translated, a
 *     nested message ({x:msg}) must itself match a template;
 *   - for any other text — the localized title of the code, the original in the details.
 * Arbitrary text is never translated word by word. The coverage test
 * (src/ui/app/__tests__/kernelMessages.test.ts) reads every message of src/modeling,
 * src/assembly_v2 and src/drawing_v2 and requires a template or an entry of
 * kernelMessagesOther.json for each.
 *
 * Fields: {name} — any text (numbers are written without trailing zeros), {name:id} — one
 * word without spaces, quotes or colons, {name:num} — a number, {name:op} / {name:kind} — a term
 * of the dictionary (kernel.op.* / kernel.kind.*), {name:msg} — a nested kernel message.
 */

import { isMessageKey } from './i18n';
import type { MessageKey, Translate } from './i18n';

/** [template id (kernel.msg.<id>), English pattern of the kernel message] */
export const KERNEL_TEMPLATES: readonly (readonly [string, string])[] = [
  ['openContourRegions', 'an open-contour sketch has no regions to select'],
  ['noMaterialRegion', 'the sketch profile has no material region'],
  ['noClosedProfile', 'sketch \'{id}\' has no closed profile'],
  ['regionsWithoutCurves', 'regions are selected but the sketch has no profile curves'],
  ['wireFromEntities', 'cannot make a wire from sketch entities{ids}'],
  ['notOnOuterContour', 'sketch entity {id} is not on the outer contour of any region'],
  ['constraintsConflict', 'sketch constraints conflict:{list}'],
  ['constraintsRedundant', 'redundant sketch constraints:{list}'],
  ['sketchNotSolved', 'sketch solver did not satisfy constraint #{n} ({type}), residual {r}'],
  ['lineEqualEnds', '{where}: line with equal end points'],
  ['radiusPositive', '{where}: radius must be positive, is {r}'],
  ['noPathCurves', 'the sketch has no path curves'],
  ['pathChains', 'the sketch path consists of several separate chains'],
  ['pathNotConnected', 'path is not connected'],
  ['sweepPathStart', 'sweep path does not start in the profile plane'],
  ['ribOpen', 'rib contour must be open'],
  ['ribOpenEnds', 'rib contour must be open (its ends coincide)'],
  ['sketchOnFace', 'sketch on a face needs one face reference, has {n}'],
  ['profileNotPlanar', 'profile face is not planar'],
  ['profileZeroArea', 'profile face has zero area'],
  ['loftOneRegion', 'loft section \'{id}\' must have exactly one region, has {n}'],
  ['filletFailed', 'fillet radius {r:num} failed: {reason}'],
  ['filletRadiusPositive', 'fillet radius must be > 0'],
  ['filletRadiusPositiveIs', 'fillet radius must be positive: {r}'],
  ['chamferPositive', 'chamfer distance must be > 0'],
  ['chamferPositiveIs', 'chamfer distance must be positive: {d}'],
  ['chamferSecondPositive', 'second chamfer distance must be > 0'],
  ['chamferTooLarge', 'BRepFilletAPI_MakeChamfer failed (distance too large?)'],
  ['shellTooThick', 'BRepOffsetAPI_MakeThickSolid failed (thickness too large?)'],
  ['shellPositive', 'shell thickness must be > 0'],
  ['noEdges', 'no edges given'],
  ['noFaces', 'no faces given'],
  ['edgeNotOfBody', 'edge #{n} does not belong to the target body'],
  ['needsBody', '{op:op} needs an existing body'],
  ['bodiesNoIntersect', 'the bodies do not intersect'],
  ['removesTarget', 'the operation removes the whole target body'],
  ['toolsNoIntersect', 'the tool bodies do not intersect the target body'],
  ['cutNoIntersect', 'cut tool does not intersect the target body'],
  ['cutRemovesAll', 'cut removes every body of the part'],
  ['cutRemovesBody', 'cut removes the whole body'],
  ['patternNoIntersect', 'pattern instances do not intersect the target body'],
  ['occtException', 'OCCT exception in {where:id}: {what}'],
  ['occtExceptionShort', 'OCCT exception: {what}'],
  ['booleanTwoBodies', 'body boolean needs a part with at least two bodies'],
  ['booleanToolBody', 'body boolean needs at least one tool body'],
  ['toolIsTarget', 'tool #{n} is the target body'],
  ['toolTwice', 'tool #{n} is listed twice'],
  ['noBodyToDelete', 'no body to delete'],
  ['deleteAllBodies', 'cannot delete every body of the part'],
  ['noBodyToCopy', 'no body to copy'],
  ['noCopies', 'no copies requested'],
  ['unionNotTouch', 'union: {n} bodies do not touch'],
  ['toolSeparate', 'tool touches no body of the part: it forms a separate solid'],
  ['cutSplits', 'cut splits a body: the part has {n} solids'],
  ['resultSolids', 'result has {n} solids'],
  ['mirrorNotTouch', 'mirrored body does not touch the original: {n} solids'],
  ['copiesNotTouch', 'merged copies do not touch the body: {n} solids'],
  ['resultInvalid', 'result is not a valid shape:{what}'],
  ['resultNoVolume', 'result is not a solid with positive volume'],
  ['patternCounts', 'pattern counts must be >= 1'],
  ['circularCount', 'circular pattern count must be >= 2'],
  ['patternParallel', 'pattern directions must not be parallel'],
  ['patternNoInstances', 'pattern has no instances besides the original'],
  ['patternNoInstancesSeed', 'pattern has no instances besides the seed'],
  ['circularAngle', 'circular pattern angle must be non-zero'],
  ['brokenReference', 'broken reference {where}: {detail:msg}'],
  ['brokenReferenceRaw', 'broken reference {where}: {detail}'],
  ['resolvedGeometry', 'reference {where} resolved by geometry: {detail}'],
  ['resolvedHistory', 'reference {where} resolved through history: {detail}'],
  ['slotExactlyOne', 'reference slot \'{slot}\' needs exactly one {kind:kind}, has {n}'],
  ['slotAtLeastOne', 'reference slot \'{slot}\' needs at least one {kind:kind}, has 0'],
  ['slotEmpty', 'reference slot \'{slot}\' is empty'],
  ['removedByFeature', '{kind:kind} \'{name}\' was removed by feature \'{feature}\''],
  ['noNamedNoSignature', 'no {kind:kind} named \'{name}\' and the reference has no geometric signature'],
  ['noNamedNoMatch', 'no {kind:kind} named \'{name}\' and no {geometry} {kind2:kind} with a matching signature{rest}'],
  ['noNamedAmbiguous', 'no {kind:kind} named \'{name}\' and the signature is ambiguous{rest}'],
  ['referenceOneFace', 'reference \'{slot}\' must give one face'],
  ['referenceOneEdge', 'reference \'{slot}\' must give one edge'],
  ['referenceOnePlanar', 'reference \'{slot}\' must give one planar face, has {n}'],
  ['linkDatumPlane', 'link \'{link}\' must name a datum plane'],
  ['linkNotSet', 'link \'feature\' is not set'],
  ['linkSketch', 'link \'sketch\' must name a sketch'],
  ['extrudeDistance', 'extrude distance must be > 0'],
  ['cutTarget', 'cut requires a target body'],
  ['behindPlane', 'target body lies behind the sketch plane'],
  ['holeDiameter', 'hole diameter must be > 0'],
  ['holeDepth', 'hole depth must be > 0'],
  ['holeOutside', 'hole point is outside the face'],
  ['revolveAxisPlane', 'revolve axis must lie in the sketch plane'],
  ['revolveCrossesAxis', 'profile crosses the revolve axis'],
  ['cyclic', 'cyclic dependency {list}'],
  ['unknownFeatureType', 'unknown feature type \'{type}\''],
  ['noFeature', 'no feature \'{id}\''],
  ['featureExists', 'feature id \'{id}\' already exists'],
  ['notExecuted', 'not executed'],
  ['emptyExpression', 'empty expression'],
  ['cannotParse', 'cannot parse \'{expr}\': {why}'],
  ['unknownVariable', 'unknown variable \'{name}\''],
  ['noVariable', 'no variable \'{name}\''],
  ['variableError', 'variable \'{name}\': {detail:msg}'],
  ['notFinite', '\'{expr}\' is not finite'],
  ['cannotEvaluate', 'cannot evaluate \'{expr}\': {why}'],
  ['invalidVariableName', 'invalid variable name \'{name}\''],
  ['nothingToUndo', 'nothing to undo'],
  ['nothingToRedo', 'nothing to redo'],
  ['drawingNothingToUndo', 'drawing history: nothing to undo'],
  ['drawingNothingToRedo', 'drawing history: nothing to redo'],
  ['finishTransaction', 'commit or abort the open transaction first'],
  ['noTransaction', 'no open transaction'],
  ['counterboreDiameter', 'counterbore diameter must exceed the hole diameter'],
  ['counterboreDepth', 'counterbore depth must be > 0'],
  ['counterboreTooDeep', 'counterbore depth must be less than the hole depth'],
  ['countersinkDiameter', 'countersink diameter must exceed the hole diameter'],
  ['countersinkTooDeep', 'countersink is deeper than the hole'],
  ['holeIntoMaterial', 'hole direction must point into the material'],
  ['holeFaceNotPlanar', 'hole face is not planar'],
  ['holeFaceSingular', 'hole face surface is singular at the point'],
  ['holePointNoProjection', 'hole point cannot be projected on the face surface'],
  ['noRegionsSelected', 'no regions selected'],
  ['regionIndexRange', 'region index {n} out of range ({m} regions)'],
  ['regionSelectedTwice', 'region {n} selected twice'],
  ['regionsTouchAtPoint', 'selected regions touch only at a point: they cannot form one face'],
  ['mergedRegionsFace', 'cannot make the face of merged regions'],
  ['mergedRegionsInvalid', 'face of merged regions is invalid (area {a} vs {b})'],
  ['mergeRegionsFailed', 'merging regions failed: {what}'],
  ['profileGroupOutline', 'profile group starting at sketch entity {n} has no single outline'],
  ['regionTouchesItself', 'sketch profile: the boundary of region {n} touches itself at {p} (curves tangent inside a region or contours touching at a point)'],
  ['holeFaceOtherBody', 'hole face does not belong to the target body'],
  ['holePointOffPlane', 'hole point is not on the face plane'],
  ['draftAngleRange', 'draft angle must be non-zero and in (-pi/2, pi/2)'],
  ['draftAngleExtrude', 'draft angle must be in [0, pi/2)'],
  ['draftFaceNotOfBody', 'draft face #{n} is not a face of the body'],
  ['draftNotApplied', 'draft cannot be applied to face #{n}'],
  ['chamferAngle', 'chamfer angle must be in (0, pi/2)'],
  ['limitBehind', 'limit face lies behind the sketch plane'],
  ['upToLimit', 'up-to extent requires a limit face'],
  ['throughAllTarget', 'through-all extent requires a target body'],
  ['twoSidedZero', 'two-sided extrude has zero total length'],
  ['thinThickness', 'thin wall thickness must be > 0'],
  ['revolveAngle', 'revolve angle must be in (0, 2*pi]'],
  ['profileOnAxis', 'profile lies on the revolve axis'],
  ['loftTwoSections', 'loft needs at least two sections'],
  ['loftClosedThree', 'closed loft needs at least three sections'],
  ['loftSamePlane', 'sections #{a} and #{b} lie in the same plane'],
  ['loftContours', 'section #{n} has {k} contours, section #1 has {m}'],
  ['loftConsecutive', 'loft sections must be consecutive: \'{slot}\' follows an empty slot'],
  ['ribThickness', 'rib thickness must be > 0'],
  ['ribNoVolume', 'rib has no volume'],
  ['ribPlane', 'rib contour does not lie in the sketch plane'],
  ['noIsoThread', 'no metric thread M{d} in the ISO 261 table'],
  ['isoPitch', 'pitch {p} is not an ISO 261 pitch of M{d}'],
  ['threadLength', 'thread length must be > 0'],
  ['threadLongerDrill', 'thread length exceeds the drill depth'],
  ['threadLongerHole', 'thread length exceeds the hole length {l}'],
  ['gearModule', 'gear module must be > 0'],
  ['gearTeethRange', 'number of teeth must be in [3, 1000]'],
  ['gearTeethInteger', 'number of teeth must be an integer in [3, 1000]'],
  ['gearShift', 'profile shift coefficient must be in [-2, 2]'],
  ['gearWidth', 'face width must be > 0'],
  ['gearWidthDoc', 'gear face width must be > 0'],
  ['gearHelix', 'helix angle must be in [0, 45] degrees'],
  ['gearPointed', 'the teeth are pointed (tip thickness sa <= 0): reduce the profile shift'],
  ['gearCutBlank', 'gear cut needs a blank body'],
  ['gearUndercut', 'gear: undercut (z = {z} < z_min = {zmin} for x = {x}): the fillet cuts into the involute'],
  ['gearSmallTip', 'gear: small tip thickness sa = {sa} mm < 0.25 m'],
  ['gearTableFit', 'gear table does not fit the frame'],
  ['pathEmpty', 'path has no edges'],
  ['pathBranches', 'path branches: more than two edges meet at a point'],
  ['pathNotWire', 'path edges do not form a wire'],
  ['pathElementNotEdge', 'path element #{n} is not an edge'],
  ['sweepPathEmpty', 'sweep path is empty'],
  ['sweepNotSolid', 'sweep of contour #{n} is not a solid'],
  ['sweepInvalid', 'sweep of contour #{n} is invalid (self-intersecting path or profile)'],
  ['sweepPath', 'sweep path: {detail:msg}'],
  ['spacingNonZero', 'spacing{n:num} must be non-zero'],
  ['circularOverlap', 'circular pattern instances overlap after a full turn'],
  ['totalAngle360', 'total pattern angle must not exceed 360'],
  ['noBodyNamed', 'no body named \'{name}\''],
  ['faceOfNoBody', 'a face of slot \'{slot}\' belongs to no body'],
  ['noToolBody', 'feature \'{id}\' ({type}) has no tool body to repeat'],
  ['invalidFeatureId', 'invalid feature id \'{id}\''],
  ['noStandalonePoints', 'sketch \'{id}\' has no standalone points for hole centres'],
  ['rollbackRange', 'rollback position {n} is out of range'],
  ['assemblyRootInsert', 'the root assembly cannot be inserted'],
  ['occurrenceNotPart', 'occurrence {id} is not a part'],
  ['patternSeedGone', 'pattern seed no longer exists'],
  ['patternTwoMembers', 'pattern must have at least two members'],
  ['onlyTopFixed', 'only top-level occurrences can be fixed'],
  ['replacementKind', 'replacement must be of the same kind (part or assembly)'],
  ['replacementCycle', 'replacement creates a cycle of subassemblies'],
  ['negativeDensity', 'negative density of \'{name}\''],
  ['partNoGeometry', 'part \'{name}\' has no geometry: {why}'],
  ['readOnlySub', 'subassembly \'{name}\' comes from an external document and is read-only'],
  ['boltShort', 'bolt length {l} mm is too short'],
  ['screwShort', 'screw length {l} mm is too short'],
  ['noHoleRadius', 'no hole of radius {r} with an entry face along the insertion direction'],
  ['detailRadius', 'detail view: radius must be positive'],
  ['sheetTooSmall', 'sheet {name} is too small for the title block'],
  ['gostNoFormat', 'GOST 2.301 has no format {w}x{h}'],
  ['titleFieldFit', 'title block: text of field \'{field}\' does not fit its cell: {text}'],
  ['localDepthBeyond', 'local section: depth is beyond the model'],
  ['localDepthPositive', 'local section: depth must be positive'],
  ['localThreePoints', 'local section: the boundary needs at least 3 points'],
  ['steppedTwoPlanes', 'stepped section: needs at least two plane points'],
  ['cuttingTraceMisses', 'cutting trace: the plane misses the view'],
  ['sectionBooleanFailed', 'section: boolean cut failed'],
  ['specNotJson', 'specification: not a JSON document'],
  ['specNotDocument', 'specification: not a bendcore-specification file'],
  ['loftPrefix', 'loft: {detail:msg}'],
  ['ribPrefix', 'rib: {detail:msg}'],
  ['partPrefix', 'part \'{name}\': {detail:msg}'],
  ['componentPrefix', 'component \'{name}\': {detail:msg}'],
  ['sweepRegionPrefix', 'sweep{where}: {detail:msg}'],
  ['notJson', 'not JSON: {what}'],
  ['notDocument', 'not a {format} document'],
  ['newerFormat', 'format version {v} is newer than the supported {s}'],
  ['invalidFormatVersion', 'invalid format version {v}'],
  ['malformed', 'malformed document: {what}'],
  ['assemblyNotJson', 'assembly document is not JSON: {what}'],
  ['assemblyNotDocument', 'not a bendcore assembly document (format != \'bendcore-assembly\')'],
  ['assemblyNewer', 'assembly document version {v} is newer than supported {s}'],
  ['assemblyMalformed', 'assembly document: {what}'],
  ['drawingNotJson', 'drawing: not a JSON document'],
  ['drawingNotDocument', 'drawing: not a bendcore-drawing file'],
  ['drawingNewer', 'drawing: file version {v} is newer than {s}'],
  ['drawingOpen', 'drawing: cannot open \'{path}\''],
  ['cannotOpen', 'cannot open \'{path}\''],
  ['cannotRead', 'cannot read \'{path}\''],
  ['cannotOpenWrite', 'cannot open \'{path}\' for writing'],
  ['cannotWrite', 'cannot write \'{path}\''],
  ['partFileType', 'unsupported part file type \'{ext}\' (expected .json, .step, .stp, .brep)'],
  ['placementNotRigid', 'placement with scale or mirror is not a rigid motion'],
  ['dxfNoUnits', 'the DXF file has no $INSUNITS; choose the drawing units'],
  ['dxfNoLayer', 'the DXF file has no layer \'{name}\''],
  ['dxfUnknownUnits', 'unknown DXF units code {code}'],
  ['dxfBinary', 'binary DXF is not supported; save the drawing as ASCII DXF'],
  ['assemblyCycle', 'inserting \'{a}\' into \'{b}\' creates a cycle'],
  ['mateSeed', '{c} is a pattern instance; mate the pattern seed instead'],
  ['unknownComponent', '{id}: unknown component id'],
  ['dragFixed', 'drag: component is fixed'],
  ['mateEntity', 'mate element must be a face, an edge or a vertex'],
  ['sectionMisses', 'section: the cutting plane does not intersect the model'],
  ['scaleNotFit', 'scale: the view does not fit even at 1:1000'],
  ['layoutNotFit', 'layout: the views do not fit the area at 1:1000'],
  ['localSectionNoCut', 'local section: the region does not cut the model'],
  ['localSectionSelf', 'local section: the boundary intersects itself'],
  ['asmDensity', 'density must be >= 0'],
  ['cannotLoadPart', 'cannot load part \'{name}\': {detail:msg}'],
  ['unknownDefinition', 'unknown definition {id}'],
  ['unknownParentDefinition', 'unknown parent definition {id}'],
  ['parentNotAssembly', 'parent \'{name}\' is not an assembly'],
  ['unknownOccurrenceName', 'unknown occurrence {id:num}'],
  ['occurrenceReadOnly', '{who} belongs to external subassembly \'{name}\' and is read-only'],
  ['patternInstanceRemove', '{id} is a pattern instance; remove the pattern'],
  ['placedByPattern', '{id} is placed by its pattern'],
  ['occurrenceNameEmpty', 'occurrence name must not be empty'],
  ['occurrencePathEmpty', 'occurrence path is empty'],
  ['notPartOf', '{id} is not part of \'{name}\''],
  ['pathEndsAtAssembly', 'path ends at assembly \'{name}\', not at a part'],
  ['elementIndexRange', '{kind:kind} index {i} is out of range in \'{name}\' ({n})'],
  ['elementRefEmpty', 'element reference has neither index nor name'],
  ['partNoElementMap', 'part \'{name}\' has no element map; cannot resolve name \'{ref}\''],
  ['nameResolvesMany', 'name \'{ref}\' resolves to {n} elements in \'{name}\''],
  ['nameNotFound', 'name \'{ref}\' not found in \'{name}\': {why}'],
  ['unknownMate', 'unknown mate {id}'],
  ['mateEmptyPath', 'mate reference has an empty occurrence path'],
  ['notTopLevel', '{id} is not a top-level occurrence'],
  ['notDraggable', '{id} is not a top-level occurrence that can be dragged'],
  ['patternInstanceReplace', '{id} is a pattern instance; replace the seed'],
  ['invalidReplacement', 'invalid replacement definition {id}'],
  ['definitionNotPart', 'definition {id} is not a part'],
  ['partNoSource', 'part \'{name}\' has no source document'],
  ['occtPlacing', 'OCCT exception while placing parts: {what}'],
  ['unknownExploded', 'unknown exploded view {id}'],
  ['progressFinite', 'progress must be finite'],
  ['explodeStep', 'explode step {n} has an invalid direction or distance'],
  ['explodeStepTop', 'explode step {n}: {id} is not a top-level occurrence'],
  ['partDocNoShape', 'part document has no valid shape'],
  ['occtLoadingPart', 'OCCT exception while loading part: {what}'],
  ['tolerancesPositive', 'tolerances must be >= 0'],
  ['intersectionOf', 'intersection of \'{a}\' and \'{b}\': {detail:msg}'],
  ['distanceFailed', 'distance between \'{a}\' and \'{b}\' failed'],
  ['occtMateGeometry', 'OCCT exception while reading mate geometry: {what}'],
  ['elementIndexOf', '{kind:kind} index {i} is out of range ({n} elements)'],
  ['mateEntityUnsupported', '{what} cannot be used as a mate entity (use {use})'],
  ['patternNoSeedHole', 'feature pattern has no resolved seed hole'],
  ['seedHoleNotCylinder', 'seed hole reference is not a cylindrical face'],
  ['seedHoleRadius', 'seed hole face does not match the requested radius {r}'],
  ['patternSeedLost', 'pattern {id:num}: seed hole lost: {detail:msg}'],
  ['patternPrefix', 'pattern {id:num}: {detail:msg}'],
  ['unknownPattern', 'unknown pattern {id}'],
  ['patternSeedTop', 'pattern seed must be a top-level occurrence'],
  ['patternSeedInstance', 'pattern seed is itself a pattern instance'],
  ['patternRefTop', 'pattern reference must be a top-level occurrence'],
  ['patternRefSeed', 'pattern reference must differ from the seed'],
  ['linearPatternDirection', 'linear pattern needs a non-zero direction and a finite spacing'],
  ['linearPatternSecond', 'second pattern direction must be non-zero with a finite spacing'],
  ['circularPatternCount', 'circular pattern needs count >= 2'],
  ['circularPatternAxis', 'circular pattern needs a valid axis and a non-zero angle step'],
  ['seedHolePathEmpty', 'seed hole reference has an empty path'],
  ['seedHoleCylinder', 'seed hole reference must be a cylindrical face'],
  ['holeRadiusPositive', 'hole radius must be positive'],
  ['cycleExternal', 'cycle of external subassemblies: {list}'],
  ['cannotReadSub', 'cannot read subassembly \'{name}\': {detail:msg}'],
  ['subassemblyPrefix', 'subassembly \'{name}\': {detail:msg}'],
  ['subSourceEmpty', 'subassembly source is empty'],
  ['occtSaving', 'OCCT exception while saving: {what}'],
  ['jsonSaving', 'JSON error while saving: {what}'],
  ['occtLoading', 'OCCT exception while loading: {what}'],
  ['inSource', 'in \'{name}\': {detail:msg}'],
  ['bearingNotInCatalogue', 'ball bearing {number} is not in the catalogue (GOST 8338: {list})'],
  ['bearingBoreNotInSeries', 'ball bearing with bore {d} mm is not in the catalogue series {series} (GOST 8338: {list})'],
  ['bearingRingsThin', 'bearing {number}: rings are too thin for the raceways'],
  ['bearingAttributeMissing', 'bearing attribute \'{key}\' is missing'],
  ['keyNoSection', 'key section {b}x{h} is not in GOST 23360 ({list})'],
  ['keyNoShaft', 'no GOST 23360 key for shaft diameter {d} mm (the table covers over 6 up to 110 mm)'],
  ['keyExecution', 'key execution must be 1, 2 or 3'],
  ['keyLengthSeries', 'key length {l} mm is not in the GOST 23360 series ({list})'],
  ['keyLengthRange', 'key {b}x{h} length must be in [{min}, {max}] mm, is {l}'],
  ['keyChamferRange', 'key chamfer s must be in [{min}, {max}] mm, is {s}'],
  ['gearChamferNegative', 'gear tooth chamfer must be >= 0'],
  ['gearChamferLarge', 'gear tooth chamfer {c} mm is too large: it must be below {a} mm (tip to form circle) and {b} mm (face width / 3)'],
  ['gearChamferStart', 'gear tooth chamfer is not built at the start face: the blank continues beyond the rim'],
  ['gearChamferEnd', 'gear tooth chamfer is not built at the end face: the blank continues beyond the rim'],
  ['gearChamferHelical', 'gear tooth chamfer is not built on helical teeth'],
  ['standardPartInvalid', 'standard part {what} is not valid (BRepCheck)'],
  ['standardPartRoleUnique', 'standard part face role \'{role}\' is not unique'],
  ['standardPartRoleMissing', 'standard part face role \'{role}\' not found'],
  ['standardPartNaming', 'standard part naming: {detail:msg}'],
  ['bearingStepFailed', 'bearing {what} failed'],
  ['keyStepFailed', 'key {what} failed'],
  ['keySolids', 'key body has {n} solids'],
  ['occtBearing', 'OCCT failure while building bearing: {what}'],
  ['occtKey', 'OCCT failure while building key: {what}'],
  ['fastenerFailed', 'fastener {what} failed'],
  ['fastenerSolids', 'fastener body has {n} solids'],
  ['fastenerInvalid', 'fastener body is not valid (BRepCheck)'],
  ['fastenerRoleUnique', 'fastener face role \'{role}\' is not unique'],
  ['fastenerRoleMissing', 'fastener face role \'{role}\' not found'],
  ['fastenerNaming', 'fastener naming: {detail:msg}'],
  ['hexChamfer', 'hex height {h} is too small for the 30° chamfer'],
  ['iso4014Length', 'ISO 4014 M{d} needs l > b = {b} mm; use ISO 4017 (fully threaded) for l = {l}'],
  ['pinLength', 'pin length {l} mm must exceed 2c = {c}'],
  ['occtFastener', 'OCCT failure while building fastener: {what}'],
  ['fastenerProfile', 'fastener profile polygon failed'],
  ['fastenerProfileFace', 'fastener profile face failed'],
  ['fastenerRevolution', 'fastener revolution failed'],
  ['hexFace', 'hexagon face failed'],
  ['hexPrism', 'hexagon prism failed'],
  ['emptyOccurrencePath', 'empty occurrence path'],
  ['zeroInsertion', 'zero insertion direction'],
  ['holeRadiusTolerance', 'hole radius and tolerance must be positive'],
  ['unknownOccurrence', 'unknown occurrence {id}'],
  ['holeNoAxis', '{where}hole face has no axis'],
  ['entryNotPlanar', '{where}entry face is not planar'],
  ['axisNotPerpendicular', '{where}hole axis is not perpendicular to the entry face'],
  ['exitFace', '{where}exit face is not a plane perpendicular to the hole axis facing the nut'],
  ['blindHole', '{where}blind hole: no exit face for the nut side'],
  ['screwShortGrip', '{where}screw is too short: grip {grip} mm, shank ends {d} mm inside the nut face'],
  ['definitionNoGeometry', 'definition \'{name}\' is not a part with geometry'],
  ['definitionNotFastener', 'definition \'{name}\' has no element map (not a fastener part)'],
  ['definitionNoFace', 'definition \'{name}\' has no face {face}'],
  ['bearingFace', 'definition \'{name}\': bearing face is not perpendicular to its axis'],
  ['concentricMateOf', 'concentric mate of \'{name}\': {detail:msg}'],
  ['bearingMateOf', 'bearing mate of \'{name}\': {detail:msg}'],
  ['fastenerKind', '{standard} is a {kind}, not a {expected}'],
  ['resolvedMany', '{ref} resolved to {n} elements'],
  ['moveDangling', 'move dimension: the dimension is dangling: {why}'],
  ['shelvesOverDimensions', 'sheet \'{sheet}\', view \'{view}\': {n:num} position shelf(s) overlap dimensions'],
  ['positionsOutOfOrder', 'sheet \'{sheet}\', view \'{view}\': positions out of order (otherwise leaders cross): {list}'],
  ['threadImageOcct', 'thread image: OCCT: {what}'],
  ['rollingBearingEmpty', 'bearing: empty shape'],
  ['rollingBearingNoCylinders', 'bearing: the part has no cylindrical faces'],
  ['rollingBearingNotRing', 'bearing: no inner and outer cylinders on one axis'],
  ['rollingBearingVertexOutside', 'bearing: a vertex lies outside the ring d…D'],
  ['rollingBearingZeroWidth', 'bearing: zero width'],
  ['rollingBearingOcct', 'bearing: {what}'],
  ['threadAxonometric', 'thread dimension: not drawn on an axonometric view (GOST 2.317)'],
  ['threadExternal', 'thread image: external thread on \'{name}\' (only internal threads are modelled)'],
  ['noPartForPositions', 'sheet \'{sheet}\', view \'{view}\': no assembly part is visible for positions'],
  ['sheetPositionPrefix', 'sheet \'{sheet}\', position \'{id}\': {detail:msg}'],
  ['sheetPartsListPrefix', 'sheet \'{sheet}\', parts list: {detail:msg}'],
  ['sheetRequirementsPrefix', 'sheet \'{sheet}\', technical requirements: {detail:msg}'],
  ['sheetViewNoResult', 'sheet \'{sheet}\', view \'{view}\': no result'],
  ['sheetViewPrefix', 'sheet \'{sheet}\', view \'{view}\': {detail:msg}'],
  ['sheetSectionPrefix', 'sheet \'{sheet}\', section \'{view}\': {detail:msg}'],
  ['sheetDetailPrefix', 'sheet \'{sheet}\', detail \'{view}\': {detail:msg}'],
  ['viewsExceedArea', 'sheet \'{sheet}\': views at {scale} exceed the view area'],
  ['viewOutOfCell', 'sheet \'{sheet}\': view \'{view}\' at {scale} does not fit its cell; set its position on the sheet'],
  ['viewsOverlap', 'sheet \'{sheet}\': views \'{view}\' and \'{other}\' overlap'],
  ['viewOverTitle', 'sheet \'{sheet}\': view \'{view}\' overlaps the title block'],
  ['viewBeyondFrame', 'sheet \'{sheet}\': view \'{view}\' goes beyond the drawing frame'],
  ['sheetDimensionPrefix', 'sheet \'{sheet}\', dimension \'{id}\': {detail:msg}'],
  ['sheetRoughnessPrefix', 'sheet \'{sheet}\', roughness \'{id}\': {detail:msg}'],
  ['sheetDatumPrefix', 'sheet \'{sheet}\', datum \'{id}\': {detail:msg}'],
  ['sheetThreadPrefix', 'sheet \'{sheet}\', thread \'{id}\': {detail:msg}'],
  ['sheetPrefix', 'sheet \'{sheet}\': {detail:msg}'],
  ['drawingParse', 'drawing: {what}'],
  ['specNewer', 'specification: file version {v} is newer'],
  ['specParse', 'specification: {what}'],
  ['exportKitSheet', 'export kit, sheet \'{sheet}\': {detail:msg}'],
  ['exportKitCreate', 'export kit: cannot create \'{path}\': {why}'],
  ['dxfBmp', 'DXF R2000 \\U+XXXX escapes cover only the BMP; code point U+{code} cannot be written'],
  ['pdfNeedsFont', 'PDF: drawing contains text, a TrueType font is required (PdfOptions::font)'],
  ['pdfNoGlyph', 'PDF: font \'{font}\' has no glyph for {code}'],
  ['cannotOpenForWriting', 'cannot open for writing: {path}'],
  ['writeFailed', 'write failed: {path}'],
  ['strokeNoGlyph', 'GOST 2.304 stroke font has no glyph for {code}'],
  ['a4TitleBlock', 'GOST 2.104: on A4 the title block is placed only along the short side (use Portrait)'],
  ['storageStrip', 'sheet too low for the storage strip (graphs 19-23, 145 mm)'],
  ['sheetNumber', 'title block: sheet number must be 1..sheets'],
  ['literaCells', 'title block: litera has at most 3 cells'],
  ['fontRestricted', 'font \'{font}\' forbids embedding (OS/2 fsType = Restricted License)'],
  ['noFontFound', 'no permissively licensed TrueType font found (DejaVu Sans or Liberation Sans); set BENDCORE_DRAWING_FONT to a .ttf file'],
  ['trueTypeParse', 'TrueType: {what}'],
  ['cannotOpenFont', 'cannot open font file: {path}'],
  ['cannotReadFont', 'cannot read font file: {path}'],
  ['centerLines', 'center lines: {what}'],
  ['auxiliaryUp', 'auxiliary view: up hint is parallel to the sight'],
  ['hlrMeshing', 'HLR (polygonal): meshing failed'],
  ['hlrOcct', 'HLR: OCCT failure: {what}'],
  ['hlrFailed', 'HLR: {what}'],
  ['hatchNoRegions', 'hatch: section has no regions'],
  ['cuttingMarkDegenerate', 'cutting plane mark: degenerate trace'],
  ['auxiliaryPerpendicular', 'auxiliary view: the view direction is perpendicular to the parent view, the arrow cannot be shown'],
  ['isoSheetTooSmall', 'sheet {name} is too small for the ISO 7200 title block'],
  ['thirdAngleEskd', 'third-angle projection (ISO 128-30, method A) is not provided by GOST 2.305; the ESKD title block has no projection symbol - use the ISO 7200 sheet style'],
  ['steppedMarkTwo', 'stepped plane mark: needs at least two planes'],
  ['steppedMarkDegenerate', 'stepped plane mark: degenerate trace'],
  ['localCoincident', 'local section: coincident boundary points'],
  ['localPolygon', 'local section: cannot build the boundary polygon'],
  ['localSpline', 'local section: spline interpolation failed'],
  ['localSplineEdge', 'local section: cannot build the spline edge'],
  ['localBoundary', 'local section: boundary: {detail:msg}'],
  ['localDegenerate', 'local section: degenerate boundary'],
  ['localZeroArea', 'local section: the boundary has zero area'],
  ['sectionOcct', 'section: OCCT failure: {what}'],
  ['sectionSight', 'section: the view must look along the sight (toward = -sight)'],
  ['sectionPlane', 'section: invalid cutting plane'],
  ['steppedPerpendicular', 'stepped section: the step axis must be perpendicular to the sight'],
  ['steppedSamePosition', 'stepped section: two plane points at the same position along the step axis'],
  ['steppedOcct', 'stepped section: OCCT failure: {what}'],
  ['frontExtentOcct', 'front extent: OCCT failure: {what}'],
  ['localNoRegions', 'local section: no regions'],
  ['localRegionFace', 'local section: cannot build the region face'],
  ['localPrism', 'local section: cannot build the cutting prism'],
  ['localOcct', 'local section: OCCT failure: {what}'],
  ['cuttingTraceBounds', 'cutting trace: invalid plane or empty bounds'],
  ['cuttingTracePerpendicular', 'cutting trace: the plane must be perpendicular to the view plane'],
  ['scaleExtents', 'scale: invalid extents'],
  ['layoutEmpty', 'layout: no views or empty area'],
  ['layoutNoGeometry', 'layout: view without geometry'],
  ['threadedHolePrefix', 'threaded hole{where}: {detail:msg}'],
  ['holeFaceSplit', 'hole face was split by hole #{n}'],
  ['sketchPrefix', 'sketch \'{name}\': {detail:msg}'],
  ['featureNoResult', 'feature produced neither a shape nor a sketch or datum'],
  ['toolNaming', 'tool naming failed: {what}'],
  ['variableNotEvaluated', 'variable \'{name}\' is not evaluated'],
  ['noSketchPoint', '{where}: no sketch point {n}'],
  ['noSketchEntity', '{where}: no sketch entity {n}'],
  ['bsplinePoles', '{where}: a B-spline needs at least 2 poles'],
  ['regionNoEntity', 'region selection: no sketch entity {n}'],
  ['sketchColonPrefix', 'sketch: {detail:msg}'],
  ['noParameter', '{type} has no parameter \'{name}\''],
  ['invalidOption', 'invalid option {name}=\'{value}\' for {type}'],
  ['noReferenceSlot', '{type} has no reference slot \'{name}\''],
  ['noLinkSlot', '{type} has no link slot \'{name}\''],
  ['noSketchData', '{type} does not take sketch data'],
  ['indexRange', 'index {n} is out of range'],
  ['bodyNoFaces', 'body \'{name}\' has no faces'],
  ['referenceKind', 'reference is a {a:kind}, slot expects a {b:kind}'],
  ['noShapeBefore', 'there is no shape before this feature'],
  ['upToOneLimit', 'up-to extent needs one \'limit\' face reference'],
  ['axisEntityNotLine', 'axisEntity {n} is not a line of sketch \'{name}\''],
  ['holePrefix', 'hole{where}: {detail:msg}'],
  ['boxPositive', 'box dimensions must be positive: {a} x {b} x {c}'],
  ['boxFace', 'box base face failed'],
  ['cylinderPositive', 'cylinder radius and height must be positive: r={r}, h={h}'],
  ['cylinderFace', 'cylinder base face failed'],
  ['cacheBase64', 'cache: invalid base64'],
  ['cacheNameOrigin', 'cache: unknown name origin'],
  ['cacheNoBodyNames', 'cache: no body names'],
  ['cacheOf', 'cache of {id}: {detail:msg}'],
  ['toolCacheOf', 'tool cache of {id}: {detail:msg}'],
  ['saveFailed', 'save failed: {what}'],
  ['variableParse', 'variable: {what}'],
  ['featureParse', 'feature: {what}'],
  ['unknownPlane', 'unknown plane choice \'{name}\''],
  ['linkDatumAxis', 'link \'{link}\' must name a datum axis'],
  ['unknownAxis', 'unknown axis choice \'{name}\''],
  ['sketchValuesMismatch', 'sketch values do not match the sketch data'],
  ['projectedNotResolved', '{where}: projected edge #{n} is not among the {k} resolved \'projected\' references'],
  ['edgeProjectsToPoint', '{where}: the edge projects to a point'],
  ['projectedNotKind', '{where}: projected edge is not a {kind}'],
  ['minorRadius', '{where}: minor radius must be positive, is {r}'],
  ['sketchCannotSolve', 'sketch cannot be solved: {detail:msg}'],
  ['regionFaceLost', 'sketch region face lost while building the profile'],
  ['profileEdgeNoEntity', 'profile edge without a sketch entity'],
  ['noPlane', 'no plane'],
  ['midwayPlane', 'midway plane needs references \'face\' and \'face2\''],
  ['noAxis', 'no axis'],
  ['axisTwoPlanes', 'axis of two planes needs links \'plane\' and \'plane2\''],
  ['referenceOneKind', 'reference \'{slot}\' must give one {what:kind}, has {n}'],
  ['linkDatumPoint', 'link \'{link}\' must name a datum point'],
  ['linkDatumCsys', 'link \'{link}\' must name a coordinate system'],
  ['noPoint', 'no point'],
  ['noOrigin', 'no origin'],
  ['noCsys', 'no coordinate system'],
  ['edgePlaneFailed', 'edge-plane intersection failed'],
  ['transactionOpen', 'transaction \'{name}\' is already open'],
  ['transactionName', 'a transaction needs a name'],
  ['bodyNumberPrefix', 'body #{n}: {detail:msg}'],
  ['mustBeOneSolid', '{what} must be one solid'],
  ['notABody', '{what} is not a body of the part'],
  ['toolNumberPrefix', 'tool #{n}: {detail:msg}'],
  ['edgeNoFaces', 'edge #{n} has no faces'],
  ['refFaceNotAdjacent', 'reference face is not adjacent to edge #{n}'],
  ['refFaceAuto', 'edge #{n}: reference face chosen automatically'],
  ['chamferRejected', 'chamfer result rejected: {why}'],
  ['removedFaceNotOfBody', 'removed face #{n} is not a face of the body'],
  ['shellRejected', 'shell result rejected: {why}'],
  ['shellInward', 'inward shell did not reduce the volume'],
  ['draftFailed', 'BRepOffsetAPI_DraftAngle failed'],
  ['draftRejected', 'draft result rejected: {why}'],
  ['itemNotEdge', 'item #{n} is not an edge'],
  ['prismFailed', 'BRepPrimAPI_MakePrism failed'],
  ['draftSideFace', 'draft cannot be applied to a side face of the extrusion'],
  ['thinFailed', 'thin wall: BRepOffsetAPI_MakeThickSolid failed'],
  ['sweepLimitFace', 'cannot sweep the limit face'],
  ['upToTrimPrefix', 'up-to trim: {detail:msg}'],
  ['notBoundedByLimit', 'profile is not fully bounded by the limit face'],
  ['upToSolids', 'up-to trim produced {n} solids'],
  ['extendPlane', 'cannot extend the limit plane'],
  ['extendSurface', 'cannot extend the limit surface'],
  ['twoSidedNegative', 'two-sided distances must be >= 0'],
  ['draftOneSided', 'draft is supported only for one-sided extents'],
  ['thinWithDraft', 'thin wall together with draft is not supported'],
  ['profileNotParallel', 'profile face is not parallel to the sketch plane'],
  ['profileNotInPlane', 'profile face does not lie in the sketch plane'],
  ['profileInvalid', 'profile face is invalid (BRepCheck_Analyzer)'],
  ['transformFailed', 'BRepBuilderAPI_Transform failed'],
  ['toolBodyPrefix', 'tool body: {detail:msg}'],
  ['holeProfile', 'hole profile polygon failed'],
  ['holeProfileFace', 'hole profile face failed'],
  ['holeRevolve', 'hole tool revolve failed'],
  ['drillAngle', 'drill point angle must be in (0, pi)'],
  ['countersinkAngle', 'countersink angle must be in (0, pi)'],
  ['totalAngle2pi', 'total pattern angle must not exceed 2*pi'],
  ['revolveTwoSided', 'two-sided revolve angles must be >= 0'],
  ['revolveTotal', 'total revolve angle must be in (0, 2*pi]'],
  ['revolFailed', 'BRepPrimAPI_MakeRevol failed'],
  ['ribRegionWire', 'rib region wire failed'],
  ['ribSelfIntersecting', 'rib region is self-intersecting: the contour must not fold back over its chord'],
  ['ribSlab', 'rib slab prism failed'],
  ['ribTrimPrefix', 'rib trim: {detail:msg}'],
  ['ribWallFace', 'rib wall face failed'],
  ['ribArcRadius', 'rib thickness exceeds the arc radius'],
  ['ribWallWire', 'rib wall wire failed'],
  ['ribLinesArcs', 'rib perpendicular to the sketch supports lines and arcs only'],
  ['ribWallPrism', 'rib wall prism failed'],
  ['ribJoint', 'rib joint face failed'],
  ['ribUnionPrefix', 'rib wall union: {detail:msg}'],
  ['ribNotReachSide', 'rib does not reach the body on the chosen side'],
  ['ribNotReachEither', 'rib does not reach the body on either side'],
  ['ribEmpty', 'rib contour is empty'],
  ['ribNotConnected', 'rib contour is not a connected wire'],
  ['ribEdgeNoVertices', 'rib contour edge without vertices'],
  ['thinOpenPath', 'thin wall needs an open path (end faces to remove)'],
  ['pathEdgeNoVertices', 'path edge without end vertices'],
  ['guideCurve', 'guide orientation needs a guide curve'],
  ['sweepNotConnected', 'sweep path is not a connected wire'],
  ['pipeShellFailed', 'BRepOffsetAPI_MakePipeShell failed for profile contour #{n}'],
  ['sweepNoVolume', 'sweep of contour #{n} has no volume'],
  ['sectionNumberPrefix', 'section #{n}: {detail:msg}'],
  ['thruSectionsFailed', 'BRepOffsetAPI_ThruSections failed for contour #{n}'],
  ['loftNotSolid', 'loft of contour #{n} is not a solid'],
  ['loftInvalid', 'loft of contour #{n} is invalid'],
  ['loftNoVolume', 'loft of contour #{n} has no volume'],
  ['threadAttrMissing', 'thread attribute \'{name}\' is missing'],
  ['threadHand', 'thread.hand must be right or left'],
  ['threadKind', 'thread.kind must be internal or external'],
  ['notNumber', '{name} is not a number: \'{value}\''],
  ['threadEntry', 'thread.entry must be three numbers'],
  ['threadDirection', 'thread.direction must be a non-zero vector'],
  ['threadDiameter', 'thread nominal diameter must be > 0'],
  ['threadPitch', 'thread pitch must be in (0, D/2)'],
  ['tapDrill', 'tap drill diameter must be in (0, D)'],
  ['drillDepthFinite', 'drill depth must be finite'],
  ['threadLengthNonNegative', 'thread length must be >= 0 (0: through the whole hole)'],
  ['threadWall', 'threaded hole: the hole wall was not found'],
  ['elementMapRecords', 'element map has {n} {kind} records, the shape has {m}'],
  ['elementMapBodies', 'element map has {n} body names, the shape has {m} solids'],
  ['namingFailed', 'naming failed: {what}'],
  ['notPartOfShape', 'the {kind:kind} is not part of the shape'],
  ['renamedMore', '{kind:kind} \'{name}\' now named \'{to}\' and {n} more'],
  ['renamed', '{kind:kind} \'{name}\' now named \'{to}\''],
  ['splitInto', '{kind:kind} \'{name}\' was split into {n} parts'],
  ['replacedBy', '{kind:kind} \'{name}\' was replaced by feature \'{feature}\' with \'{to}\''],
  ['signatureFailed', 'signature computation failed: {what}'],
  ['entityRemoved', 'sketch entity {n} is removed'],
  ['curveConversion', 'curve conversion failed: {what}'],
  ['ellipseDirection', 'ellipse major direction is a zero vector'],
  ['ellipseRadius', 'ellipse radius is not greater than the tolerance'],
  ['sketchFaceNotPlanar', 'sketch face is not planar'],
  ['faceIndexRange', 'face index {i} out of range ({n} faces)'],
  ['edgeForEntity', 'cannot make an edge for sketch entity {n} (BRepBuilderAPI_EdgeError {code})'],
  ['faceFromLoop', 'cannot make a face from the loop starting at entity {n}'],
  ['regionFace', 'cannot make the face of region {n}'],
  ['regionFaceInvalid', 'face of region {n} is invalid'],
  ['regionFaceArea', 'face of region {n} is inconsistent with its boundary (area {a} vs {b})'],
  ['profileConstruction', 'profile construction failed: {what}'],
  ['edgeForEntityShort', 'cannot make an edge for sketch entity {n}'],
  ['pathWire', 'cannot join the sketch path into a wire'],
  ['sketchPathPrefix', 'sketch path: {detail:msg}'],
  ['projectionFailed', 'projection onto the sketch plane failed'],
  ['projectedNotPlanar', 'projected curve is not planar'],
  ['edgeProjectionFailed', 'edge projection failed: {what}'],
  ['edgeIndexRange', 'edge index {i} out of range ({n} edges)'],
  ['edgeNumberPrefix', 'edge {n}: {detail:msg}'],
  ['vertexIndexRange', 'vertex index {i} out of range'],
  ['edgeDegenerated', 'edge is degenerated (collapsed to a point)'],
  ['edgeNo3d', 'edge has no 3D curve'],
  ['pointAlongEdge', 'cannot locate the point along the edge'],
];

type FieldType = 'text' | 'id' | 'num' | 'op' | 'kind' | 'msg';

interface Compiled {
  id: string;
  pattern: string;
  names: string[];
  types: FieldType[];
  strict: RegExp;
}

const escapeRegExp = (s: string) => s.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');

const FIELD = /\{(\w+)(?::(\w+))?\}/g;

const GROUP: Record<FieldType, string> = {
  text: '([\\s\\S]*?)',
  id: "([^\\s:'\"]+)",
  num: '(-?\\d+(?:\\.\\d+)?(?:[eE][-+]?\\d+)?)',
  op: '([a-z ]+?)',
  kind: '(face|edge|vertex)',
  msg: '([\\s\\S]+)',
};

/** Regular expression of a pattern; `loose` turns every field into "any text". */
export function patternRegExp(pattern: string, loose = false): { re: RegExp; names: string[]; types: FieldType[] } {
  const names: string[] = [];
  const types: FieldType[] = [];
  let src = '';
  let pos = 0;
  for (const m of pattern.matchAll(FIELD)) {
    src += escapeRegExp(pattern.slice(pos, m.index));
    const type = (m[2] ?? 'text') as FieldType;
    if (!(type in GROUP)) throw new Error(`template '${pattern}': unknown field type '${type}'`);
    names.push(m[1]);
    types.push(type);
    src += loose ? '([\\s\\S]*?)' : GROUP[type];
    pos = (m.index ?? 0) + m[0].length;
  }
  src += escapeRegExp(pattern.slice(pos));
  return { re: new RegExp(`^${src}$`), names, types };
}

let compiled: Compiled[] | null = null;
function templates(): Compiled[] {
  if (!compiled) {
    compiled = KERNEL_TEMPLATES.map(([id, pattern]) => {
      const { re, names, types } = patternRegExp(pattern);
      return { id, pattern, names, types, strict: re };
    });
  }
  return compiled;
}

/** A number written by std::to_string ("30.000000") without trailing zeros. */
function value(v: string): string {
  if (/^-?\d+\.\d+$/.test(v)) return String(Number(v));
  return v;
}

/**
 * A nested message continues the sentence of its prefix ("Лист «S1», вид «V1»: разрез: …"): its
 * first letter is lowered unless it starts an abbreviation (OCCT, ГОСТ, PDF).
 */
function continuation(text: string): string {
  return /^\p{Lu}\p{Ll}/u.test(text) ? text[0].toLowerCase() + text.slice(1) : text;
}

export interface TemplateMatch {
  id: string;
  key: MessageKey;
  params: Record<string, string>;
}

/** The template a kernel text matches, with the values filled in (localized by `t`), or null. */
export function matchKernelText(text: string, t: Translate, depth = 0): TemplateMatch | null {
  if (depth > 4) return null;
  const trimmed = text.trim();
  for (const c of templates()) {
    const m = c.strict.exec(trimmed);
    if (!m) continue;
    const params: Record<string, string> = {};
    let ok = true;
    c.names.forEach((name, i) => {
      if (!ok) return;
      const raw = m[i + 1];
      const type = c.types[i];
      if (type === 'op' || type === 'kind') {
        const key = `kernel.${type}.${raw}`;
        if (!isMessageKey(key)) ok = false;
        else params[name] = t(key);
      } else if (type === 'msg') {
        const inner = matchKernelText(raw, t, depth + 1);
        if (!inner) ok = false;
        else params[name] = continuation(t(inner.key, inner.params));
      } else {
        params[name] = value(raw);
      }
    });
    const key = `kernel.msg.${c.id}`;
    if (ok && isMessageKey(key)) return { id: c.id, key, params };
  }
  return null;
}

export interface LocalizedKernelMessage {
  /** error code, "message" for a feature node message */
  code: string;
  /** localized title of the code */
  title: string;
  /** localized text when a template matched, else null */
  text: string | null;
  /** the kernel's original text */
  original: string;
  /** id of the matched template */
  template: string | null;
}

/** Localized form of a kernel text with its code. */
export function localizeKernelMessage(t: Translate, code: string | null, original: string): LocalizedKernelMessage {
  const c = code ?? 'message';
  const titleKey = `kernel.code.${c}`;
  const title = t(isMessageKey(titleKey) ? titleKey : 'kernel.code.other');
  const m = matchKernelText(original, t);
  return { code: c, title, text: m ? t(m.key, m.params) : null, original, template: m ? m.id : null };
}

/** Localized form of a thrown value: a KernelError { code, detail }, an Error or anything else. */
export function localizeKernelError(t: Translate, e: unknown): LocalizedKernelMessage {
  if (e && typeof e === 'object' && 'code' in e && ('detail' in e || 'message' in e)) {
    const ke = e as { code: unknown; detail?: unknown; message?: unknown };
    return localizeKernelMessage(t, String(ke.code), String(ke.detail ?? ke.message ?? ''));
  }
  return localizeKernelMessage(t, 'other', e instanceof Error ? e.message : String(e));
}

/** One line for a status bar: the localized text, or the title with the original. */
export function kernelMessageLine(m: LocalizedKernelMessage): string {
  return m.text ?? `${m.title}: ${m.original}`;
}
