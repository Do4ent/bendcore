import { useCallback, useState } from 'react';
import { useIpc } from './useIpc';

interface StandardInfo {
  standardName: string;
  complianceStatus: number;
  complianceScore: number;
}

interface StandardDetail {
  name: string;
  description: string;
  minWall: number;
  maxOd: number;
  [key: string]: any;
}

/**
 * Hook for Standards Compliance Checking
 * Communicates with C++ StandardsUIManager via IPC
 *
 * Supports standards: DIN 2391, ISO 286, GOST, ANSI, EN
 *
 * Operations:
 * - getStandardsForTube: Get applicable standards for given tube dimensions
 * - getCurrentStandard: Get currently selected standard
 * - setCurrentStandard: Change active standard for compliance checking
 * - getStandardDetails: Get detailed standard requirements
 * - checkCompliance: Check single tube against standard
 */
export const useStandards = () => {
  const ipc = useIpc();
  const [currentStandard, setCurrentStandardState] = useState<string>('DIN 2391');
  const [standards, setStandards] = useState<StandardInfo[]>([]);
  const [isLoading, setIsLoading] = useState(false);

  const getStandardsForTube = useCallback(
    async (tubeId: string, od: number, wall: number): Promise<StandardInfo[]> => {
      setIsLoading(true);
      try {
        const result = await ipc.getStandardsForTube(tubeId, od, wall);
        const stdArray = Array.isArray(result) ? result : [];
        setStandards(stdArray);
        return stdArray;
      } catch (error) {
        console.error('Failed to get standards for tube:', error);
        return [];
      } finally {
        setIsLoading(false);
      }
    },
    [ipc]
  );

  const getCurrentStandard = useCallback(
    async (): Promise<string> => {
      try {
        const standard = await ipc.getCurrentStandard();
        if (typeof standard === 'string') {
          setCurrentStandardState(standard);
          return standard;
        }
        return currentStandard;
      } catch (error) {
        console.error('Failed to get current standard:', error);
        return currentStandard;
      }
    },
    [ipc, currentStandard]
  );

  const setCurrentStandard = useCallback(
    async (standardName: string): Promise<boolean> => {
      try {
        const success = await ipc.setCurrentStandard(standardName);
        if (success) {
          setCurrentStandardState(standardName);
        }
        return success;
      } catch (error) {
        console.error('Failed to set current standard:', error);
        return false;
      }
    },
    [ipc]
  );

  const getStandardDetails = useCallback(
    async (standardName: string): Promise<StandardDetail | null> => {
      try {
        return await ipc.getStandardDetails(standardName);
      } catch (error) {
        console.error('Failed to get standard details:', error);
        return null;
      }
    },
    [ipc]
  );

  const checkCompliance = useCallback(
    async (
      tubeId: string,
      od: number,
      wall: number,
      standard?: string
    ): Promise<StandardInfo | null> => {
      try {
        const stdToCheck = standard || currentStandard;
        return await ipc.checkCompliance(tubeId, od, wall, stdToCheck);
      } catch (error) {
        console.error('Failed to check compliance:', error);
        return null;
      }
    },
    [ipc, currentStandard]
  );

  /**
   * Check if tube is compliant with current standard
   */
  const isCompliant = useCallback(
    (standardInfo: StandardInfo | null): boolean => {
      if (!standardInfo) return false;
      // status: 1 = compliant, 0 = non-compliant
      return standardInfo.complianceStatus === 1;
    },
    []
  );

  /**
   * Get compliance score as percentage
   */
  const getCompliancePercentage = useCallback(
    (standardInfo: StandardInfo | null): number => {
      if (!standardInfo) return 0;
      return Math.round(standardInfo.complianceScore);
    },
    []
  );

  /**
   * Get compliance status text
   */
  const getComplianceStatus = useCallback(
    (standardInfo: StandardInfo | null): string => {
      if (!standardInfo) return 'Unknown';
      if (standardInfo.complianceStatus === 1) {
        return `Compliant (${Math.round(standardInfo.complianceScore)}%)`;
      } else {
        return `Non-compliant (${Math.round(standardInfo.complianceScore)}%)`;
      }
    },
    []
  );

  /**
   * List available standards
   */
  const availableStandards = useCallback((): string[] => {
    // These are the standards supported by bendcore
    return ['DIN 2391', 'ISO 286', 'GOST', 'ANSI', 'EN'];
  }, []);

  return {
    // State
    currentStandard,
    standards,
    isLoading,

    // Operations
    getStandardsForTube,
    getCurrentStandard,
    setCurrentStandard,
    getStandardDetails,
    checkCompliance,

    // Helpers
    isCompliant,
    getCompliancePercentage,
    getComplianceStatus,
    availableStandards,

    // Connection
    ipcConnected: ipc.connected,
  };
};
