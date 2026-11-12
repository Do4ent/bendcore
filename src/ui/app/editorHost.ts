/**
 * What the application shell (docs/ui/APP_SHELL.md) gives an editor and gets back from it.
 * The standalone pages (web/part-modeler, web/assembly, web/drawing) render the editors without
 * these props: then an editor is always active, starts with a new document and keeps everything
 * to itself.
 */

export type DocKind = 'part' | 'assembly' | 'drawing';

/** A document as text, the way it is saved, kept as a draft and passed between editors. */
export interface DocumentFile {
  kind: DocKind;
  /** File name shown in the tab and used for downloads. */
  name: string;
  /**
   * Document JSON: bendcore-part, bendcore-assembly, or the drawing file bendcore-drawing-file
   * with the drawing and its model (src/ui/drawing/drawingFile.ts).
   */
  text: string;
  /**
   * Drafts and recent files written by the earlier shell only: a drawing kept as the drawing
   * document in `text` and its model here. migrateDrawingDocument() packs them into one file.
   */
  model?: string;
  /**
   * Assembly only: part JSON of the definitions inserted from bendcore parts. The assembly file
   * keeps BRep only; this list (kept in drafts, not in the file) lets "open component part" find
   * the part document of a component.
   */
  parts?: PartSource[];
}

export interface PartSource {
  /** Definition id in the assembly; -1 for a library entry not inserted yet. */
  definition: number;
  name: string;
  text: string;
}

export interface EditorHostProps {
  /** Shown tab: only the active editor handles keyboard shortcuts (default true). */
  active?: boolean;
  /** Document opened at start instead of a new one. */
  initial?: DocumentFile | null;
  /** True when `initial` is a restored draft: the document starts as modified. */
  initialDirty?: boolean;
  /**
   * After every completed change outside an open transaction: the document as it is now
   * (draft for autosave) and whether it differs from the last saved or opened file.
   */
  onChange?: (snapshot: DocumentFile, dirty: boolean) => void;
  /** A file was saved (downloaded) or opened in the editor: for the recent-files list. */
  onFile?: (file: DocumentFile) => void;
  /** Part editor: "create drawing" of the current part. */
  onCreateDrawing?: (part: DocumentFile) => void;
  /** Assembly editor: "open component part" of a component defined by a bendcore part. */
  onOpenPart?: (part: DocumentFile) => void;
}

export function downloadText(name: string, text: string | Uint8Array, type: string): void {
  const blob = new Blob([text as BlobPart], { type });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = name;
  document.body.appendChild(a);
  a.click();
  a.remove();
  URL.revokeObjectURL(url);
}
