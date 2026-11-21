/**
 * Panel "Таблица параметров" of the part page (docs/ui/PART_MODELING_UI.md): editable grid of the
 * configuration table — rows are configurations (designation, name), columns are document
 * variables, suppression flags of features and the material. The radio button of a row makes it
 * active (its values drive the variables, then the part is rebuilt). Rows and columns are added
 * and removed, the table is imported from and exported to CSV, every edit is one undo entry of
 * the document (Ctrl+Z).
 */

import React, { useState } from 'react';
import type { ConfigurationCheck, ConfigurationColumn, ConfigurationTable, LibraryMaterial } from '../../wasm/bendcore_modeling_api.mjs';
import { useI18n } from '../app/i18n';
import {
  addRow,
  availableColumns,
  columnFromKey,
  columnKey,
  editRow,
  removeColumn,
  removeRow,
  tableInput,
} from './configurationTable';
import type { TableInput } from './configurationTable';

export interface ConfigurationPanelProps {
  table: ConfigurationTable | null;
  variables: readonly string[];
  features: readonly { id: string; type: string; label: string }[];
  materials: readonly LibraryMaterial[];
  checks: ConfigurationCheck[] | null;
  disabled: boolean;
  onSetTable(table: TableInput): void;
  onSetCell(row: number, column: number, value: string): void;
  onAddColumn(column: ConfigurationColumn): void;
  onActivate(designation: string): void;
  onCheck(): void;
  onImportCsv(text: string): void;
  onExportCsv(decimal: '.' | ','): void;
}

const ConfigurationPanel: React.FC<ConfigurationPanelProps> = ({
  table,
  variables,
  features,
  materials,
  checks,
  disabled,
  onSetTable,
  onSetCell,
  onAddColumn,
  onActivate,
  onCheck,
  onImportCsv,
  onExportCsv,
}) => {
  const { t } = useI18n();
  const [newColumn, setNewColumn] = useState('');
  const [decimal, setDecimal] = useState<'.' | ','>(',');
  if (!table) return null;
  const input = tableInput(table);
  const candidates = availableColumns(input, variables, features);
  const label = (c: ConfigurationColumn) => {
    if (c.kind === 'variable') return c.target;
    if (c.kind === 'material') return t('part.config.material');
    const f = features.find((x) => x.id === c.target);
    return t('part.config.suppress', { feature: f ? f.label : c.target });
  };
  const checkOf = (designation: string) => checks?.find((c) => c.designation === designation) ?? null;

  return (
    <section className="pm-configurations" data-testid="configurations">
      <h2>{t('part.config.title')}</h2>
      <div className="pm-config-scroll">
        <table>
          <thead>
            <tr>
              <th title={t('part.config.activeHint')}>{t('part.config.active')}</th>
              <th>{t('part.config.designation')}</th>
              <th>{t('part.config.name')}</th>
              {table.columns.map((c, k) => (
                <th key={columnKey(c)} data-testid={`config-col-${k}`}>
                  {label(c)}
                  <button
                    type="button"
                    aria-label={t('part.config.removeColumn', { column: label(c) })}
                    data-testid={`config-remove-col-${k}`}
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
                  name="pm-config-active"
                  checked={table.active === ''}
                  disabled={disabled}
                  data-testid="config-active-base"
                  onChange={() => onActivate('')}
                />
              </td>
              <td colSpan={2 + table.columns.length + 1} className="pm-config-base">
                {t('part.config.base')}
              </td>
            </tr>
            {table.rows.map((r, i) => {
              const check = checkOf(r.designation);
              return (
                <tr key={`${i}:${r.designation}`} className={table.active === r.designation ? 'active' : ''} data-testid={`config-row-${i}`}>
                  <td>
                    <input
                      type="radio"
                      name="pm-config-active"
                      checked={table.active === r.designation}
                      disabled={disabled}
                      data-testid={`config-active-${i}`}
                      title={r.fullDesignation ?? r.designation}
                      onChange={() => onActivate(r.designation)}
                    />
                    {check && (
                      <span
                        className={check.ok ? 'pm-config-ok' : 'pm-config-bad'}
                        data-testid={`config-check-${i}`}
                        title={check.ok ? t('part.config.builds') : check.problems.join('\n')}
                      >
                        {check.ok ? '✓' : '!'}
                      </span>
                    )}
                  </td>
                  <td>
                    <input
                      defaultValue={r.designation}
                      key={`d${i}=${r.designation}`}
                      disabled={disabled}
                      data-testid={`config-designation-${i}`}
                      title={r.fullDesignation ?? r.designation}
                      onBlur={(e) => e.target.value !== r.designation && onSetTable(editRow(input, i, 'designation', e.target.value))}
                    />
                  </td>
                  <td>
                    <input
                      defaultValue={r.name}
                      key={`n${i}=${r.name}`}
                      disabled={disabled}
                      data-testid={`config-name-${i}`}
                      onBlur={(e) => e.target.value !== r.name && onSetTable(editRow(input, i, 'name', e.target.value))}
                    />
                  </td>
                  {table.columns.map((c, k) => {
                    const value = r.cells[k] ?? '';
                    const testid = `config-cell-${i}-${k}`;
                    if (c.kind === 'suppression') {
                      return (
                        <td key={columnKey(c)}>
                          <select value={value} disabled={disabled} data-testid={testid} onChange={(e) => onSetCell(i, k, e.target.value)}>
                            <option value="">{t('part.config.inherit')}</option>
                            <option value="0">{t('part.config.on')}</option>
                            <option value="1">{t('part.config.suppressed')}</option>
                          </select>
                        </td>
                      );
                    }
                    if (c.kind === 'material') {
                      return (
                        <td key={columnKey(c)}>
                          <select value={value} disabled={disabled} data-testid={testid} onChange={(e) => onSetCell(i, k, e.target.value)}>
                            <option value="">{t('part.config.inherit')}</option>
                            {materials.map((m) => (
                              <option key={m.id} value={m.id}>
                                {m.name}
                              </option>
                            ))}
                          </select>
                        </td>
                      );
                    }
                    return (
                      <td key={columnKey(c)}>
                        <input
                          defaultValue={value}
                          key={`${testid}=${value}`}
                          placeholder={t('part.config.inherit')}
                          disabled={disabled}
                          data-testid={testid}
                          onBlur={(e) => e.target.value !== value && onSetCell(i, k, e.target.value)}
                        />
                      </td>
                    );
                  })}
                  <td>
                    <button
                      type="button"
                      aria-label={t('part.config.removeRow', { designation: r.designation })}
                      data-testid={`config-remove-row-${i}`}
                      disabled={disabled}
                      onClick={() => onSetTable(removeRow(input, i))}
                    >
                      ×
                    </button>
                  </td>
                </tr>
              );
            })}
          </tbody>
        </table>
      </div>
      <div className="pm-config-actions">
        <button type="button" data-testid="config-add-row" disabled={disabled} onClick={() => onSetTable(addRow(input))}>
          {t('part.config.addRow')}
        </button>
        <select value={newColumn} disabled={disabled || candidates.length === 0} data-testid="config-new-col" onChange={(e) => setNewColumn(e.target.value)}>
          <option value="">{t('part.config.column')}</option>
          {candidates.map((c) => (
            <option key={columnKey(c)} value={columnKey(c)}>
              {label(c)}
            </option>
          ))}
        </select>
        <button
          type="button"
          data-testid="config-add-col"
          disabled={disabled || !newColumn}
          onClick={() => {
            const c = columnFromKey(newColumn);
            if (c) onAddColumn(c);
            setNewColumn('');
          }}
        >
          {t('part.config.addColumn')}
        </button>
        <button type="button" data-testid="config-check" disabled={disabled || table.rows.length === 0} onClick={onCheck}>
          {t('part.config.check')}
        </button>
      </div>
      <div className="pm-config-actions">
        <label className="pm-file-button">
          <input
            type="file"
            accept=".csv,text/csv"
            data-testid="config-import"
            disabled={disabled}
            onChange={(e) => {
              const file = e.target.files?.[0];
              e.target.value = '';
              if (file) void file.text().then(onImportCsv);
            }}
          />
          {t('part.config.import')}
        </label>
        <button type="button" data-testid="config-export" disabled={disabled} onClick={() => onExportCsv(decimal)}>
          {t('part.config.export')}
        </button>
        <select value={decimal} disabled={disabled} data-testid="config-decimal" title={t('part.config.decimal')} onChange={(e) => setDecimal(e.target.value as '.' | ',')}>
          <option value=",">{t('part.config.decimalComma')}</option>
          <option value=".">{t('part.config.decimalPoint')}</option>
        </select>
      </div>
    </section>
  );
};

export default ConfigurationPanel;
