/**
 * Элементы среды эскиза пути (1.2): угол подтверждения («Завершить эскиз» / «Отменить», как
 * в SOLIDWORKS) и триада блокировки оси — клавиши X / Y / Z, повторное нажатие снимает блокировку.
 */
import React, { useEffect } from 'react';
import { toggleAxis, type Axis } from './core/axisLock';
import { t } from './core/i18n';
import './panels.css';

export interface SketchConfirmCornerProps {
  onFinish: () => void;
  onCancel: () => void;
}

export const SketchConfirmCorner: React.FC<SketchConfirmCornerProps> = ({ onFinish, onCancel }) => (
  <div className="bc-confirm-corner">
    <button className="ok" onClick={onFinish} title={t('sketch.finish')} aria-label={t('sketch.finish')}>
      ✓
    </button>
    <button className="cancel" onClick={onCancel} title={t('sketch.cancel')} aria-label={t('sketch.cancel')}>
      ✕
    </button>
  </div>
);

const AXIS_COLOR: Record<Axis, string> = { x: '#d93a3a', y: '#2e9e5b', z: '#1f6fc5' };

export interface AxisTriadProps {
  locked: Axis | null;
  onChange: (axis: Axis | null) => void;
  /** Слушать клавиши X / Y / Z на окне (выключить, когда фокус в поле ввода у курсора). */
  listenKeys?: boolean;
}

export const AxisTriad: React.FC<AxisTriadProps> = ({ locked, onChange, listenKeys = true }) => {
  useEffect(() => {
    if (!listenKeys) return;
    const onKey = (e: KeyboardEvent) => {
      const el = e.target as HTMLElement | null;
      if (el && (el.tagName === 'INPUT' || el.tagName === 'TEXTAREA' || el.isContentEditable)) return;
      if (e.ctrlKey || e.metaKey || e.altKey) return;
      const next = toggleAxis(locked, e.key);
      if (next !== locked) onChange(next);
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [locked, onChange, listenKeys]);

  return (
    <div className="bc-triad" role="group" aria-label={t('sketch.axisLock')} title={t('sketch.axisLock')}>
      {(['x', 'y', 'z'] as Axis[]).map((a) => (
        <button
          key={a}
          aria-pressed={locked === a}
          style={{ color: locked === a ? '#fff' : AXIS_COLOR[a], background: locked === a ? AXIS_COLOR[a] : undefined }}
          onClick={() => onChange(locked === a ? null : a)}
        >
          {a.toUpperCase()}
        </button>
      ))}
    </div>
  );
};
