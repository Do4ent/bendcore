// Selection state of a viewport page (docs/ui/KERNEL_VIEWPORT.md, "Фильтры выбора и выбор рамкой"):
// the filter, the box switch and the picked elements of the current element mode. Clicks and boxes
// combine with the selection by their operation (replace, Shift add, Ctrl remove); the total area of
// the selected faces or the length of the selected edges is asked from the kernel by the page.

import { useCallback, useEffect, useRef, useState } from 'react';
import { combineSelection, defaultFilter } from './selection';
import type { BoxResult, ElementMode, SelectOp, SelectionFilter } from './selection';

export type ElementKey = 'faces' | 'edges' | 'vertices' | 'bodies';
export type Selected = Record<ElementKey, number[]>;

export const EMPTY_SELECTION: Selected = { faces: [], edges: [], vertices: [], bodies: [] };

export const KEY_OF_MODE: Record<ElementMode, ElementKey> = { face: 'faces', edge: 'edges', vertex: 'vertices', body: 'bodies' };

export interface ElementSelectionOptions {
  /** Kernel total of the selected faces (area, mm²) or edges (length, mm). */
  total?(kind: 'face' | 'edge', indices: readonly number[]): Promise<number>;
  /** Changes when the model changes: the indices refer to the old model. */
  modelKey?: unknown;
}

export interface ElementSelection {
  filter: SelectionFilter;
  setFilter(filter: SelectionFilter): void;
  boxSelect: boolean;
  setBoxSelect(on: boolean): void;
  selected: Selected;
  /** Elements of the current mode. */
  current: number[];
  /** A click: `index` null is a click on nothing (clears unless the selection is being edited). */
  pick(mode: ElementMode, index: number | null, op?: SelectOp): void;
  box(result: BoxResult, op: SelectOp): void;
  clear(): void;
  replace(mode: ElementMode, indices: number[]): void;
  total: { kind: 'area' | 'length'; value: number } | null;
}

export function useElementSelection(options: ElementSelectionOptions = {}): ElementSelection {
  const [filter, setFilterState] = useState<SelectionFilter>(defaultFilter());
  const [boxSelect, setBoxSelect] = useState(false);
  const [selected, setSelected] = useState<Selected>(EMPTY_SELECTION);
  const [total, setTotal] = useState<{ kind: 'area' | 'length'; value: number } | null>(null);
  const optionsRef = useRef(options);
  optionsRef.current = options;

  useEffect(() => {
    setSelected(EMPTY_SELECTION);
  }, [options.modelKey]);

  const filterRef = useRef(filter);
  const setFilter = useCallback((next: SelectionFilter) => {
    // Another element mode starts a new selection.
    if (filterRef.current.mode !== next.mode) setSelected(EMPTY_SELECTION);
    filterRef.current = next;
    setFilterState(next);
  }, []);

  const update = useCallback((key: ElementKey, found: readonly number[], op: SelectOp) => {
    setSelected((prev) => ({ ...EMPTY_SELECTION, [key]: combineSelection(prev[key], found, op) }));
  }, []);

  const pick = useCallback(
    (mode: ElementMode, index: number | null, op: SelectOp = 'replace') => {
      const key = KEY_OF_MODE[mode];
      if (index === null) {
        // A click on nothing clears a plain selection; with Shift or Ctrl it keeps it.
        if (op === 'replace') setSelected(EMPTY_SELECTION);
        return;
      }
      update(key, [index], op);
    },
    [update],
  );

  const box = useCallback(
    (result: BoxResult, op: SelectOp) => {
      const key = KEY_OF_MODE[filter.mode];
      update(key, result[key], op);
    },
    [filter.mode, update],
  );

  const clear = useCallback(() => setSelected(EMPTY_SELECTION), []);
  const replace = useCallback((mode: ElementMode, indices: number[]) => {
    setSelected({ ...EMPTY_SELECTION, [KEY_OF_MODE[mode]]: indices });
  }, []);

  const current = selected[KEY_OF_MODE[filter.mode]];
  const kind = filter.mode === 'face' ? 'face' : filter.mode === 'edge' ? 'edge' : null;
  const key = current.join(',');
  useEffect(() => {
    setTotal(null);
    const run = optionsRef.current.total;
    if (!kind || !run || current.length === 0) return undefined;
    let stale = false;
    run(kind, current).then(
      (value) => {
        if (!stale) setTotal({ kind: kind === 'face' ? 'area' : 'length', value });
      },
      () => {
        if (!stale) setTotal(null);
      },
    );
    return () => {
      stale = true;
    };
    // `current` is stood for by key.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [kind, key, options.modelKey]);

  return { filter, setFilter, boxSelect, setBoxSelect, selected, current, pick, box, clear, replace, total };
}
