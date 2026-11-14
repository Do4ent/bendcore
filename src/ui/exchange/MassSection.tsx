/**
 * Mass properties section of the part, imported-model and assembly panels
 * (docs/ui/DATA_EXCHANGE.md): mass, centre of mass, inertia tensor at the centre of mass and the
 * principal moments, all computed by the kernel's analysis module (bendcore_kernel_analysis).
 * The editor passes `load`, which calls its module; the section reloads when `version` changes.
 */

import React, { useEffect, useState } from 'react';
import { useI18n } from '../app/i18n';
import { inertiaDiagonal, inertiaProducts } from './exchange';
import type { MassProperties } from './exchange';
import './Exchange.css';

/** What the module returned: properties, nothing to compute (reason), or an error text. */
export type MassResult =
  | { kind: 'ok'; props: MassProperties; volume?: number; missing?: string[] }
  | { kind: 'empty'; reason: 'noBody' | 'noDensity' | 'none'; volume?: number; missing?: string[] };

export interface MassSectionProps {
  load: () => Promise<MassResult>;
  /** Any value that changes when the body or the densities change. */
  version: unknown;
  /** Part and imported model: the density field of the section (kg/m³). */
  density?: { value: string; onChange: (text: string) => void };
}

const num = (v: number, digits = 4) => (Number.isFinite(v) ? Number(v.toPrecision(digits + 2)).toString() : String(v));
const triple = (v: number[], digits = 4) => v.map((x) => num(x, digits)).join('; ');

const MassSection: React.FC<MassSectionProps> = ({ load, version, density }) => {
  const { t } = useI18n();
  const [result, setResult] = useState<MassResult | null>(null);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    let live = true;
    load().then(
      (r) => {
        if (!live) return;
        setResult(r);
        setError(null);
      },
      (e: unknown) => {
        if (!live) return;
        const ke = e as { code?: string; detail?: string; message?: string };
        setError(ke.code ? `${ke.code}: ${ke.detail ?? ke.message}` : String(ke.message ?? e));
        setResult(null);
      },
    );
    return () => {
      live = false;
    };
    // `load` closes over the editor's document; `version` says when to call it again.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [version, density?.value]);

  const p = result?.kind === 'ok' ? result.props : null;
  const emptyText =
    result?.kind === 'empty'
      ? result.reason === 'noBody'
        ? t('exchange.massNoBody')
        : result.reason === 'noDensity'
          ? t('exchange.massNoDensity')
          : t('exchange.massNone')
      : '—';

  return (
    <section className="ex-mass" data-testid="mass-section">
      <h2>{t('exchange.mass')}</h2>
      <dl>
        {density && (
          <>
            <dt>{t('exchange.massDensity')}</dt>
            <dd>
              <input value={density.value} onChange={(e) => density.onChange(e.target.value)} data-testid="mass-density" />
            </dd>
          </>
        )}
        {result?.volume !== undefined && (
          <>
            <dt>{t('exchange.massVolume')}</dt>
            <dd data-testid="mass-volume" data-value={result.volume}>
              {num(result.volume, 6)}
            </dd>
          </>
        )}
        <dt>{t('exchange.massValue')}</dt>
        <dd data-testid="mass-value" data-value={p?.mass ?? ''}>
          {p ? num(p.mass, 5) : emptyText}
        </dd>
        <dt>{t('exchange.massCenter')}</dt>
        <dd data-testid="mass-center" data-value={p ? p.centerOfMass.join(',') : ''}>
          {p ? triple(p.centerOfMass) : '—'}
        </dd>
        <dt>{t('exchange.massInertia')}</dt>
        <dd data-testid="mass-inertia" data-value={p ? inertiaDiagonal(p).join(',') : ''}>
          {p ? triple(inertiaDiagonal(p)) : '—'}
        </dd>
        <dt>{t('exchange.massProducts')}</dt>
        <dd>{p ? triple(inertiaProducts(p)) : '—'}</dd>
        <dt>{t('exchange.massPrincipal')}</dt>
        <dd>{p ? triple(p.principalMoments) : '—'}</dd>
      </dl>
      {result?.missing && result.missing.length > 0 && (
        <p className="ex-hint" data-testid="mass-missing">
          {t('exchange.massMissing', { list: result.missing.join(', ') })}
        </p>
      )}
      {error && (
        <p className="ex-error" role="alert" data-testid="mass-error">
          {error}
        </p>
      )}
    </section>
  );
};

export default MassSection;
