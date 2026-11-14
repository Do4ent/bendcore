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
        else params[name] = t(inner.key, inner.params);
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
