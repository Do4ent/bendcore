/**
 * "Document properties" of the part page (docs/ui/PART_MODELING_UI.md, "Свойства документа"):
 * designation, name, developer, organization and note of the part document. A field is written
 * to the document when it loses focus (one undo entry, the file changes); the material stays in
 * the "Properties" block. With an active configuration the designation and name the assembly,
 * the specification and the drawing see are shown under the fields.
 */
import React, { useEffect, useState } from 'react';

import type { PartDocumentProperties } from '../../wasm/bendcore_modeling_api.mjs';
import { useI18n } from '../app/i18n';
import type { MessageKey } from '../app/i18n';

export type DocumentPropertyField = 'designation' | 'name' | 'developer' | 'organization' | 'note';

const FIELDS: DocumentPropertyField[] = ['designation', 'name', 'developer', 'organization', 'note'];

interface Props {
  properties: PartDocumentProperties | null;
  disabled: boolean;
  onChange(field: DocumentPropertyField, value: string): void;
}

const LABELS: Record<DocumentPropertyField, MessageKey> = {
  designation: 'part.docProps.designation',
  name: 'part.docProps.name',
  developer: 'part.docProps.developer',
  organization: 'part.docProps.organization',
  note: 'part.docProps.note',
};

export function DocumentPropertiesPanel({ properties, disabled, onChange }: Props): React.ReactElement {
  const { t } = useI18n();
  const [values, setValues] = useState<Record<DocumentPropertyField, string>>(() => valuesOf(properties));
  // The document is the source: undo, opening a file or another configuration replace the fields.
  useEffect(() => setValues(valuesOf(properties)), [properties]);
  const effective = properties?.effective;
  const differs =
    !!effective && !!properties && (effective.designation !== properties.designation || effective.name !== properties.name);
  return (
    <section data-testid="document-properties">
      <h2>{t('part.docProps')}</h2>
      <dl>
        {FIELDS.map((f) => (
          <React.Fragment key={f}>
            <dt>
              <label htmlFor={`doc-prop-${f}`}>{t(LABELS[f])}</label>
            </dt>
            <dd>
              {f === 'note' ? (
                <textarea
                  id={`doc-prop-${f}`}
                  data-testid={`doc-prop-${f}`}
                  rows={2}
                  value={values[f]}
                  disabled={disabled || !properties}
                  onChange={(e) => setValues({ ...values, [f]: e.target.value })}
                  onBlur={() => {
                    if (properties && values[f] !== properties[f]) onChange(f, values[f]);
                  }}
                />
              ) : (
                <input
                  id={`doc-prop-${f}`}
                  data-testid={`doc-prop-${f}`}
                  value={values[f]}
                  disabled={disabled || !properties}
                  onChange={(e) => setValues({ ...values, [f]: e.target.value })}
                  onKeyDown={(e) => {
                    if (e.key === 'Enter') (e.target as HTMLInputElement).blur();
                  }}
                  onBlur={() => {
                    if (properties && values[f] !== properties[f]) onChange(f, values[f]);
                  }}
                />
              )}
            </dd>
          </React.Fragment>
        ))}
      </dl>
      {differs && effective && (
        <p className="pm-hint" data-testid="doc-prop-effective">
          {t('part.docProps.effective', { designation: effective.designation || '—', name: effective.name || '—' })}
        </p>
      )}
      <p className="pm-hint">{t('part.docProps.hint')}</p>
    </section>
  );
}

function valuesOf(p: PartDocumentProperties | null): Record<DocumentPropertyField, string> {
  return {
    designation: p?.designation ?? '',
    name: p?.name ?? '',
    developer: p?.developer ?? '',
    organization: p?.organization ?? '',
    note: p?.note ?? '',
  };
}
