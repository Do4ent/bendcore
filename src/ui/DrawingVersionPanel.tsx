/**
 * История версий чертежа: список всех версий с возможностью восстановления
 * и сравнения (панель справа от основного чертежа).
 */
import React, { useState } from 'react';
import { t } from './core/i18n';
import type { AssociativityStatus, DrawingVersion } from './core/types';
import './panels.css';

export interface DrawingVersionPanelProps {
  tubeId: string;
  currentRevision: number;
  versions: DrawingVersion[];
  status: AssociativityStatus;
  onRestore: (revision: number) => Promise<void>;
  onCompare: (rev1: number, rev2: number) => Promise<void>;
}

const statusDisplay = {
  fresh: { icon: '✓', text: 'Свежий', color: '#4caf50' },
  stale: { icon: '⚠', text: 'Устарелый', color: '#ff9800' },
  broken: { icon: '✗', text: 'Ошибка', color: '#f44336' },
  not_applicable: { icon: '-', text: 'N/A', color: '#9e9e9e' },
};

export const DrawingVersionPanel: React.FC<DrawingVersionPanelProps> = ({
  tubeId,
  currentRevision,
  versions,
  status,
  onRestore,
  onCompare,
}) => {
  const [restoring, setRestoring] = useState<number | null>(null);
  const [comparing, setComparing] = useState<number | null>(null);
  const [error, setError] = useState<string | null>(null);

  const handleRestore = async (revision: number) => {
    try {
      setRestoring(revision);
      setError(null);
      await onRestore(revision);
    } catch (e) {
      setError((e as Error).message);
    } finally {
      setRestoring(null);
    }
  };

  const handleCompare = async (rev: number) => {
    try {
      setComparing(rev);
      setError(null);
      await onCompare(currentRevision, rev);
    } catch (e) {
      setError((e as Error).message);
    } finally {
      setComparing(null);
    }
  };

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

      {error && <div className="bc-banner error" style={{ margin: '8px 0' }}>{error}</div>}

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
              <th style={{ width: '80px' }}></th>
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
                  <td style={{ whiteSpace: 'nowrap' }}>
                    <button
                      className="bc-btn small"
                      onClick={() => handleRestore(v.revision)}
                      disabled={isCurrent || restoring === v.revision}
                      style={{ padding: '2px 6px', fontSize: '11px' }}
                      title={isCurrent ? 'Текущая версия' : 'Восстановить эту версию'}
                    >
                      {restoring === v.revision ? '...' : 'Откат'}
                    </button>
                    {!isCurrent && (
                      <button
                        className="bc-btn small"
                        onClick={() => handleCompare(v.revision)}
                        disabled={comparing === v.revision}
                        style={{ padding: '2px 6px', fontSize: '11px', marginLeft: '4px' }}
                        title="Сравнить с текущей версией"
                      >
                        {comparing === v.revision ? '...' : 'Сравнить'}
                      </button>
                    )}
                  </td>
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
