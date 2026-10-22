import { useCallback } from 'react';
import { useIpc } from './useIpc';

interface ReleaseRequest {
  tubeId: string;
  approvedBy: string;
  comments: string;
}

interface ReportSummary {
  reportId: string;
  status: 'draft' | 'approved' | 'released';
  timestamp: number;
  approvedBy: string;
  bendCardCount: number;
}

interface ExportRequest {
  reportId: string;
  formats: string[];
  outputPath: string;
}

/**
 * Hook for Production Reporting operations
 * Communicates with C++ ReportingUIManager via IPC
 *
 * Uses useIpc for backend communication:
 * - ReleaseForProduction: Release tube for manufacturing
 * - GetProductionReports: Fetch report history with filtering
 * - ExportReport: Export report in specified formats
 * - DeleteReport: Remove report from history
 */
export const useReporting = () => {
  const ipc = useIpc();

  const releaseForProduction = useCallback(
    async (request: ReleaseRequest): Promise<string> => {
      try {
        return await ipc.releaseForProduction(
          request.tubeId,
          request.approvedBy,
          request.comments
        );
      } catch (error) {
        console.error('Release failed:', error);
        throw error;
      }
    },
    [ipc]
  );

  const getProductionReports = useCallback(
    async (filter?: string): Promise<ReportSummary[]> => {
      try {
        const reports = await ipc.getProductionReports(filter || '');
        return Array.isArray(reports) ? reports : [];
      } catch (error) {
        console.error('Failed to fetch reports:', error);
        // Return empty array on error
        return [];
      }
    },
    [ipc]
  );

  const exportReport = useCallback(
    async (request: ExportRequest): Promise<string> => {
      try {
        return await ipc.exportReport(
          request.reportId,
          request.formats,
          request.outputPath
        );
      } catch (error) {
        console.error('Export failed:', error);
        throw error;
      }
    },
    [ipc]
  );

  const deleteReport = useCallback(
    async (reportId: string): Promise<boolean> => {
      try {
        return await ipc.deleteReport(reportId);
      } catch (error) {
        console.error('Delete failed:', error);
        throw error;
      }
    },
    [ipc]
  );

  return {
    releaseForProduction,
    getProductionReports,
    exportReport,
    deleteReport,
    ipcConnected: ipc.connected,
  };
};
