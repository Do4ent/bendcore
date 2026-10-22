import { useCallback, useRef, useEffect, useState } from 'react';

/**
 * IPC Response structure matching C++ backend
 */
interface IpcResponse<T = any> {
  requestId: string;
  success: boolean;
  result: T;
  errorMessage: string;
  timestamp: number;
}

/**
 * IPC Message structure
 */
interface IpcMessage<T = any> {
  type: string;
  params: T;
  requestId: string;
  timestamp: number;
}

/**
 * Hook for IPC communication with C++ bendcore backend
 * Provides methods for:
 * - Reporting (Release, Export, Delete reports)
 * - Performance Monitoring (metrics, cache stats)
 * - Standards Checking (compliance, DIN/ISO/GOST/ANSI/EN)
 * - Collaboration (comments, versions, changes)
 */
export const useIpc = () => {
  const callbacksRef = useRef<Map<string, (response: IpcResponse) => void>>(new Map());
  const [connected, setConnected] = useState(false);

  // Initialize IPC listener
  useEffect(() => {
    // In a real implementation, this would:
    // 1. Connect to C++ backend via IPC (Electron, Node.js child process, WebSocket, etc.)
    // 2. Listen for responses and route them to callbacks
    // 3. Set connected state when ready

    // For now, we assume a global window.ipc interface (Electron pattern)
    if (typeof window !== 'undefined' && (window as any).ipc) {
      setConnected(true);

      // Listen for IPC responses
      (window as any).ipc.on('bendcore-response', (response: IpcResponse) => {
        const callback = callbacksRef.current.get(response.requestId);
        if (callback) {
          callback(response);
          callbacksRef.current.delete(response.requestId);
        }
      });
    }

    return () => {
      callbacksRef.current.clear();
    };
  }, []);

  /**
   * Core method to send IPC message and get response
   */
  const sendMessage = useCallback(
    async <T = any>(type: string, params: any): Promise<IpcResponse<T>> => {
      return new Promise((resolve, reject) => {
        const requestId = `${type}-${Date.now()}-${Math.random()}`;
        const message: IpcMessage = {
          type,
          params,
          requestId,
          timestamp: Date.now(),
        };

        // Set callback for response
        callbacksRef.current.set(requestId, (response: IpcResponse) => {
          if (response.success) {
            resolve(response);
          } else {
            reject(new Error(response.errorMessage));
          }
        });

        // Send message
        if ((window as any).ipc?.invoke) {
          // Electron IPC pattern
          (window as any).ipc.invoke('bendcore-api', message).catch((err: Error) => {
            callbacksRef.current.delete(requestId);
            reject(err);
          });
        } else {
          // Fallback: simulate response (for testing)
          setTimeout(() => {
            const callback = callbacksRef.current.get(requestId);
            if (callback) {
              callback({
                requestId,
                success: true,
                result: {},
                errorMessage: '',
                timestamp: Date.now(),
              });
            }
          }, 100);
        }

        // Timeout after 30 seconds
        setTimeout(() => {
          if (callbacksRef.current.has(requestId)) {
            callbacksRef.current.delete(requestId);
            reject(new Error('IPC request timeout'));
          }
        }, 30000);
      });
    },
    []
  );

  // ========== Reporting System API ==========

  const releaseForProduction = useCallback(
    async (tubeId: string, approvedBy: string, comments: string): Promise<string> => {
      const response = await sendMessage<string>('ReleaseForProduction', {
        tubeId,
        approvedBy,
        comments,
      });
      return response.result;
    },
    [sendMessage]
  );

  const getProductionReports = useCallback(
    async (filter: string = '') => {
      const response = await sendMessage('GetProductionReports', { filter });
      return response.result;
    },
    [sendMessage]
  );

  const exportReport = useCallback(
    async (
      reportId: string,
      formats: string[] = ['PDF', 'JSON'],
      outputPath: string = ''
    ): Promise<string> => {
      const response = await sendMessage<string>('ExportReport', {
        reportId,
        formats,
        outputPath,
      });
      return response.result;
    },
    [sendMessage]
  );

  const deleteReport = useCallback(
    async (reportId: string): Promise<boolean> => {
      const response = await sendMessage<boolean>('DeleteReport', { reportId });
      return response.result;
    },
    [sendMessage]
  );

  // ========== Performance Monitoring API ==========

  interface PerformanceMetrics {
    analysisCacheHitRate: number;
    geometryCacheHitRate: number;
    standardsCacheHitRate: number;
    averageAnalysisTime: number;
    peakMemoryUsage: number;
    totalTubesProcessed: number;
    cacheHits: number;
    cacheMisses: number;
  }

  const getPerformanceMetrics = useCallback(
    async (): Promise<PerformanceMetrics> => {
      const response = await sendMessage<PerformanceMetrics>('GetPerformanceMetrics', {});
      return response.result;
    },
    [sendMessage]
  );

  const resetMetrics = useCallback(
    async (): Promise<boolean> => {
      const response = await sendMessage<boolean>('ResetMetrics', {});
      return response.result;
    },
    [sendMessage]
  );

  // ========== Standards Checking API ==========

  interface StandardsInfo {
    standardName: string;
    complianceStatus: number;
    complianceScore: number;
  }

  const getStandardsForTube = useCallback(
    async (tubeId: string, od: number, wall: number): Promise<StandardsInfo[]> => {
      const response = await sendMessage<StandardsInfo[]>('GetStandardsForTube', {
        tubeId,
        od,
        wall,
      });
      return response.result;
    },
    [sendMessage]
  );

  const getCurrentStandard = useCallback(
    async (): Promise<string> => {
      const response = await sendMessage<string>('GetCurrentStandard', {});
      return response.result;
    },
    [sendMessage]
  );

  const setCurrentStandard = useCallback(
    async (standardName: string): Promise<boolean> => {
      const response = await sendMessage<boolean>('SetCurrentStandard', {
        standardName,
      });
      return response.result;
    },
    [sendMessage]
  );

  interface StandardDetail {
    name: string;
    description: string;
    minWall: number;
    maxOd: number;
    [key: string]: any;
  }

  const getStandardDetails = useCallback(
    async (standardName: string): Promise<StandardDetail> => {
      const response = await sendMessage<StandardDetail>('GetStandardDetails', {
        standardName,
      });
      return response.result;
    },
    [sendMessage]
  );

  const checkCompliance = useCallback(
    async (
      tubeId: string,
      od: number,
      wall: number,
      standard: string
    ): Promise<StandardsInfo> => {
      const response = await sendMessage<StandardsInfo>('CheckCompliance', {
        tubeId,
        od,
        wall,
        standard,
      });
      return response.result;
    },
    [sendMessage]
  );

  // ========== Collaboration API (Phase 26C) ==========

  interface Comment {
    id: string;
    author: string;
    text: string;
    timestamp: number;
  }

  interface Version {
    id: string;
    timestamp: number;
    author: string;
    changes: string;
  }

  const getComments = useCallback(
    async (tubeId: string): Promise<Comment[]> => {
      const response = await sendMessage<Comment[]>('GetComments', { tubeId });
      return response.result;
    },
    [sendMessage]
  );

  const getVersions = useCallback(
    async (tubeId: string): Promise<Version[]> => {
      const response = await sendMessage<Version[]>('GetVersions', { tubeId });
      return response.result;
    },
    [sendMessage]
  );

  const getChanges = useCallback(
    async (tubeId: string): Promise<string[]> => {
      const response = await sendMessage<string[]>('GetChanges', { tubeId });
      return response.result;
    },
    [sendMessage]
  );

  // ========== System API ==========

  const ping = useCallback(
    async (): Promise<boolean> => {
      try {
        const response = await sendMessage<boolean>('Ping', {});
        return response.success;
      } catch {
        return false;
      }
    },
    [sendMessage]
  );

  return {
    // Connection state
    connected,
    ping,

    // Reporting API
    releaseForProduction,
    getProductionReports,
    exportReport,
    deleteReport,

    // Performance API
    getPerformanceMetrics,
    resetMetrics,

    // Standards API
    getStandardsForTube,
    getCurrentStandard,
    setCurrentStandard,
    getStandardDetails,
    checkCompliance,

    // Collaboration API
    getComments,
    getVersions,
    getChanges,

    // Low-level
    sendMessage,
  };
};
