/**
 * Side panels of the assembly page (docs/ui/ASSEMBLY_UI.md): mate, fasteners, interference,
 * BOM, exploded view, pattern and component properties. Panels hold only form state; every
 * action is an AssemblyDocumentHandle call made by AssemblyApp through the callbacks.
 */

import React, { useEffect, useMemo, useState } from 'react';
import type { Remote } from '../worker/wasmClient';
import type {
  Alignment,
  AssemblyKernel,
  AssemblyState,
  Bom,
  DefinitionInfo,
  FastenerDescription,
  FastenerSpec,
  FastenerStandardInfo,
  InterferenceReport,
  MateSpec,
  MateType,
  OccurrenceInfo,
  PartProperties,
} from '../../wasm/bendcore_assembly_api.mjs';
import { bomTableRows } from './assemblyMath';
import type { V3 } from './assemblyMath';
import { massText } from './labels';
import { caption, useI18n } from '../app/i18n';
import { MATE_TYPES, allowedMateTypes, mateValueUnit, suggestMateType } from './mateRules';
import type { PickedElement } from './mateRules';

const num = (s: string, fallback = 0) => {
  const v = Number(s.replace(',', '.'));
  return Number.isFinite(v) ? v : fallback;
};

function PickSlot({ title, pick, onClear, testId }: { title: string; pick: PickedElement | null; onClear: () => void; testId: string }) {
  const { t } = useI18n();
  return (
    <div className={`as-slot ${pick ? 'as-slot-filled' : ''}`} data-testid={testId}>
      <span className="as-slot-title">{title}</span>
      {pick ? (
        <>
          <span className="as-slot-label">
            {pick.label} — {caption(t, 'assembly.geometry.', pick.geometry)}
            {pick.entity?.radius !== undefined ? ` R${+pick.entity.radius.toFixed(4)}` : ''}
          </span>
          <button type="button" onClick={onClear} title={t('assembly.slot.clear')}>
            ✕
          </button>
        </>
      ) : (
        <span className="as-muted">{t('assembly.slot.pick')}</span>
      )}
    </div>
  );
}

// ------------------------------------------------------------------ mate

export interface MatePanelProps {
  slots: [PickedElement | null, PickedElement | null];
  onClearSlot: (slot: 0 | 1) => void;
  onAdd: (spec: MateSpec) => void;
}

export const MatePanel: React.FC<MatePanelProps> = ({ slots, onClearSlot, onAdd }) => {
  const { t } = useI18n();
  const [a, b] = slots;
  const allowed = useMemo(() => allowedMateTypes(a, b), [a, b]);
  const [type, setType] = useState<MateType | null>(null);
  const [value, setValue] = useState('0');
  const [alignment, setAlignment] = useState<Alignment>('closest');
  const [internal, setInternal] = useState(false);
  useEffect(() => {
    setType((t) => (t && allowed.includes(t) ? t : suggestMateType(a, b)));
  }, [a, b, allowed]);
  const unit = type ? mateValueUnit(type) : null;
  const add = () => {
    if (!a || !b || !type) return;
    onAdd({
      type,
      a: a.ref,
      b: b.ref,
      value: unit ? num(value) : 0,
      alignment,
      internal: type === 'tangent' ? internal : false,
    });
  };
  return (
    <div className="as-panel" data-testid="mate-panel">
      <h2>{t('assembly.mate.title')}</h2>
      <PickSlot title="A" pick={a} onClear={() => onClearSlot(0)} testId="mate-slot-a" />
      <PickSlot title="B" pick={b} onClear={() => onClearSlot(1)} testId="mate-slot-b" />
      {a && b && allowed.length === 0 && <p className="as-warn">{t('assembly.mate.sameComponent')}</p>}
      <div className="as-types" role="radiogroup">
        {MATE_TYPES.map((m) => (
          <label key={m} className={allowed.includes(m) ? '' : 'as-disabled'}>
            <input
              type="radio"
              name="mate-type"
              value={m}
              disabled={!allowed.includes(m)}
              checked={type === m}
              onChange={() => setType(m)}
              data-testid={`mate-type-${m}`}
            />
            {caption(t, 'assembly.mateType.', m)}
          </label>
        ))}
      </div>
      {unit && (
        <label className="as-field">
          {unit === 'mm' ? t('assembly.mate.distance') : t('assembly.mate.angle')}
          <input data-testid="mate-value" value={value} onChange={(e) => setValue(e.target.value)} />
        </label>
      )}
      {type && type !== 'rigid' && type !== 'perpendicular' && type !== 'angle' && (
        <label className="as-field">
          {type === 'tangent' ? t('assembly.mate.side') : t('assembly.mate.alignment')}
          <select value={alignment} onChange={(e) => setAlignment(e.target.value as Alignment)} data-testid="mate-alignment">
            <option value="closest">{t('assembly.mate.closest')}</option>
            <option value="aligned">{t('assembly.mate.aligned')}</option>
            <option value="anti-aligned">{t('assembly.mate.antiAligned')}</option>
          </select>
        </label>
      )}
      {type === 'tangent' && (
        <label className="as-field">
          <input type="checkbox" checked={internal} onChange={(e) => setInternal(e.target.checked)} />
          {t('assembly.mate.internal')}
        </label>
      )}
      <button type="button" className="as-primary" disabled={!a || !b || !type} onClick={add} data-testid="mate-add">
        {t('assembly.mate.add')}
      </button>
    </div>
  );
};

// ------------------------------------------------------------------ fasteners

export interface FastenerPanelProps {
  /** Assembly module in the WASM worker. */
  kernel: Remote<AssemblyKernel>;
  hole: PickedElement | null;
  onClearHole: () => void;
  onInsert: (spec: {
    screw: FastenerSpec;
    headWashers: FastenerSpec[];
    nutWashers: FastenerSpec[];
    nut: FastenerSpec | null;
    allHoles: boolean;
  }) => void;
}

export const FastenerPanel: React.FC<FastenerPanelProps> = ({ kernel, hole, onClearHole, onInsert }) => {
  const { t } = useI18n();
  const [catalog, setCatalog] = useState<FastenerStandardInfo[]>([]);
  useEffect(() => {
    let live = true;
    kernel.fastenerCatalog().then((c) => live && setCatalog(c), () => undefined);
    return () => {
      live = false;
    };
  }, [kernel]);
  const [system, setSystem] = useState<'gost' | 'iso'>('gost');
  const inSystem = catalog.filter((c) => c.gost === (system === 'gost'));
  const screws = inSystem.filter((c) => c.kind === 'hex-bolt' || c.kind === 'socket-head-screw');
  const [screwId, setScrewId] = useState('gost-7798');
  const screw = catalog.find((c) => c.id === screwId) ?? screws[0];
  const [size, setSize] = useState(10);
  const [length, setLength] = useState('40');
  const [headWasher, setHeadWasher] = useState('gost-11371');
  const [nutWasher, setNutWasher] = useState('gost-11371');
  const [nut, setNut] = useState('gost-5915');
  const [allHoles, setAllHoles] = useState(true);
  useEffect(() => {
    // Switching the standard system keeps the same construction through the counterpart.
    const swap = (id: string) => {
      const c = catalog.find((x) => x.id === id);
      if (!c || c.gost === (system === 'gost')) return id;
      return c.counterpart ?? '';
    };
    setScrewId((id) => {
      const s = swap(id);
      return s && screws.some((x) => x.id === s) ? s : screws[0]?.id ?? '';
    });
    setHeadWasher((id) => (id ? swap(id) : id));
    setNutWasher((id) => (id ? swap(id) : id));
    setNut((id) => (id ? swap(id) : id));
    // screws is derived from system.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [system, catalog]);
  const sizes = screw?.sizes ?? [];
  const withSize = (kinds: string[]) => inSystem.filter((c) => kinds.includes(c.kind) && c.sizes.includes(size));
  const washers = withSize(['plain-washer', 'spring-washer']);
  const nuts = withSize(['hex-nut']);
  const screwSpec: FastenerSpec | null = screw ? { standard: screw.id, size, length: num(length) } : null;
  const [described, setDescribed] = useState<{ key: string; description: FastenerDescription | null; error: string }>({
    key: '',
    description: null,
    error: '',
  });
  const specKey = screwSpec && sizes.includes(size) ? JSON.stringify(screwSpec) : '';
  useEffect(() => {
    if (!specKey) return undefined;
    let live = true;
    kernel.describeFastener(JSON.parse(specKey) as FastenerSpec).then(
      (d) => live && setDescribed({ key: specKey, description: d, error: '' }),
      (e: unknown) => live && setDescribed({ key: specKey, description: null, error: (e as Error).message }),
    );
    return () => {
      live = false;
    };
  }, [kernel, specKey]);
  const description = specKey && described.key === specKey ? described.description : null;
  const descriptionError = specKey && described.key === specKey ? described.error : '';
  const valid = (id: string, list: FastenerStandardInfo[]) => (id && list.some((c) => c.id === id) ? id : '');
  const hw = valid(headWasher, washers);
  const nw = valid(nutWasher, washers);
  const nt = valid(nut, nuts);
  const isHole = hole?.entity?.type === 'cylinder';
  const insert = () => {
    if (!screwSpec) return;
    onInsert({
      screw: screwSpec,
      headWashers: hw ? [{ standard: hw, size }] : [],
      nutWashers: nw ? [{ standard: nw, size }] : [],
      nut: nt ? { standard: nt, size } : null,
      allHoles,
    });
  };
  const select = (labelText: string, value: string, set: (v: string) => void, list: FastenerStandardInfo[], testId: string, none = true) => (
    <label className="as-field">
      {labelText}
      <select value={value} onChange={(e) => set(e.target.value)} data-testid={testId}>
        {none && <option value="">{t('assembly.fasteners.none')}</option>}
        {list.map((c) => (
          <option key={c.id} value={c.id}>
            {caption(t, 'assembly.fastenerKind.', c.kind)} {c.designation}
          </option>
        ))}
      </select>
    </label>
  );
  return (
    <div className="as-panel" data-testid="fastener-panel">
      <h2>{t('assembly.fasteners.title')}</h2>
      <div className="as-types">
        <label>
          <input type="radio" checked={system === 'gost'} onChange={() => setSystem('gost')} data-testid="fastener-gost" />
          {t('assembly.fasteners.gost')}
        </label>
        <label>
          <input type="radio" checked={system === 'iso'} onChange={() => setSystem('iso')} data-testid="fastener-iso" />
          ISO
        </label>
      </div>
      {select(t('assembly.fasteners.screw'), screw?.id ?? '', setScrewId, screws, 'fastener-screw', false)}
      <label className="as-field">
        {t('assembly.fasteners.size')}
        <select value={size} onChange={(e) => setSize(Number(e.target.value))} data-testid="fastener-size">
          {sizes.map((d) => (
            <option key={d} value={d}>
              M{d}
            </option>
          ))}
        </select>
      </label>
      <label className="as-field">
        {t('assembly.fasteners.length')}
        <input value={length} onChange={(e) => setLength(e.target.value)} data-testid="fastener-length" />
      </label>
      {select(t('assembly.fasteners.headWasher'), hw, setHeadWasher, washers, 'fastener-head-washer')}
      {select(t('assembly.fasteners.nutWasher'), nw, setNutWasher, washers, 'fastener-nut-washer')}
      {select(t('assembly.fasteners.nut'), nt, setNut, nuts, 'fastener-nut')}
      {description && (
        <p className="as-muted" data-testid="fastener-description">
          {t('assembly.fasteners.mass', { designation: description.designation, mass: massText(description.mass) })}
          {screw && !screw.verified ? t('assembly.fasteners.unverified') : ''}
        </p>
      )}
      {descriptionError && <p className="as-warn">{descriptionError}</p>}
      <PickSlot title={t('assembly.fasteners.hole')} pick={hole} onClear={onClearHole} testId="fastener-hole" />
      {hole && !isHole && <p className="as-warn">{t('assembly.fasteners.pickHole')}</p>}
      <label className="as-field">
        <input type="checkbox" checked={allHoles} onChange={(e) => setAllHoles(e.target.checked)} data-testid="fastener-all" />
        {t('assembly.fasteners.allHoles')}
      </label>
      <p className="as-hint">{t('assembly.fasteners.hint')}</p>
      <button type="button" className="as-primary" disabled={!isHole || !screwSpec} onClick={insert} data-testid="fastener-insert">
        {t('assembly.fasteners.insert')}
      </button>
    </div>
  );
};

// ------------------------------------------------------------------ interference

export interface InterferencePanelProps {
  report: InterferenceReport | null;
  showTouching: boolean;
  selectedPair: number | null;
  onCheck: () => void;
  onShowTouching: (show: boolean) => void;
  onSelectPair: (index: number | null) => void;
}

export const InterferencePanel: React.FC<InterferencePanelProps> = ({
  report,
  showTouching,
  selectedPair,
  onCheck,
  onShowTouching,
  onSelectPair,
}) => {
  const { t } = useI18n();
  const pairs = report ? report.pairs.map((p, i) => ({ ...p, i })).filter((p) => showTouching || p.kind === 'interference') : [];
  const interfering = report ? report.pairs.filter((p) => p.kind === 'interference').length : 0;
  return (
    <div className="as-panel" data-testid="interference-panel">
      <h2>{t('assembly.interference.title')}</h2>
      <button type="button" className="as-primary" onClick={onCheck} data-testid="interference-check">
        {t('assembly.interference.check')}
      </button>
      <label className="as-field">
        <input type="checkbox" checked={showTouching} onChange={(e) => onShowTouching(e.target.checked)} />
        {t('assembly.interference.touching')}
      </label>
      {report && (
        <p data-testid="interference-summary">
          {t('assembly.interference.summary', {
            interfering,
            touching: report.pairs.length - interfering,
            pairs: report.totalPairs,
            candidates: report.candidatePairs,
            volume: report.totalVolume.toFixed(3),
          })}
        </p>
      )}
      <ul className="as-list">
        {pairs.map((p) => (
          <li
            key={p.i}
            className={`as-row ${p.kind === 'interference' ? 'as-interference' : 'as-touching'} ${selectedPair === p.i ? 'as-selected' : ''}`}
            onClick={() => onSelectPair(selectedPair === p.i ? null : p.i)}
            data-testid={`interference-pair-${p.i}`}
          >
            <span className="as-name">
              {report!.parts[p.a].pathName} ↔ {report!.parts[p.b].pathName}
            </span>
            <span className="as-status">{p.kind === 'interference' ? t('assembly.interference.volume', { volume: p.volume.toFixed(3) }) : t('assembly.interference.touch')}</span>
          </li>
        ))}
      </ul>
    </div>
  );
};

// ------------------------------------------------------------------ BOM

export const BomPanel: React.FC<{ bom: Bom | null; mode: 'flat' | 'structured'; onMode: (m: 'flat' | 'structured') => void }> = ({
  bom,
  mode,
  onMode,
}) => {
  const { t } = useI18n();
  return (
  <div className="as-panel as-wide" data-testid="bom-panel">
    <h2>{t('assembly.bom.title')}</h2>
    <div className="as-types">
      <label>
        <input type="radio" checked={mode === 'flat'} onChange={() => onMode('flat')} data-testid="bom-flat" />
        {t('assembly.bom.flat')}
      </label>
      <label>
        <input type="radio" checked={mode === 'structured'} onChange={() => onMode('structured')} data-testid="bom-structured" />
        {t('assembly.bom.structured')}
      </label>
    </div>
    {bom && (
      <table className="as-bom" data-testid="bom-table">
        <thead>
          <tr>
            <th>{t('assembly.bom.position')}</th>
            <th>{t('assembly.bom.designation')}</th>
            <th>{t('assembly.bom.name')}</th>
            <th>{t('assembly.bom.material')}</th>
            <th>{t('assembly.bom.quantity')}</th>
            <th>{t('assembly.bom.unitMass')}</th>
            <th>{t('assembly.bom.mass')}</th>
          </tr>
        </thead>
        <tbody>
          {bomTableRows(bom.rows).map((r) => (
            <tr key={`${r.positionPath}-${r.definition}`} className={r.kind === 'assembly' ? 'as-bom-assembly' : ''}>
              <td>{r.positionPath}</td>
              <td>{r.designation}</td>
              <td style={{ paddingLeft: 4 + r.level * 12 }} data-testid="bom-name">
                {r.name}
              </td>
              <td>{r.material}</td>
              <td data-testid="bom-quantity">{r.quantity}</td>
              <td>{massText(r.unitMass)}</td>
              <td>{massText(r.totalMass)}</td>
            </tr>
          ))}
        </tbody>
        <tfoot>
          <tr>
            <td colSpan={4}>{t('assembly.bom.total')}</td>
            <td data-testid="bom-total-parts">{bom.totalParts}</td>
            <td />
            <td data-testid="bom-total-mass">{massText(bom.totalMass)}</td>
          </tr>
        </tfoot>
      </table>
    )}
    <p className="as-hint">{t('assembly.bom.hint')}</p>
  </div>
  );
};

// ------------------------------------------------------------------ exploded view

export interface ExplodePanelProps {
  state: AssemblyState;
  view: number | null;
  progress: number;
  selectedComponent: number | null;
  onSelectView: (index: number | null) => void;
  onProgress: (p: number) => void;
  onAddView: (view: { name: string; steps: { occurrences: number[]; direction: V3; distance: number }[] }) => void;
  onRemoveView: (index: number) => void;
}

const DIRECTIONS: Record<string, V3> = {
  '+X': [1, 0, 0],
  '−X': [-1, 0, 0],
  '+Y': [0, 1, 0],
  '−Y': [0, -1, 0],
  '+Z': [0, 0, 1],
  '−Z': [0, 0, -1],
};

export const ExplodePanel: React.FC<ExplodePanelProps> = ({
  state,
  view,
  progress,
  selectedComponent,
  onSelectView,
  onProgress,
  onAddView,
  onRemoveView,
}) => {
  const { t } = useI18n();
  const [steps, setSteps] = useState<{ occurrences: number[]; direction: V3; distance: number }[]>([]);
  const [direction, setDirection] = useState('+Z');
  const [distance, setDistance] = useState('50');
  const [name, setName] = useState(() => t('assembly.explode.defaultName'));
  const current = view !== null ? state.explodedViews[view] : null;
  const nameOf = (id: number) => state.occurrences.find((o) => o.id === id)?.name ?? String(id);
  return (
    <div className="as-panel" data-testid="explode-panel">
      <h2>{t('assembly.explode.title')}</h2>
      <ul className="as-list">
        {state.explodedViews.map((v, i) => (
          <li key={i} className={`as-row ${view === i ? 'as-selected' : ''}`} onClick={() => onSelectView(view === i ? null : i)}>
            <span className="as-name">
              {t('assembly.explode.steps', { name: v.name, count: v.steps.length })}
            </span>
            <span className="as-actions" onClick={(e) => e.stopPropagation()}>
              <button type="button" onClick={() => onRemoveView(i)} title={t('assembly.explode.remove')}>
                ✕
              </button>
            </span>
          </li>
        ))}
      </ul>
      {current && (
        <label className="as-field">
          {t('assembly.explode.progress', { progress: progress.toFixed(2), count: current.steps.length })}
          <input
            type="range"
            min={0}
            max={current.steps.length}
            step={0.05}
            value={progress}
            onChange={(e) => onProgress(Number(e.target.value))}
            data-testid="explode-slider"
          />
        </label>
      )}
      <h2>{t('assembly.explode.newView')}</h2>
      <label className="as-field">
        {t('assembly.explode.name')}
        <input value={name} onChange={(e) => setName(e.target.value)} />
      </label>
      <label className="as-field">
        {t('assembly.explode.direction')}
        <select value={direction} onChange={(e) => setDirection(e.target.value)} data-testid="explode-direction">
          {Object.keys(DIRECTIONS).map((d) => (
            <option key={d}>{d}</option>
          ))}
        </select>
      </label>
      <label className="as-field">
        {t('assembly.explode.distance')}
        <input value={distance} onChange={(e) => setDistance(e.target.value)} data-testid="explode-distance" />
      </label>
      <button
        type="button"
        disabled={selectedComponent === null}
        onClick={() =>
          selectedComponent !== null &&
          setSteps([...steps, { occurrences: [selectedComponent], direction: DIRECTIONS[direction], distance: num(distance) }])
        }
        data-testid="explode-add-step"
      >
        {t('assembly.explode.addStep')}
      </button>
      <ol className="as-list">
        {steps.map((s, i) => (
          <li key={i}>
            {t('assembly.explode.step', { names: s.occurrences.map(nameOf).join(', '), distance: s.distance, direction: s.direction.join(', ') })}
          </li>
        ))}
      </ol>
      <button
        type="button"
        className="as-primary"
        disabled={steps.length === 0}
        onClick={() => {
          onAddView({ name, steps });
          setSteps([]);
        }}
        data-testid="explode-create"
      >
        {t('assembly.explode.create')}
      </button>
    </div>
  );
};

// ------------------------------------------------------------------ pattern

export interface PatternPanelProps {
  seed: OccurrenceInfo | null;
  hole: PickedElement | null;
  onAdd: (spec: Record<string, unknown> & { kind: 'linear' | 'circular' | 'feature'; seed: number }) => void;
}

export const PatternPanel: React.FC<PatternPanelProps> = ({ seed, hole, onAdd }) => {
  const { t } = useI18n();
  const [kind, setKind] = useState<'linear' | 'circular' | 'feature'>('linear');
  const [dir, setDir] = useState('1, 0, 0');
  const [spacing, setSpacing] = useState('50');
  const [count, setCount] = useState('3');
  const [axisPoint, setAxisPoint] = useState('0, 0, 0');
  const [angle, setAngle] = useState('90');
  const vec = (s: string): V3 => {
    const p = s.split(/[;,\s]+/).filter(Boolean).map((x) => num(x));
    return [p[0] ?? 0, p[1] ?? 0, p[2] ?? 0];
  };
  const add = () => {
    if (!seed) return;
    if (kind === 'linear') onAdd({ kind, seed: seed.id, direction: vec(dir), spacing: num(spacing), count: Math.round(num(count, 2)) });
    else if (kind === 'circular')
      onAdd({ kind, seed: seed.id, axisPoint: vec(axisPoint), axisDirection: vec(dir), angleStep: num(angle), count: Math.round(num(count, 2)) });
    else if (hole) onAdd({ kind, seed: seed.id, seedHole: hole.ref });
  };
  return (
    <div className="as-panel" data-testid="pattern-panel">
      <h2>{t('assembly.pattern.title')}</h2>
      <p>
        {t('assembly.pattern.seed')} {seed ? seed.name : <span className="as-muted">{t('assembly.pattern.pickSeed')}</span>}
      </p>
      <label className="as-field">
        {t('assembly.pattern.kind')}
        <select value={kind} onChange={(e) => setKind(e.target.value as typeof kind)} data-testid="pattern-kind">
          <option value="linear">{t('assembly.pattern.linear')}</option>
          <option value="circular">{t('assembly.pattern.circular')}</option>
          <option value="feature">{t('assembly.pattern.feature')}</option>
        </select>
      </label>
      {kind !== 'feature' && (
        <label className="as-field">
          {kind === 'linear' ? t('assembly.pattern.direction') : t('assembly.pattern.axisDirection')}
          <input value={dir} onChange={(e) => setDir(e.target.value)} />
        </label>
      )}
      {kind === 'circular' && (
        <label className="as-field">
          {t('assembly.pattern.axisPoint')}
          <input value={axisPoint} onChange={(e) => setAxisPoint(e.target.value)} />
        </label>
      )}
      {kind === 'linear' && (
        <label className="as-field">
          {t('assembly.pattern.spacing')}
          <input value={spacing} onChange={(e) => setSpacing(e.target.value)} />
        </label>
      )}
      {kind === 'circular' && (
        <label className="as-field">
          {t('assembly.pattern.angle')}
          <input value={angle} onChange={(e) => setAngle(e.target.value)} />
        </label>
      )}
      {kind !== 'feature' && (
        <label className="as-field">
          {t('assembly.pattern.count')}
          <input value={count} onChange={(e) => setCount(e.target.value)} />
        </label>
      )}
      {kind === 'feature' && (
        <p>
          {t('assembly.pattern.seedHole')}{' '}
          {hole ? `${hole.label} (${caption(t, 'assembly.geometry.', hole.geometry)})` : <span className="as-muted">{t('assembly.pattern.pickHole')}</span>}
        </p>
      )}
      <button type="button" className="as-primary" disabled={!seed || (kind === 'feature' && !hole)} onClick={add}>
        {t('assembly.pattern.create')}
      </button>
    </div>
  );
};

// ------------------------------------------------------------------ properties

export interface PropertiesPanelProps {
  occurrence: OccurrenceInfo | null;
  definition: DefinitionInfo | null;
  onRename: (name: string) => void;
  onProperties: (p: Partial<PartProperties>) => void;
}

export const PropertiesPanel: React.FC<PropertiesPanelProps> = ({ occurrence, definition, onRename, onProperties }) => {
  const { t } = useI18n();
  const [name, setName] = useState('');
  const [designation, setDesignation] = useState('');
  const [title, setTitle] = useState('');
  const [material, setMaterial] = useState('');
  const [density, setDensity] = useState('');
  useEffect(() => {
    setName(occurrence?.name ?? '');
    setDesignation(definition?.properties.designation ?? '');
    setTitle(definition?.properties.name ?? '');
    setMaterial(definition?.properties.material ?? '');
    setDensity(definition?.properties.density !== null && definition?.properties.density !== undefined ? String(definition.properties.density) : '');
  }, [occurrence, definition]);
  if (!occurrence || !definition) {
    return (
      <div className="as-panel" data-testid="properties-panel">
        <h2>{t('assembly.props.title')}</h2>
        <p className="as-hint">{t('assembly.props.pick')}</p>
      </div>
    );
  }
  return (
    <div className="as-panel" data-testid="properties-panel">
      <h2>{t('assembly.props.component')}</h2>
      <label className="as-field">
        {t('assembly.props.occurrenceName')}
        <input value={name} onChange={(e) => setName(e.target.value)} onBlur={() => name !== occurrence.name && onRename(name)} />
      </label>
      <p className="as-muted">
        {t('assembly.props.definition', {
          name: definition.name,
          kind: definition.kind === 'part' ? t('assembly.props.part') : t('assembly.props.assembly'),
          readOnly: definition.readOnly ? t('assembly.props.readOnly') : '',
          counts: definition.faces !== undefined ? t('assembly.props.counts', { faces: definition.faces, edges: definition.edges ?? 0 }) : '',
        })}
      </p>
      <label className="as-field">
        {t('assembly.props.designation')}
        <input value={designation} onChange={(e) => setDesignation(e.target.value)} data-testid="prop-designation" />
      </label>
      <label className="as-field">
        {t('assembly.props.name')}
        <input value={title} onChange={(e) => setTitle(e.target.value)} />
      </label>
      <label className="as-field">
        {t('assembly.props.material')}
        <input value={material} onChange={(e) => setMaterial(e.target.value)} />
      </label>
      <label className="as-field">
        {t('assembly.props.density')}
        <input value={density} onChange={(e) => setDensity(e.target.value)} data-testid="prop-density" />
      </label>
      <button
        type="button"
        className="as-primary"
        onClick={() =>
          onProperties({
            designation,
            name: title,
            material,
            density: density.trim() === '' ? null : num(density),
          })
        }
        data-testid="prop-apply"
      >
        {t('assembly.props.apply')}
      </button>
    </div>
  );
};
