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
import { FASTENER_KIND_LABELS, GEOMETRY_LABELS, MATE_LABELS, massText } from './labels';
import { MATE_TYPES, allowedMateTypes, mateValueUnit, suggestMateType } from './mateRules';
import type { PickedElement } from './mateRules';

const num = (s: string, fallback = 0) => {
  const v = Number(s.replace(',', '.'));
  return Number.isFinite(v) ? v : fallback;
};

function PickSlot({ title, pick, onClear, testId }: { title: string; pick: PickedElement | null; onClear: () => void; testId: string }) {
  return (
    <div className={`as-slot ${pick ? 'as-slot-filled' : ''}`} data-testid={testId}>
      <span className="as-slot-title">{title}</span>
      {pick ? (
        <>
          <span className="as-slot-label">
            {pick.label} — {GEOMETRY_LABELS[pick.geometry] ?? pick.geometry}
            {pick.entity?.radius !== undefined ? ` R${+pick.entity.radius.toFixed(4)}` : ''}
          </span>
          <button type="button" onClick={onClear} title="Очистить">
            ✕
          </button>
        </>
      ) : (
        <span className="as-muted">выберите во вьюпорте</span>
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
      <h2>Сопряжение</h2>
      <PickSlot title="A" pick={a} onClear={() => onClearSlot(0)} testId="mate-slot-a" />
      <PickSlot title="B" pick={b} onClear={() => onClearSlot(1)} testId="mate-slot-b" />
      {a && b && allowed.length === 0 && <p className="as-warn">Элементы одного компонента: выберите элемент другого компонента.</p>}
      <div className="as-types" role="radiogroup">
        {MATE_TYPES.map((t) => (
          <label key={t} className={allowed.includes(t) ? '' : 'as-disabled'}>
            <input
              type="radio"
              name="mate-type"
              value={t}
              disabled={!allowed.includes(t)}
              checked={type === t}
              onChange={() => setType(t)}
              data-testid={`mate-type-${t}`}
            />
            {MATE_LABELS[t]}
          </label>
        ))}
      </div>
      {unit && (
        <label className="as-field">
          {unit === 'mm' ? 'Расстояние, мм' : 'Угол, °'}
          <input data-testid="mate-value" value={value} onChange={(e) => setValue(e.target.value)} />
        </label>
      )}
      {type && type !== 'rigid' && type !== 'perpendicular' && type !== 'angle' && (
        <label className="as-field">
          {type === 'tangent' ? 'Сторона' : 'Ориентация'}
          <select value={alignment} onChange={(e) => setAlignment(e.target.value as Alignment)} data-testid="mate-alignment">
            <option value="closest">ближайшая</option>
            <option value="aligned">сонаправлено</option>
            <option value="anti-aligned">встречно</option>
          </select>
        </label>
      )}
      {type === 'tangent' && (
        <label className="as-field">
          <input type="checkbox" checked={internal} onChange={(e) => setInternal(e.target.checked)} />
          внутреннее касание (цилиндры)
        </label>
      )}
      <button type="button" className="as-primary" disabled={!a || !b || !type} onClick={add} data-testid="mate-add">
        Добавить сопряжение
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
        {none && <option value="">нет</option>}
        {list.map((c) => (
          <option key={c.id} value={c.id}>
            {FASTENER_KIND_LABELS[c.kind]} {c.designation}
          </option>
        ))}
      </select>
    </label>
  );
  return (
    <div className="as-panel" data-testid="fastener-panel">
      <h2>Библиотека крепежа</h2>
      <div className="as-types">
        <label>
          <input type="radio" checked={system === 'gost'} onChange={() => setSystem('gost')} data-testid="fastener-gost" />
          ГОСТ
        </label>
        <label>
          <input type="radio" checked={system === 'iso'} onChange={() => setSystem('iso')} data-testid="fastener-iso" />
          ISO
        </label>
      </div>
      {select('Болт / винт', screw?.id ?? '', setScrewId, screws, 'fastener-screw', false)}
      <label className="as-field">
        Размер
        <select value={size} onChange={(e) => setSize(Number(e.target.value))} data-testid="fastener-size">
          {sizes.map((d) => (
            <option key={d} value={d}>
              M{d}
            </option>
          ))}
        </select>
      </label>
      <label className="as-field">
        Длина, мм
        <input value={length} onChange={(e) => setLength(e.target.value)} data-testid="fastener-length" />
      </label>
      {select('Шайба под головку', hw, setHeadWasher, washers, 'fastener-head-washer')}
      {select('Шайба под гайку', nw, setNutWasher, washers, 'fastener-nut-washer')}
      {select('Гайка', nt, setNut, nuts, 'fastener-nut')}
      {description && (
        <p className="as-muted" data-testid="fastener-description">
          {description.designation}, {massText(description.mass)} кг
          {screw && !screw.verified ? ' (таблица требует сверки)' : ''}
        </p>
      )}
      {descriptionError && <p className="as-warn">{descriptionError}</p>}
      <PickSlot title="Отверстие" pick={hole} onClear={onClearHole} testId="fastener-hole" />
      {hole && !isHole && <p className="as-warn">Выберите цилиндрическую грань отверстия.</p>}
      <label className="as-field">
        <input type="checkbox" checked={allHoles} onChange={(e) => setAllHoles(e.target.checked)} data-testid="fastener-all" />
        во все отверстия этого диаметра и направления
      </label>
      <p className="as-hint">Головка ставится со стороны камеры; ось — по оси отверстия.</p>
      <button type="button" className="as-primary" disabled={!isHole || !screwSpec} onClick={insert} data-testid="fastener-insert">
        Поставить крепёж
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
  const pairs = report ? report.pairs.map((p, i) => ({ ...p, i })).filter((p) => showTouching || p.kind === 'interference') : [];
  const interfering = report ? report.pairs.filter((p) => p.kind === 'interference').length : 0;
  return (
    <div className="as-panel" data-testid="interference-panel">
      <h2>Проверка пересечений</h2>
      <button type="button" className="as-primary" onClick={onCheck} data-testid="interference-check">
        Проверить
      </button>
      <label className="as-field">
        <input type="checkbox" checked={showTouching} onChange={(e) => onShowTouching(e.target.checked)} />
        показывать касания
      </label>
      {report && (
        <p data-testid="interference-summary">
          Пересечений: {interfering}, касаний: {report.pairs.length - interfering}; пар деталей {report.totalPairs}, по габаритам{' '}
          {report.candidatePairs}; объём пересечений {report.totalVolume.toFixed(3)} мм³
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
            <span className="as-status">{p.kind === 'interference' ? `${p.volume.toFixed(3)} мм³` : 'касание'}</span>
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
}) => (
  <div className="as-panel as-wide" data-testid="bom-panel">
    <h2>Состав изделия (BOM)</h2>
    <div className="as-types">
      <label>
        <input type="radio" checked={mode === 'flat'} onChange={() => onMode('flat')} data-testid="bom-flat" />
        плоский
      </label>
      <label>
        <input type="radio" checked={mode === 'structured'} onChange={() => onMode('structured')} data-testid="bom-structured" />
        по уровням
      </label>
    </div>
    {bom && (
      <table className="as-bom" data-testid="bom-table">
        <thead>
          <tr>
            <th>Поз.</th>
            <th>Обозначение</th>
            <th>Наименование</th>
            <th>Материал</th>
            <th>Кол.</th>
            <th>Масса ед., кг</th>
            <th>Масса, кг</th>
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
            <td colSpan={4}>Всего деталей</td>
            <td data-testid="bom-total-parts">{bom.totalParts}</td>
            <td />
            <td data-testid="bom-total-mass">{massText(bom.totalMass)}</td>
          </tr>
        </tfoot>
      </table>
    )}
    <p className="as-hint">Масса — объём тела × плотность из свойств детали; без плотности масса не считается.</p>
  </div>
);

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
  const [steps, setSteps] = useState<{ occurrences: number[]; direction: V3; distance: number }[]>([]);
  const [direction, setDirection] = useState('+Z');
  const [distance, setDistance] = useState('50');
  const [name, setName] = useState('Разнесённый вид');
  const current = view !== null ? state.explodedViews[view] : null;
  const nameOf = (id: number) => state.occurrences.find((o) => o.id === id)?.name ?? String(id);
  return (
    <div className="as-panel" data-testid="explode-panel">
      <h2>Разнесённые виды</h2>
      <ul className="as-list">
        {state.explodedViews.map((v, i) => (
          <li key={i} className={`as-row ${view === i ? 'as-selected' : ''}`} onClick={() => onSelectView(view === i ? null : i)}>
            <span className="as-name">
              {v.name} ({v.steps.length} шаг.)
            </span>
            <span className="as-actions" onClick={(e) => e.stopPropagation()}>
              <button type="button" onClick={() => onRemoveView(i)} title="Удалить вид">
                ✕
              </button>
            </span>
          </li>
        ))}
      </ul>
      {current && (
        <label className="as-field">
          Шаг {progress.toFixed(2)} из {current.steps.length}
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
      <h2>Новый вид</h2>
      <label className="as-field">
        Название
        <input value={name} onChange={(e) => setName(e.target.value)} />
      </label>
      <label className="as-field">
        Направление
        <select value={direction} onChange={(e) => setDirection(e.target.value)} data-testid="explode-direction">
          {Object.keys(DIRECTIONS).map((d) => (
            <option key={d}>{d}</option>
          ))}
        </select>
      </label>
      <label className="as-field">
        Смещение, мм
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
        Добавить шаг для выбранного компонента
      </button>
      <ol className="as-list">
        {steps.map((s, i) => (
          <li key={i}>
            {s.occurrences.map(nameOf).join(', ')}: {s.distance} мм по ({s.direction.join(', ')})
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
        Создать вид
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
      <h2>Массив компонентов</h2>
      <p>Исходный компонент: {seed ? seed.name : <span className="as-muted">выберите в дереве</span>}</p>
      <label className="as-field">
        Вид
        <select value={kind} onChange={(e) => setKind(e.target.value as typeof kind)} data-testid="pattern-kind">
          <option value="linear">линейный</option>
          <option value="circular">круговой</option>
          <option value="feature">по отверстиям детали</option>
        </select>
      </label>
      {kind !== 'feature' && (
        <label className="as-field">
          {kind === 'linear' ? 'Направление (x, y, z)' : 'Ось: направление (x, y, z)'}
          <input value={dir} onChange={(e) => setDir(e.target.value)} />
        </label>
      )}
      {kind === 'circular' && (
        <label className="as-field">
          Ось: точка (x, y, z)
          <input value={axisPoint} onChange={(e) => setAxisPoint(e.target.value)} />
        </label>
      )}
      {kind === 'linear' && (
        <label className="as-field">
          Шаг, мм
          <input value={spacing} onChange={(e) => setSpacing(e.target.value)} />
        </label>
      )}
      {kind === 'circular' && (
        <label className="as-field">
          Угловой шаг, °
          <input value={angle} onChange={(e) => setAngle(e.target.value)} />
        </label>
      )}
      {kind !== 'feature' && (
        <label className="as-field">
          Количество (с исходным)
          <input value={count} onChange={(e) => setCount(e.target.value)} />
        </label>
      )}
      {kind === 'feature' && (
        <p>
          Отверстие исходного компонента:{' '}
          {hole ? `${hole.label} (${GEOMETRY_LABELS[hole.geometry] ?? hole.geometry})` : <span className="as-muted">выберите грань во вьюпорте</span>}
        </p>
      )}
      <button type="button" className="as-primary" disabled={!seed || (kind === 'feature' && !hole)} onClick={add}>
        Создать массив
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
        <h2>Свойства</h2>
        <p className="as-hint">Выберите компонент в дереве или во вьюпорте.</p>
      </div>
    );
  }
  return (
    <div className="as-panel" data-testid="properties-panel">
      <h2>Свойства компонента</h2>
      <label className="as-field">
        Имя вхождения
        <input value={name} onChange={(e) => setName(e.target.value)} onBlur={() => name !== occurrence.name && onRename(name)} />
      </label>
      <p className="as-muted">
        Определение: {definition.name} ({definition.kind === 'part' ? 'деталь' : 'сборка'}
        {definition.readOnly ? ', только чтение' : ''}){definition.faces !== undefined ? `, граней ${definition.faces}, рёбер ${definition.edges}` : ''}
      </p>
      <label className="as-field">
        Обозначение
        <input value={designation} onChange={(e) => setDesignation(e.target.value)} data-testid="prop-designation" />
      </label>
      <label className="as-field">
        Наименование
        <input value={title} onChange={(e) => setTitle(e.target.value)} />
      </label>
      <label className="as-field">
        Материал
        <input value={material} onChange={(e) => setMaterial(e.target.value)} />
      </label>
      <label className="as-field">
        Плотность, кг/м³
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
        Применить
      </button>
    </div>
  );
};
