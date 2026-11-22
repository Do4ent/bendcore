/**
 * Panels of the GOST 2.113 group documents (docs/ui/DRAWING_UI.md, "Групповые документы"):
 * GroupDrawingPanel — switch of the group drawing of a part, columns of the table of
 * configurations with editable headers, the part's configuration table (cells change the model,
 * then "Обновить по модели"); GroupSpecificationPanel — switch of the group specification of an
 * assembly, the layout of the variable data, editable headers of the quantity columns and the table
 * with the quantities per configuration.
 */

import React from 'react';
import type {
  ConfigurationTableInfo,
  DrawingDescription,
  DrawingHandle,
  GroupColumn,
  SpecificationTable,
} from '../../wasm/bendcore_drawing_api.mjs';
import type { Remote } from '../worker/wasmClient';
import type { MessageKey } from '../app/i18n';
import { GROUP_SPEC_LAYOUTS, columnHeadersAfter, groupSpecRows, headerChange, isGroupSpecLayout, lettersSummary } from './groupDocuments';

type Drawing = Remote<DrawingHandle>;
type Translate = (key: MessageKey, params?: Record<string, string | number>) => string;
type Run = (label: string, action: (d: Drawing) => unknown) => Promise<boolean>;

/** Text input that commits on Enter or blur when the value changed. */
function CommitInput(props: { value: string; title?: string; testId: string; size?: number; onCommit: (v: string) => void }) {
  const { value, title, testId, size, onCommit } = props;
  return (
    <input
      key={value}
      defaultValue={value}
      title={title}
      size={size}
      data-testid={testId}
      onKeyDown={(e) => {
        if (e.key === 'Enter') (e.target as HTMLInputElement).blur();
      }}
      onBlur={(e) => {
        if (e.target.value !== value) onCommit(e.target.value);
      }}
    />
  );
}

export function GroupDrawingPanel(props: { desc: DrawingDescription; configurations: ConfigurationTableInfo | null; run: Run; t: Translate }) {
  const { desc, configurations, run, t } = props;
  const g = desc.group;
  if (!g.available) return null;
  const table = g.table;
  return (
    <div className="dr-box" data-testid="group-panel">
      <h2>{t('drawing.group.title')}</h2>
      <label className="dr-check" title={t('drawing.group.enabledHint')}>
        <input
          type="checkbox"
          checked={g.enabled}
          data-testid="group-enabled"
          disabled={!g.enabled && g.configurations < 2}
          onChange={() =>
            run(t(g.enabled ? 'drawing.done.groupOff' : 'drawing.done.group'), (d) => d.setSettings({ group: { enabled: !g.enabled } }))
          }
        />
        {t('drawing.group.enabled')}
      </label>
      {g.configurations < 2 && <div className="dr-muted">{t('drawing.group.noTable')}</div>}
      {g.enabled && (
        <div className="dr-row">
          <label className="dr-check">
            <input type="checkbox" checked={g.mass} data-testid="group-mass" onChange={() => run(t('drawing.done.group'), (d) => d.setSettings({ group: { mass: !g.mass } }))} />
            {t('drawing.group.mass')}
          </label>
          <label className="dr-check">
            <input
              type="checkbox"
              checked={g.material}
              data-testid="group-material"
              onChange={() => run(t('drawing.done.group'), (d) => d.setSettings({ group: { material: !g.material } }))}
            />
            {t('drawing.group.material')}
          </label>
        </div>
      )}
      {g.enabled && table && (
        <>
          <div className="dr-muted" data-testid="group-letters">
            {table.dimensions.length ? t('drawing.group.letters', { list: lettersSummary(table.dimensions) }) : t('drawing.group.lettersNone')}
          </div>
          <h3>{t('drawing.group.columns')}</h3>
          <table className="dr-vars" data-testid="group-table">
            <thead>
              <tr>
                {table.columns.map((c: GroupColumn) => (
                  <th key={c.key}>
                    <CommitInput
                      value={c.header}
                      size={Math.max(3, c.header.length)}
                      title={t('drawing.group.headerHint', { default: c.defaultHeader })}
                      testId={`group-header-${c.key}`}
                      onCommit={(v) => {
                        const change = headerChange(c, v);
                        if (change) run(t('drawing.done.groupHeader', { key: c.defaultHeader }), (d) => d.setSettings(change));
                      }}
                    />
                  </th>
                ))}
              </tr>
            </thead>
            <tbody>
              {table.rows.map((row, r) => (
                <tr key={table.designations[r]} data-testid={`group-row-${r}`}>
                  {row.map((cell, c) => (
                    <td key={table.columns[c].key} data-testid={`group-cell-${r}-${table.columns[c].key}`}>{cell}</td>
                  ))}
                </tr>
              ))}
            </tbody>
          </table>
        </>
      )}
      {configurations && configurations.rows.length > 0 && (
        <>
          <h3>{t('drawing.group.parameters')}</h3>
          <table className="dr-vars" data-testid="configuration-table">
            <thead>
              <tr>
                <th />
                {configurations.columns.map((c) => (
                  <th key={c.title}>{c.title}</th>
                ))}
              </tr>
            </thead>
            <tbody>
              {configurations.rows.map((row, r) => (
                <tr key={row.designation}>
                  <td>{row.fullDesignation}</td>
                  {row.cells.map((cell, c) => (
                    <td key={configurations.columns[c].title}>
                      <CommitInput
                        value={cell}
                        size={Math.max(3, cell.length)}
                        testId={`configuration-cell-${r}-${c}`}
                        onCommit={(v) =>
                          run(t('drawing.done.configurationCell', { row: row.designation, column: configurations.columns[c].title }), (d) =>
                            d.setConfigurationCell(r, c, v),
                          )
                        }
                      />
                    </td>
                  ))}
                </tr>
              ))}
            </tbody>
          </table>
        </>
      )}
    </div>
  );
}

export function GroupSpecificationPanel(props: { desc: DrawingDescription; table: SpecificationTable | null; run: Run; t: Translate }) {
  const { desc, table, run, t } = props;
  const gs = desc.groupSpecification;
  if (!gs) return null;
  const group = table?.group ?? null;
  const rows = groupSpecRows(table);
  return (
    <div className="dr-box" data-testid="group-spec-panel">
      <h2>{t('drawing.groupSpec.title')}</h2>
      <label className="dr-check">
        <input
          type="checkbox"
          checked={gs.enabled}
          data-testid="group-spec-enabled"
          onChange={() =>
            run(t(gs.enabled ? 'drawing.done.groupSpecOff' : 'drawing.done.groupSpec'), (d) => d.setSettings({ groupSpecification: { enabled: !gs.enabled } }))
          }
        />
        {t('drawing.groupSpec.enabled')}
      </label>
      {gs.enabled && (
        <label>
          {t('drawing.groupSpec.layout')}
          <select
            value={gs.layout}
            data-testid="group-spec-layout"
            onChange={(e) => {
              const layout = e.target.value;
              if (isGroupSpecLayout(layout)) run(t('drawing.done.groupSpec'), (d) => d.setSettings({ groupSpecification: { layout } }));
            }}
          >
            {GROUP_SPEC_LAYOUTS.map((l) => (
              <option key={l} value={l}>{t(`drawing.groupSpec.layout.${l}`)}</option>
            ))}
          </select>
        </label>
      )}
      {gs.enabled && group && (
        <table data-testid="group-spec-table">
          <thead>
            <tr>
              <th>{t('drawing.spec.position')}</th>
              <th>{t('drawing.spec.designation')}</th>
              <th>{t('drawing.spec.name')}</th>
              {group.columnHeaders.map((h, i) => (
                <th key={group.configurations[i]} title={group.configurations[i]}>
                  <CommitInput
                    value={h}
                    size={Math.max(3, h.length)}
                    title={t('drawing.groupSpec.header', { designation: group.configurations[i] })}
                    testId={`group-spec-header-${i}`}
                    onCommit={(v) => {
                      // Defaults: the kernel's headers when nothing is set ("–", "01", …).
                      const defaults = group.columnHeaders.map((x, k) => (gs.columnHeaders[k] ? '' : x));
                      const columnHeaders = columnHeadersAfter(gs.columnHeaders, group.configurations.length, i, v, defaults);
                      run(t('drawing.done.groupSpecHeader'), (d) => d.setSettings({ groupSpecification: { columnHeaders } }));
                    }}
                  />
                </th>
              ))}
              <th>{t('drawing.spec.note')}</th>
            </tr>
          </thead>
          <tbody>
            {rows.map((r) =>
              r.kind === 'section' ? (
                <tr key={r.key} className="dr-spec-section"><td colSpan={4 + group.configurations.length}>{r.title}</td></tr>
              ) : (
                <tr key={r.key} data-testid={r.position ? `group-spec-position-${r.position}` : undefined} data-constant={r.constant ? 'yes' : 'no'}>
                  <td>{r.position || ''}</td>
                  <td>{r.designation}</td>
                  <td>{r.name}</td>
                  {r.quantities!.map((q, i) => (
                    <td key={group.configurations[i]} data-testid={r.position ? `group-spec-qty-${r.position}-${i}` : undefined}>{q}</td>
                  ))}
                  <td>{r.note}</td>
                </tr>
              ),
            )}
          </tbody>
        </table>
      )}
    </div>
  );
}
