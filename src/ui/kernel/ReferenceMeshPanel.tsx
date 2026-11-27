/**
 * Panel of the reference meshes of a part or an assembly (docs/ui/PART_MODELING_UI.md and
 * docs/ui/ASSEMBLY_UI.md, "Справочная сетка"): "Insert STL mesh…", and per mesh its name, file
 * units, offset and rotation, visibility, opacity, the kernel's figures (triangles, size, area,
 * volume of a closed mesh) and, in the part, the section by a base plane as a sketch. The page
 * calls the kernel; the panel only edits the values.
 */

import React, { useEffect, useState } from 'react';
import { useI18n } from '../app/i18n';
import type { MeshProperties, MeshReadReport, MeshUnit } from '../../wasm/bendcore_kernel_api.mjs';

type Vec3 = [number, number, number];

export const MESH_UNITS: readonly MeshUnit[] = ['mm', 'cm', 'm', 'in'];

export interface ReferenceMeshItem {
  key: string | number;
  name: string;
  fileName: string;
  unit: MeshUnit;
  translation: Vec3;
  rotation: Vec3;
  visible: boolean;
  opacity: number;
  report: MeshReadReport;
  properties: MeshProperties | null;
}

export interface ReferenceMeshEdits {
  name?: string;
  unit?: MeshUnit;
  translation?: Vec3;
  rotation?: Vec3;
  visible?: boolean;
  opacity?: number;
}

export interface ReferenceMeshPanelProps {
  items: readonly ReferenceMeshItem[];
  disabled?: boolean;
  onInsert(file: File): void;
  onEdit(key: string | number, edits: ReferenceMeshEdits): void;
  onRemove(key: string | number): void;
  /** Part only: section of the mesh by a base plane as a sketch of construction lines. */
  onSectionSketch?(key: string | number, plane: 'XY' | 'XZ' | 'YZ', offset: number): void;
}

const fmt = (v: number, digits = 3) => Number(v.toFixed(digits)).toString();

/** Parses three numbers typed as "x, y, z" (commas, spaces or semicolons; decimal point); null if invalid. */
export function parseVec3(text: string): Vec3 | null {
  const parts = text.split(/[\s;,]+/).filter((p) => p.length > 0);
  if (parts.length !== 3) return null;
  const v = parts.map(Number);
  return v.every((x) => Number.isFinite(x)) ? (v as Vec3) : null;
}

export const formatVec3 = (v: Vec3) => v.map((x) => fmt(x, 6)).join(', ');

const MeshRow: React.FC<{
  item: ReferenceMeshItem;
  disabled: boolean;
  onEdit: ReferenceMeshPanelProps['onEdit'];
  onRemove: ReferenceMeshPanelProps['onRemove'];
  onSectionSketch?: ReferenceMeshPanelProps['onSectionSketch'];
}> = ({ item, disabled, onEdit, onRemove, onSectionSketch }) => {
  const { t } = useI18n();
  const [translation, setTranslation] = useState(formatVec3(item.translation));
  const [rotation, setRotation] = useState(formatVec3(item.rotation));
  const [plane, setPlane] = useState<'XY' | 'XZ' | 'YZ'>('XY');
  const [offset, setOffset] = useState('0');
  useEffect(() => setTranslation(formatVec3(item.translation)), [item.translation]);
  useEffect(() => setRotation(formatVec3(item.rotation)), [item.rotation]);
  const tr = parseVec3(translation);
  const rot = parseVec3(rotation);
  const off = offset.trim() === '' ? NaN : Number(offset);
  const p = item.properties;
  const id = String(item.key);
  return (
    <li className="kv-mesh" data-testid={`mesh-${id}`} data-key={id}>
      <label className="pm-field">
        <span>{t('tools.mesh.name')}</span>
        <input
          defaultValue={item.name}
          key={item.name}
          data-testid="mesh-name"
          disabled={disabled}
          onBlur={(e) => e.target.value !== item.name && onEdit(item.key, { name: e.target.value })}
        />
      </label>
      <p className="pm-hint" title={item.fileName}>
        {t('tools.mesh.file')}: {item.fileName}
      </p>
      <label className="pm-field">
        <span>{t('tools.mesh.unit')}</span>
        <select data-testid="mesh-unit" value={item.unit} disabled={disabled} onChange={(e) => onEdit(item.key, { unit: e.target.value as MeshUnit })}>
          {MESH_UNITS.map((u) => (
            <option key={u} value={u}>
              {t(`tools.mesh.unit.${u}` as 'tools.mesh.unit.mm')}
            </option>
          ))}
        </select>
      </label>
      <label className="pm-field">
        <span>{t('tools.mesh.translation')}</span>
        <input data-testid="mesh-translation" value={translation} disabled={disabled} aria-invalid={!tr} onChange={(e) => setTranslation(e.target.value)} />
      </label>
      <label className="pm-field">
        <span>{t('tools.mesh.rotation')}</span>
        <input data-testid="mesh-rotation" value={rotation} disabled={disabled} aria-invalid={!rot} onChange={(e) => setRotation(e.target.value)} />
      </label>
      <button
        type="button"
        data-testid="mesh-apply"
        disabled={disabled || !tr || !rot}
        onClick={() => tr && rot && onEdit(item.key, { translation: tr, rotation: rot })}
      >
        {t('tools.mesh.apply')}
      </button>
      <label className="pm-field">
        <input type="checkbox" data-testid="mesh-visible" checked={item.visible} disabled={disabled} onChange={(e) => onEdit(item.key, { visible: e.target.checked })} />
        <span>{t('tools.mesh.visible')}</span>
      </label>
      <label className="pm-field">
        <span>{t('tools.mesh.opacity')}</span>
        <input
          type="range"
          min={0.05}
          max={1}
          step={0.05}
          data-testid="mesh-opacity"
          defaultValue={item.opacity}
          key={item.opacity}
          disabled={disabled}
          onPointerUp={(e) => onEdit(item.key, { opacity: Number((e.target as HTMLInputElement).value) })}
          onKeyUp={(e) => onEdit(item.key, { opacity: Number((e.target as HTMLInputElement).value) })}
        />
      </label>
      <dl data-testid="mesh-properties">
        <dt>{t('tools.mesh.triangles', { count: p?.triangles ?? 0 })}</dt>
        <dd data-testid="mesh-triangles" data-value={p?.triangles ?? ''} />
        {item.report.degenerateDropped > 0 && <dt>{t('tools.mesh.dropped', { count: item.report.degenerateDropped })}</dt>}
        <dt>{t('tools.mesh.size')}</dt>
        <dd data-testid="mesh-size" data-value={p ? p.size.join(',') : ''}>
          {p ? p.size.map((x) => fmt(x)).join(' × ') : '—'}
        </dd>
        <dt>{t('tools.mesh.area')}</dt>
        <dd data-testid="mesh-area" data-value={p?.area ?? ''}>
          {p ? fmt(p.area) : '—'}
        </dd>
        {p && p.closed && p.volume !== null ? (
          <>
            <dt>{t('tools.mesh.volume')}</dt>
            <dd data-testid="mesh-volume" data-value={p.volume}>
              {fmt(p.volume)}
            </dd>
          </>
        ) : (
          <dt data-testid="mesh-open">{t('tools.mesh.open', { count: p?.openEdges ?? 0 })}</dt>
        )}
      </dl>
      {onSectionSketch && (
        <fieldset className="pm-field" data-testid="mesh-section">
          <legend>{t('tools.mesh.sectionSketch')}</legend>
          <label>
            <span>{t('tools.mesh.sectionPlane')}</span>
            <select data-testid="mesh-section-plane" value={plane} disabled={disabled} onChange={(e) => setPlane(e.target.value as 'XY' | 'XZ' | 'YZ')}>
              <option value="XY">XY</option>
              <option value="XZ">XZ</option>
              <option value="YZ">YZ</option>
            </select>
          </label>
          <label>
            <span>{t('tools.mesh.sectionOffset')}</span>
            <input data-testid="mesh-section-offset" value={offset} disabled={disabled} onChange={(e) => setOffset(e.target.value)} />
          </label>
          <button
            type="button"
            data-testid="mesh-section-make"
            disabled={disabled || !Number.isFinite(off)}
            onClick={() => onSectionSketch(item.key, plane, off)}
          >
            {t('tools.mesh.sectionMake')}
          </button>
        </fieldset>
      )}
      <button type="button" data-testid="mesh-remove" disabled={disabled} onClick={() => onRemove(item.key)}>
        {t('tools.mesh.remove')}
      </button>
    </li>
  );
};

export const ReferenceMeshPanel: React.FC<ReferenceMeshPanelProps> = ({ items, disabled = false, onInsert, onEdit, onRemove, onSectionSketch }) => {
  const { t } = useI18n();
  return (
    <section data-testid="mesh-panel">
      <h2>{t('tools.mesh.title')}</h2>
      <label className="kv-file" title={t('tools.mesh.insertTitle')}>
        <input
          type="file"
          accept=".stl,.3mf,model/stl,model/3mf"
          data-testid="mesh-insert"
          disabled={disabled}
          onChange={(e) => {
            const file = e.target.files?.[0];
            if (file) onInsert(file);
            e.target.value = '';
          }}
        />
        {t('tools.mesh.insert')}
      </label>
      {items.length === 0 ? (
        <p className="pm-hint">{t('tools.mesh.none')}</p>
      ) : (
        <>
          <p className="pm-hint">{t('tools.mesh.reference')}</p>
          <ul className="kv-mesh-list">
            {items.map((item) => (
              <MeshRow key={String(item.key)} item={item} disabled={disabled} onEdit={onEdit} onRemove={onRemove} onSectionSketch={onSectionSketch} />
            ))}
          </ul>
        </>
      )}
    </section>
  );
};
