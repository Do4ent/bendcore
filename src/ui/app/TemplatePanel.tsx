/**
 * "New document from a template" of the start page and the "Save as template" dialog of the shell
 * (docs/ui/APP_SHELL.md, "Шаблоны"). Presentation only: the shell owns the templates and the
 * kernel calls (templateService.ts).
 */

import React, { useEffect, useState } from 'react';
import { caption, useI18n } from './i18n';
import type { Translate } from './i18n';
import type { DocKind } from './editorHost';
import type { AnyTemplate } from './templates';

export interface TemplateModelChoice {
  /** Key of the choice: "tab:<id>" or "recent:<id>". */
  key: string;
  kind: 'part' | 'assembly';
  name: string;
  source: 'open' | 'recent';
}

export function templateTitle(t: Translate, template: AnyTemplate): string {
  return template.nameKey ? t(template.nameKey) : template.name;
}

function summary(t: Translate, template: AnyTemplate): string {
  switch (template.kind) {
    case 'part': {
      const s = template.settings;
      return t('template.summary.part', {
        material: s.material ? `${s.material.id} (${s.material.density})` : t('template.noMaterial'),
        variables: s.variables.length,
        planes: s.planes.length,
      });
    }
    case 'assembly': {
      const s = template.settings;
      return t('template.summary.assembly', {
        material: s.material ? `${s.material.name} (${s.material.density})` : t('template.noMaterial'),
        components: s.components.length,
      });
    }
    case 'drawing': {
      const s = template.settings;
      const sheet = t('template.summary.drawing', {
        format: s.format,
        orientation: t(`template.orientation.${s.orientation}`),
        style: caption(t, 'drawing.sheetStyle.', s.style),
        projection: caption(t, 'drawing.projection.', s.projection),
        font: caption(t, 'drawing.textFont.', s.textFont),
      });
      return s.title.organization || s.title.developer
        ? `${sheet}. ${t('template.summary.drawingTitle', { organization: s.title.organization || '—', developer: s.title.developer || '—' })}`
        : sheet;
    }
  }
}

const KIND_KEY = { part: 'kind.part', assembly: 'kind.assembly', drawing: 'kind.drawing' } as const;
const KINDS: DocKind[] = ['part', 'assembly', 'drawing'];

export interface TemplatePanelProps {
  templates: readonly AnyTemplate[];
  models: readonly TemplateModelChoice[];
  /** Whether user templates can be kept (IndexedDB open). */
  canStore: boolean;
  storageReason: string | null;
  busy: string | null;
  onCreate(template: AnyTemplate, model: TemplateModelChoice | null): void;
  onExport(template: AnyTemplate): void;
  onDelete(template: AnyTemplate): void;
  onImport(files: FileList | null): void;
}

export const TemplatePanel: React.FC<TemplatePanelProps> = ({ templates, models, canStore, storageReason, busy, onCreate, onExport, onDelete, onImport }) => {
  const { t } = useI18n();
  const [modelKey, setModelKey] = useState('');
  useEffect(() => {
    if (!models.some((m) => m.key === modelKey)) setModelKey(models[0]?.key ?? '');
  }, [models, modelKey]);
  const model = models.find((m) => m.key === modelKey) ?? null;

  const row = (template: AnyTemplate) => {
    const needsModel = template.kind === 'drawing' && !model;
    return (
      <li key={template.id} className="app-template" data-testid={`template-${template.id}`} data-kind={template.kind}>
        <div className="app-template-text">
          <strong>{templateTitle(t, template)}</strong>
          <span className="app-hint">{template.descriptionKey ? ` ${t(template.descriptionKey)}` : ''}</span>
          <div className="app-hint" data-testid={`template-summary-${template.id}`}>
            {summary(t, template)}
          </div>
        </div>
        <button
          type="button"
          className="app-primary"
          disabled={needsModel || busy !== null}
          onClick={() => onCreate(template, template.kind === 'drawing' ? model : null)}
          data-testid={`template-create-${template.id}`}
        >
          {t('template.create')}
        </button>
        {!template.builtin && (
          <>
            <button type="button" onClick={() => onExport(template)} data-testid={`template-export-${template.id}`}>
              {t('template.export')}
            </button>
            <button type="button" onClick={() => onDelete(template)} data-testid={`template-delete-${template.id}`}>
              {t('template.delete')}
            </button>
          </>
        )}
      </li>
    );
  };

  const group = (builtin: boolean) =>
    KINDS.map((kind) => {
      const list = templates.filter((x) => x.kind === kind && x.builtin === builtin);
      return list.length === 0 ? null : (
        <div key={`${builtin}-${kind}`} className="app-template-kind">
          <h3>{t(KIND_KEY[kind])}</h3>
          <ul>{list.map(row)}</ul>
        </div>
      );
    });

  const userCount = templates.filter((x) => !x.builtin).length;
  return (
    <div className="app-card app-templates" data-testid="templates">
      <h2>{t('template.title')}</h2>
      <p className="app-hint">{t('template.hint')}</p>
      <label className="app-template-model">
        {t('template.drawingModel')}
        <select value={modelKey} onChange={(e) => setModelKey(e.target.value)} disabled={models.length === 0} data-testid="template-model">
          {models.map((m) => (
            <option key={m.key} value={m.key}>
              {t(m.source === 'open' ? 'template.modelOpen' : 'template.modelRecent', { kind: t(KIND_KEY[m.kind]), name: m.name })}
            </option>
          ))}
        </select>
      </label>
      {models.length === 0 && <p className="app-hint" data-testid="template-no-model">{t('template.noModel')}</p>}
      <h3>{t('template.builtinGroup')}</h3>
      {group(true)}
      <h3>{t('template.userGroup')}</h3>
      {userCount === 0 && <p className="app-hint">{canStore ? t('template.userEmpty') : t('template.storageUnavailable', { reason: storageReason ?? '' })}</p>}
      {group(false)}
      <label className="app-file">
        {t('template.import')}
        <input
          type="file"
          accept=".json,application/json"
          data-testid="template-import"
          disabled={!canStore}
          onChange={(e) => {
            onImport(e.target.files);
            e.target.value = '';
          }}
        />
      </label>
      <p className="app-hint">{t('template.importHint')}</p>
    </div>
  );
};

export interface SaveTemplateDialogProps {
  documentName: string;
  initialName: string;
  busy: boolean;
  onSave(name: string): void;
  onCancel(): void;
}

export const SaveTemplateDialog: React.FC<SaveTemplateDialogProps> = ({ documentName, initialName, busy, onSave, onCancel }) => {
  const { t } = useI18n();
  const [name, setName] = useState(initialName);
  const valid = name.trim() !== '';
  return (
    <div className="app-dialog-backdrop" role="presentation">
      <form
        className="app-dialog"
        role="dialog"
        aria-label={t('template.saveTitle')}
        data-testid="save-template-dialog"
        onSubmit={(e) => {
          e.preventDefault();
          if (valid && !busy) onSave(name.trim());
        }}
      >
        <h2>{t('template.saveTitle')}</h2>
        <p className="app-hint">{t('template.saveHint', { name: documentName })}</p>
        <label>
          {t('template.name')}
          <input value={name} autoFocus onChange={(e) => setName(e.target.value)} data-testid="template-name" />
        </label>
        <div className="app-dialog-actions">
          <button type="submit" className="app-primary" disabled={!valid || busy} data-testid="template-save">
            {t('template.save')}
          </button>
          <button type="button" onClick={onCancel} data-testid="template-cancel">
            {t('template.cancel')}
          </button>
        </div>
      </form>
    </div>
  );
};
