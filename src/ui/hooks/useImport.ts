// useImport Hook (Q3 2027, Task 1, Subtask 9)
// React hook for file import functionality

import { useState, useCallback } from 'react';

export interface ImportHookResult {
  importFile: (file: File, author: string, useCache?: boolean) => Promise<any>;
  importing: boolean;
  progress: number;
  error: string | null;
  formulas: any[];
}

export function useImport(): ImportHookResult {
  const [importing, setImporting] = useState(false);
  const [progress, setProgress] = useState(0);
  const [error, setError] = useState<string | null>(null);
  const [formulas, setFormulas] = useState<any[]>([]);

  const importFile = useCallback(
    async (file: File, author: string, useCache = true): Promise<any> => {
      try {
        setImporting(true);
        setProgress(0);
        setError(null);

        const formData = new FormData();
        formData.append('file', file);
        formData.append('author', author);
        formData.append('useCache', useCache.toString());

        setProgress(25);

        const response = await fetch('/api/bendcore/import-file', {
          method: 'POST',
          body: formData
        });

        setProgress(50);

        if (!response.ok) {
          throw new Error(`Import failed: ${response.statusText}`);
        }

        const result = await response.json();

        setProgress(100);

        if (result.success) {
          setFormulas(result.formulas || []);
          return result;
        } else {
          throw new Error(result.error || 'Import failed');
        }
      } catch (err) {
        const errorMessage = err instanceof Error ? err.message : 'Unknown error';
        setError(errorMessage);
        throw err;
      } finally {
        setImporting(false);
      }
    },
    []
  );

  return {
    importFile,
    importing,
    progress,
    error,
    formulas
  };
}

export default useImport;
