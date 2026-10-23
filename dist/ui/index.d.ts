/**
 * BendCore UI Entry Point
 * React root component for CommandManager Ribbon UI
 */
import './styles.css';
export { default as TubeDialog } from './TubeDialog';
export type { TubeConfig } from './TubeDialog';
declare global {
    interface Window {
        electron?: {
            ipc: {
                invoke: (channel: string, data: any) => Promise<any>;
                send: (channel: string, data: any) => void;
                on: (channel: string, callback: (data: any) => void) => void;
            };
        };
    }
}
//# sourceMappingURL=index.d.ts.map