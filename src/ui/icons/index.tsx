// Icon registry of the bendcore interface: name → SVG (src/ui/icons/README.md). General actions
// come from Tabler Icons (MIT, src/ui/icons/tabler.ts, generated from the @tabler/icons package);
// CAD commands are drawn for bendcore in the same style (src/ui/icons/cad.ts, MIT of the project).

import React from 'react';
import { CAD_ICONS, type CadIconSection } from './cad';
import { TABLER_ICONS } from './tabler';

export type IconSource = 'tabler' | 'bendcore';

export interface IconEntry {
  name: string;
  source: IconSource;
  section: CadIconSection;
  /** Tabler name of a general icon. */
  tabler?: string;
  /** Inner SVG markup (24×24 grid). */
  body: string;
}

/** General actions of every editor: application name → Tabler Icons name. */
export const GENERAL_ICONS: Record<string, string> = {
  new: 'file-plus',
  open: 'folder-open',
  save: 'device-floppy',
  undo: 'arrow-back-up',
  redo: 'arrow-forward-up',
  export: 'file-export',
  import: 'file-import',
  print: 'printer',
  measure: 'ruler-measure',
  search: 'search',
  settings: 'settings',
  select: 'pointer',
  fit: 'zoom-scan',
  delete: 'trash',
  'collapse': 'chevron-up',
  'expand': 'chevron-down',
  close: 'x',
  rebuild: 'refresh',
  show: 'eye',
  list: 'list-details',
};

function buildRegistry(): Record<string, IconEntry> {
  const out: Record<string, IconEntry> = {};
  for (const [name, tabler] of Object.entries(GENERAL_ICONS)) {
    const body = TABLER_ICONS[tabler];
    if (!body) throw new Error(`Tabler icon '${tabler}' is not in src/ui/icons/tabler.ts`);
    out[name] = { name, source: 'tabler', section: 'general', tabler, body };
  }
  for (const [name, icon] of Object.entries(CAD_ICONS)) {
    if (out[name]) throw new Error(`icon '${name}' is defined twice`);
    out[name] = { name, source: 'bendcore', section: icon.section, body: icon.body };
  }
  return out;
}

/** Every icon by name. */
export const ICONS: Readonly<Record<string, IconEntry>> = buildRegistry();

/** Icon shown for a command without an icon of its own (a feature the kernel adds later). */
export const FALLBACK_ICON = 'feature';

export function hasIcon(name: string): boolean {
  return Object.prototype.hasOwnProperty.call(ICONS, name);
}

/** Entry of an icon; an unknown name gives the general feature icon. */
export function iconEntry(name: string): IconEntry {
  return hasIcon(name) ? ICONS[name] : ICONS[FALLBACK_ICON];
}

/** Standalone SVG text of an icon (catalog page, files). */
export function iconSvg(name: string, size = 24): string {
  return (
    `<svg xmlns="http://www.w3.org/2000/svg" width="${size}" height="${size}" viewBox="0 0 24 24" fill="none" ` +
    `stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">${iconEntry(name).body}</svg>`
  );
}

export interface IconProps {
  name: string;
  size?: number;
  className?: string;
}

/** Inline SVG icon; decorative (the command's name is on the button). */
export const Icon: React.FC<IconProps> = ({ name, size = 24, className }) => {
  const entry = iconEntry(name);
  return (
    <svg
      xmlns="http://www.w3.org/2000/svg"
      width={size}
      height={size}
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      strokeWidth={2}
      strokeLinecap="round"
      strokeLinejoin="round"
      className={className ? `bc-icon ${className}` : 'bc-icon'}
      aria-hidden="true"
      focusable="false"
      data-icon={entry.name}
      dangerouslySetInnerHTML={{ __html: entry.body }}
    />
  );
};
