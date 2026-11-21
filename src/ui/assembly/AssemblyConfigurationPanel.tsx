/**
 * Panel "Исполнения сборки" of the assembly page (docs/ui/ASSEMBLY_UI.md): grid of the assembly
 * configuration table — rows are configurations (designation, name), columns are the inclusion of
 * a component, the configuration of a component (part or subassembly document) and the value of a
 * distance/angle/gear mate. The radio button of a row makes it active: the composition changes and
 * the mates are solved again. Every edit and every activation is one undo step; the table is
 * imported from and exported to CSV.
 */

import React, { useState } from 'react';
import type {
  AssemblyConfigurationColumn,
  AssemblyConfigurationTableInfo,
  AssemblyState,
  ConfigurationActivation,
} from '../../wasm/bendcore_assembly_api.mjs';
import { caption, useI18n } from '../app/i18n';
import {
  addColumn,
  addRow,
  assemblyColumnFromKey,
  assemblyColumnKey,
  assemblyTableInput,
  availableAssemblyColumns,
  editRow,
  removeColumn,
  removeRow,
  setCell,
} from './configurationTable';
import type { AssemblyTableInput } from './configurationTable';

export interface AssemblyConfigurationPanelProps {
  table: AssemblyConfigurationTableInfo | null;
  state: AssemblyState;
  activation: ConfigurationActivation | null;
  disabled: boolean;
  onSetTable(table: AssemblyTableInput): void;
  onActivate(designation: string): void;
  onUndo(): void;
  onImportCsv(text: string): void;
  onExportCsv(decimal: '.' | ','): void;
}

const AssemblyConfigurationPanel: React.FC<AssemblyConfigurationPanelProps> = ({
  table,
  state,
  activation,
  disabled,
  onSetTable,
  onActivate,
  onUndo,
  onImportCsv,
  onExportCsv,
}) => {
  const { t } = useI18n();
  const [newColumn, setNewColumn] = useState('');
  const [decimal, setDecimal] = useState<'.' | ','>(',');
  if (!table) return null;
  const input = assemblyTableInput(table);
  const candidates = availableAssemblyColumns(input, state);
  const occurrenceName = (id: number) => state.occurrences.find((o) => o.id === id)?.name ?? `#${id}`;
  const mateName = (id: number) => {
    const m = state.mates.find((x) => x.id === id);
    if (!m) return `#${id}`;
    return m.name || `${caption(t, 'assembly.mateType.', m.type)} ${m.id}`;
  };
  const label = (c: AssemblyConfigurationColumn) => {
    if (c.kind === 'suppression') return t('assembly.config.include', { component: occurrenceName(c.target) });
    if (c.kind === 'configuration') return t('assembly.config.componentConfiguration', { component: occurrenceName(c.target) });
    return t('assembly.config.mateValue', { mate: mateName(c.target) });
  };
  const unitOf = (k: number) => {
    const u = table.columns[k]?.unit;
    return u === 'deg' ? t('assembly.config.unitDeg') : u === 'ratio' ? '' : u === 'mm' ? t('assembly.config.unitMm') : '';
  };
  const baseText = (k: number) => {
    const c = table.columns[k];
    const v = table.base[k] ?? '';
    if (!c) return v;
    if (c.kind === 'suppression') return v === '1' ? t('assembly.config.excluded') : t('assembly.config.included');
    if (c.kind === 'configuration') return v === '#' ? t('assembly.config.asSaved') : v;
    return `${v} ${unitOf(k)}`.trim();
  };
  const history = state.history;

  return (
    <div className="as-panel as-wide" data-testid="asm-configurations">
      <h2>{t('assembly.config.title')}</h2>
      <p className="as-hint" data-testid="asm-config-active-designation">
        {table.active === ''
          ? t('assembly.config.activeBase')
          : t('assembly.config.activeNamed', { designation: table.designation || table.active })}
      </p>
      <div className="pm-config-scroll">
        <table className="as-config">
          <thead>
            <tr>
              <th title={t('assembly.config.activeHint')}>{t('assembly.config.active')}</th>
              <th>{t('assembly.config.designation')}</th>
              <th>{t('assembly.config.name')}</th>
              {input.columns.map((c, k) => (
                <th key={assemblyColumnKey(c)} data-testid={`asm-config-col-${k}`} title={table.columns[k]?.title}>
                  {label(c)}
                  <button
                    type="button"
                    aria-label={t('assembly.config.removeColumn', { column: label(c) })}
                    data-testid={`asm-config-remove-col-${k}`}
                    disabled={disabled}
                    onClick={() => onSetTable(removeColumn(input, k))}
                  >
                    ×
                  </button>
                </th>
              ))}
              <th />
            </tr>
          </thead>
          <tbody>
            <tr className={table.active === '' ? 'active' : ''}>
              <td>
                <input
                  type="radio"
                  name="as-config-active"
                  checked={table.active === ''}
                  disabled={disabled}
                  data-testid="asm-config-active-base"
                  onChange={() => onActivate('')}
                />
              </td>
              <td colSpan={2} className="pm-config-base">
                {t('assembly.config.base')}
              </td>
              {input.columns.map((c, k) => (
                <td key={assemblyColumnKey(c)} className="pm-config-base" data-testid={`asm-config-base-${k}`}>
                  {baseText(k)}
                </td>
              ))}
              <td />
            </tr>
            {table.rows.map((r, i) => (
              <tr key={`${i}:${r.designation}`} className={table.active === r.designation ? 'active' : ''} data-testid={`asm-config-row-${i}`}>
                <td>
                  <input
                    type="radio"
                    name="as-config-active"
                    checked={table.active === r.designation}
                    disabled={disabled}
                    data-testid={`asm-config-active-${i}`}
                    title={r.fullDesignation}
                    onChange={() => onActivate(r.designation)}
                  />
                </td>
                <td>
                  <input
                    defaultValue={r.designation}
                    key={`d${i}=${r.designation}`}
                    disabled={disabled}
                    data-testid={`asm-config-designation-${i}`}
                    title={r.fullDesignation}
                    onBlur={(e) => e.target.value !== r.designation && onSetTable(editRow(input, i, 'designation', e.target.value))}
                  />
                </td>
                <td>
                  <input
                    defaultValue={r.name}
                    key={`n${i}=${r.name}`}
                    disabled={disabled}
                    data-testid={`asm-config-name-${i}`}
                    onBlur={(e) => e.target.value !== r.name && onSetTable(editRow(input, i, 'name', e.target.value))}
                  />
                </td>
                {input.columns.map((c, k) => {
                  const value = r.cells[k] ?? '';
                  const testid = `asm-config-cell-${i}-${k}`;
                  const change = (v: string) => onSetTable(setCell(input, i, k, v));
                  if (c.kind === 'suppression') {
                    return (
                      <td key={assemblyColumnKey(c)}>
                        <select value={value} disabled={disabled} data-testid={testid} onChange={(e) => change(e.target.value)}>
                          <option value="">{t('assembly.config.inherit')}</option>
                          <option value="0">{t('assembly.config.included')}</option>
                          <option value="1">{t('assembly.config.excluded')}</option>
                        </select>
                      </td>
                    );
                  }
                  if (c.kind === 'configuration') {
                    const options = table.columns[k]?.options ?? [];
                    return (
                      <td key={assemblyColumnKey(c)}>
                        <select value={value} disabled={disabled} data-testid={testid} onChange={(e) => change(e.target.value)}>
                          <option value="">{t('assembly.config.inherit')}</option>
                          <option value="#">{t('assembly.config.asSaved')}</option>
                          {options.map((o) => (
                            <option key={o.designation} value={o.designation}>
                              {o.name ? `${o.fullDesignation} — ${o.name}` : o.fullDesignation}
                            </option>
                          ))}
                          {value !== '' && value !== '#' && !options.some((o) => o.designation === value) && <option value={value}>{value}</option>}
                        </select>
                      </td>
                    );
                  }
                  return (
                    <td key={assemblyColumnKey(c)}>
                      <input
                        defaultValue={value}
                        key={`${testid}=${value}`}
                        placeholder={t('assembly.config.inherit')}
                        disabled={disabled}
                        data-testid={testid}
                        title={unitOf(k)}
                        onBlur={(e) => e.target.value !== value && change(e.target.value)}
                      />
                    </td>
                  );
                })}
                <td>
                  <button
                    type="button"
                    aria-label={t('assembly.config.removeRow', { designation: r.designation })}
                    data-testid={`asm-config-remove-row-${i}`}
                    disabled={disabled}
                    onClick={() => onSetTable(removeRow(input, i))}
                  >
                    ×
                  </button>
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
      <div className="pm-config-actions">
        <button type="button" data-testid="asm-config-add-row" disabled={disabled} onClick={() => onSetTable(addRow(input))}>
          {t('assembly.config.addRow')}
        </button>
        <select
          value={newColumn}
          disabled={disabled || candidates.length === 0}
          data-testid="asm-config-new-col"
          onChange={(e) => setNewColumn(e.target.value)}
        >
          <option value="">{t('assembly.config.column')}</option>
          {candidates.map((c) => (
            <option key={assemblyColumnKey(c)} value={assemblyColumnKey(c)}>
              {label(c)}
            </option>
          ))}
        </select>
        <button
          type="button"
          data-testid="asm-config-add-col"
          disabled={disabled || !newColumn}
          onClick={() => {
            const c = assemblyColumnFromKey(newColumn);
            if (c) onSetTable(addColumn(input, c));
            setNewColumn('');
          }}
        >
          {t('assembly.config.addColumn')}
        </button>
        <button
          type="button"
          data-testid="asm-config-undo"
          disabled={disabled || !history.canUndo}
          title={history.undoLabel ? t('assembly.undoNamed', { name: history.undoLabel }) : t('assembly.undo')}
          onClick={onUndo}
        >
          {t('assembly.undo')}
        </button>
      </div>
      <div className="pm-config-actions">
        <label className="pm-file-button">
          <input
            type="file"
            accept=".csv,text/csv"
            data-testid="asm-config-import"
            disabled={disabled}
            onChange={(e) => {
              const file = e.target.files?.[0];
              e.target.value = '';
              if (file) void file.text().then(onImportCsv);
            }}
          />
          {t('assembly.config.import')}
        </label>
        <button type="button" data-testid="asm-config-export" disabled={disabled} onClick={() => onExportCsv(decimal)}>
          {t('assembly.config.export')}
        </button>
        <select
          value={decimal}
          disabled={disabled}
          data-testid="asm-config-decimal"
          title={t('assembly.config.decimal')}
          onChange={(e) => setDecimal(e.target.value as '.' | ',')}
        >
          <option value=",">{t('assembly.config.decimalComma')}</option>
          <option value=".">{t('assembly.config.decimalPoint')}</option>
        </select>
      </div>
      {activation && (
        <div className="as-hint" data-testid="asm-config-report">
          <div>{t('assembly.config.excludedCount', { n: activation.excluded.length })}</div>
          {activation.disabledMates.length > 0 && (
            <div data-testid="asm-config-disabled-mates">
              {t('assembly.config.disabledMates', { n: activation.disabledMates.length })}
              <ul>
                {activation.disabledMates.map((d) => (
                  <li key={d.mate}>
                    {t('assembly.config.disabledMate', { mate: mateName(d.mate), component: occurrenceName(d.occurrence) })}
                  </li>
                ))}
              </ul>
            </div>
          )}
          {activation.brokenMates.map((b) => (
            <div key={b.mate} className="as-error">
              {t('assembly.config.brokenMate', { mate: mateName(b.mate), reason: b.reason })}
            </div>
          ))}
        </div>
      )}
    </div>
  );
};

export default AssemblyConfigurationPanel;
