// "Document properties" panel of the part page (docs/ui/PART_MODELING_UI.md, "Свойства документа"):
// Russian and English captions, field values from the document, the configuration line when the
// active configuration shows another designation.
import React from 'react';
import { renderToStaticMarkup } from 'react-dom/server';

import { I18nContext, translate } from '../../app/i18n';
import type { Lang } from '../../app/i18n';
import { DocumentPropertiesPanel } from '../DocumentPropertiesPanel';

const props = {
  designation: 'АБВГ.715311.001',
  name: 'Плита опорная',
  material: 'al_6061_t6',
  density: null,
  developer: 'Иванов',
  organization: 'ООО «Ромашка»',
  note: 'Анодировать',
  effective: { designation: 'АБВГ.715311.001-01', name: 'Плита длинная', material: 'al_6061_t6', density: 2700 },
};

function render(lang: Lang, p: typeof props | null = props): string {
  return renderToStaticMarkup(
    <I18nContext.Provider value={{ lang, t: (key, params) => translate(lang, key, params), setLang: () => undefined }}>
      <DocumentPropertiesPanel properties={p} disabled={false} onChange={() => undefined} />
    </I18nContext.Provider>,
  );
}

describe('DocumentPropertiesPanel', () => {
  test('Russian captions and values', () => {
    const html = render('ru');
    for (const s of ['Свойства документа', 'Обозначение', 'Наименование', 'Разработчик', 'Организация', 'Примечание']) expect(html).toContain(s);
    expect(html).toContain('value="АБВГ.715311.001"');
    expect(html).toContain('ООО «Ромашка»');
    expect(html).toContain('В исполнении: АБВГ.715311.001-01 — Плита длинная');
  });

  test('English captions', () => {
    const html = render('en');
    for (const s of ['Document properties', 'Designation', 'Name', 'Developer', 'Organization', 'Note']) expect(html).toContain(s);
    expect(html).toContain('In the configuration: АБВГ.715311.001-01 — Плита длинная');
  });

  test('no configuration line when the effective values are the document ones; disabled without a document', () => {
    const same = { ...props, effective: { ...props.effective, designation: props.designation, name: props.name } };
    expect(render('ru', same)).not.toContain('В исполнении');
    expect(render('ru', null)).toContain('disabled=""');
  });
});
