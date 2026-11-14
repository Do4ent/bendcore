/**
 * История версий чертежа: список версий ассоциативного чертежа (drawing.make_associative)
 * и состояние ассоциативности. Отката и сравнения версий ядро не предоставляет
 * (команд нет в src/core_api.cpp), поэтому панель их не предлагает.
 */
import React from 'react';
import { t } from './core/i18n';
import type { AssociativityStatus, DrawingVersion } from './core/types';
import './panels.css';

export interface DrawingVersionPanelProps {
  currentRevision: number;
  versions: DrawingVersion[];
  status: AssociativityStatus;
}

const statusDisplay = {
  fresh: { icon: '✓', text: 'Свежий', color: '#4caf50' },
  stale: { icon: '⚠', text: 'Устарелый', color: '#ff9800' },
  broken: { icon: '✗', text: 'Ошибка', color: '#f44336' },
  not_applicable: { icon: '-', text: 'N/A', color: '#9e9e9e' },
};

export const DrawingVersionPanel: React.FC<DrawingVersionPanelProps> = ({
  currentRevision,
  versions,
  status,
}) => {
  const disp = statusDisplay[status];

  return (
    <section className="bc-panel" style={{ height: '100%', display: 'flex', flexDirection: 'column' }}>
      <div className="bc-header" style={{ paddingBottom: '8px', borderBottom: '1px solid var(--bc-line)' }}>
        <h2 style={{ margin: '0 0 8px', fontSize: '14px', fontWeight: 600 }}>История чертежа</h2>
        <div
          style={{
            display: 'inline-flex',
            alignItems: 'center',
            gap: '4px',
            padding: '4px 8px',
            borderRadius: '4px',
            backgroundColor: disp.color + '22',
            color: disp.color,
            fontSize: '12px',
            fontWeight: 500,
          }}
        >
          <span>{disp.icon}</span>
          <span>{disp.text}</span>
        </div>
      </div>

      <div className="bc-body" style={{ flex: 1, overflow: 'auto', padding: '8px' }}>
        <div style={{ fontSize: '11px', color: 'var(--bc-text-secondary)', marginBottom: '8px' }}>
          Всего версий: <strong>{versions.length}</strong>
        </div>

        <table className="bc-table" style={{ fontSize: '12px' }}>
          <thead>
            <tr>
              <th style={{ width: '50px' }}>Rev</th>
              <th style={{ width: '100px' }}>Дата</th>
              <th style={{ width: '80px' }}>Автор</th>
              <th style={{ flex: 1 }}>Комментарий</th>
            </tr>
          </thead>
          <tbody>
            {versions.map((v) => {
              const isCurrent = v.revision === currentRevision;
              return (
                <tr
                  key={v.revision}
                  style={{
                    backgroundColor: isCurrent ? 'var(--bc-highlight)' : 'transparent',
                    borderLeft: isCurrent ? '3px solid var(--bc-primary)' : '3px solid transparent',
                    paddingLeft: '8px',
                  }}
                >
                  <td className="num" style={{ fontWeight: isCurrent ? 600 : 400 }}>
                    #{v.revision}
                    {isCurrent && <span style={{ marginLeft: '4px', fontSize: '10px' }}>●</span>}
                  </td>
                  <td style={{ whiteSpace: 'nowrap', fontSize: '11px' }}>
                    {new Date(v.timestamp).toLocaleString('ru-RU', {
                      year: '2-digit',
                      month: '2-digit',
                      day: '2-digit',
                      hour: '2-digit',
                      minute: '2-digit',
                    })}
                  </td>
                  <td style={{ fontSize: '11px' }}>{v.author}</td>
                  <td style={{ fontSize: '11px', color: 'var(--bc-text-secondary)' }}>{v.comment}</td>
                </tr>
              );
            })}
          </tbody>
        </table>

        {versions.length === 0 && (
          <div style={{ textAlign: 'center', color: 'var(--bc-text-secondary)', padding: '16px', fontSize: '12px' }}>
            Нет версий
          </div>
        )}
      </div>
    </section>
  );
};

export default DrawingVersionPanel;
