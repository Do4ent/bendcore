/**
 * Catalog of the interface icons (src/ui/icons/README.md): every icon of the registry by section
 * at 32, 24 and 16 pixels, on light and dark ground, with its name and source (Tabler Icons or
 * drawn for bendcore). Page web/icons (npm run dev:icons) and /icons.html of the shell's server;
 * the Playwright scenario tests/e2e-app/ribbon.spec.ts takes its picture for the documentation.
 */

import React from 'react';
import { ICONS, Icon } from '../icons';
import type { IconEntry } from '../icons';
import type { CadIconSection } from '../icons/cad';
import { TABLER_VERSION } from '../icons/tabler';
import { useI18n } from './i18n';
import './IconCatalog.css';

const SECTIONS: CadIconSection[] = ['general', 'sketch', 'feature', 'assembly', 'drawing'];
const SIZES = [32, 24, 16];

const IconCard: React.FC<{ icon: IconEntry }> = ({ icon }) => {
  const { t } = useI18n();
  return (
    <figure className="ic-card" data-testid={`icon-${icon.name}`} data-source={icon.source}>
      <div className="ic-sizes">
        {SIZES.map((s) => (
          <Icon key={s} name={icon.name} size={s} />
        ))}
      </div>
      <div className="ic-sizes ic-dark">
        {SIZES.map((s) => (
          <Icon key={s} name={icon.name} size={s} />
        ))}
      </div>
      <figcaption>
        <code>{icon.name}</code>
        <span className={`ic-source ic-${icon.source}`}>{icon.tabler ? `${t('icons.source.tabler')} ${icon.tabler}` : t('icons.source.bendcore')}</span>
      </figcaption>
    </figure>
  );
};

const IconCatalog: React.FC = () => {
  const { t } = useI18n();
  const all = Object.values(ICONS);
  const own = all.filter((i) => i.source === 'bendcore').length;
  return (
    <main className="ic-root" data-testid="icon-catalog">
      <h1>{t('icons.title')}</h1>
      <p className="ic-lead">{t('icons.lead', { version: TABLER_VERSION })}</p>
      <p className="ic-lead" data-testid="icon-count" data-total={all.length} data-own={own} data-tabler={all.length - own}>
        {t('icons.count', { total: all.length, own, tabler: all.length - own })}
      </p>
      {SECTIONS.map((section) => (
        <section key={section} data-testid={`icon-section-${section}`}>
          <h2>{t(`icons.section.${section}`)}</h2>
          <div className="ic-grid">
            {all
              .filter((i) => i.section === section)
              .map((i) => (
                <IconCard key={i.name} icon={i} />
              ))}
          </div>
        </section>
      ))}
    </main>
  );
};

export default IconCatalog;
