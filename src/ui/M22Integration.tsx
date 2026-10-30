import React, { useState } from 'react';
import './M22Styles.css';

// M22 Component Imports - File Operations & Settings
import FilePickerDialog from './FilePickerDialog';
import SaveDialog from './SaveDialog';
import RecentFilesManager from './RecentFilesManager';
import FileExplorer from './FileExplorer';
import SettingsDialog from './SettingsDialog';
import ThemeSelector from './ThemeSelector';
import IconThemeManager from './IconThemeManager';
import KeyboardShortcutEditor from './KeyboardShortcutEditor';
import UnitPreferences from './UnitPreferences';
import PerformanceSettings from './PerformanceSettings';
import PluginSettings from './PluginSettings';
import CloudSettings from './CloudSettings';

/**
 * M22Integration Component
 * Manages all M22 dialogs and their state
 * Provides centralized control for File Operations & Settings (GAP 079-090)
 */

export interface M22IntegrationProps {
  onCommandExecuted?: (command: string, data?: any) => void;
}

const M22Integration: React.FC<M22IntegrationProps> = ({ onCommandExecuted }) => {
  // Dialog state management
  const [openDialogs, setOpenDialogs] = useState<Set<string>>(new Set());

  const dialogConfigs = {
    'file-open': { component: FilePickerDialog, label: 'Open File' },
    'file-save': { component: SaveDialog, label: 'Save File' },
    'recent-files': { component: RecentFilesManager, label: 'Recent Files' },
    'file-explorer': { component: FileExplorer, label: 'File Explorer' },
    'settings': { component: SettingsDialog, label: 'Settings' },
    'theme-selector': { component: ThemeSelector, label: 'Theme Selector' },
    'icon-theme': { component: IconThemeManager, label: 'Icon Theme' },
    'shortcuts': { component: KeyboardShortcutEditor, label: 'Keyboard Shortcuts' },
    'units': { component: UnitPreferences, label: 'Unit Preferences' },
    'performance': { component: PerformanceSettings, label: 'Performance' },
    'plugins': { component: PluginSettings, label: 'Plugins' },
    'cloud': { component: CloudSettings, label: 'Cloud Settings' }
  };

  /**
   * Open a dialog
   */
  const openDialog = (dialogId: string) => {
    setOpenDialogs(prev => new Set([...prev, dialogId]));
    if (onCommandExecuted) onCommandExecuted('dialog-open', { dialogId });
  };

  /**
   * Close a dialog
   */
  const closeDialog = (dialogId: string) => {
    setOpenDialogs(prev => {
      const next = new Set(prev);
      next.delete(dialogId);
      return next;
    });
    if (onCommandExecuted) onCommandExecuted('dialog-close', { dialogId });
  };

  /**
   * Handle dialog actions (save, select, etc.)
   */
  const handleDialogAction = (dialogId: string, action: string, data?: any) => {
    if (onCommandExecuted) {
      onCommandExecuted(`${dialogId}:${action}`, data);
    }
    closeDialog(dialogId);
  };

  return (
    <div className="m22-integration">
      {/* File Open Dialog */}
      <FilePickerDialog
        isOpen={openDialogs.has('file-open')}
        onClose={() => closeDialog('file-open')}
        onSelect={(path) => handleDialogAction('file-open', 'select', { path })}
      />

      {/* File Save Dialog */}
      <SaveDialog
        isOpen={openDialogs.has('file-save')}
        onClose={() => closeDialog('file-save')}
        onSave={(path, format) => handleDialogAction('file-save', 'save', { path, format })}
      />

      {/* Recent Files Manager */}
      {openDialogs.has('recent-files') && (
        <RecentFilesManager
          isOpen={true}
          onClose={() => closeDialog('recent-files')}
          onSelect={(path) => handleDialogAction('recent-files', 'select', { path })}
        />
      )}

      {/* File Explorer */}
      {openDialogs.has('file-explorer') && (
        <FileExplorer
          isOpen={true}
          onClose={() => closeDialog('file-explorer')}
          onSelect={(path) => handleDialogAction('file-explorer', 'select', { path })}
        />
      )}

      {/* Settings Dialog */}
      <SettingsDialog
        isOpen={openDialogs.has('settings')}
        onClose={() => closeDialog('settings')}
        onSave={(settings) => handleDialogAction('settings', 'save', settings)}
      />

      {/* Theme Selector */}
      {openDialogs.has('theme-selector') && (
        <ThemeSelector
          isOpen={true}
          onClose={() => closeDialog('theme-selector')}
          onThemeChange={(theme, colors) => handleDialogAction('theme-selector', 'change', { theme, colors })}
        />
      )}

      {/* Icon Theme Manager */}
      {openDialogs.has('icon-theme') && (
        <IconThemeManager
          isOpen={true}
          onClose={() => closeDialog('icon-theme')}
          onSave={(theme) => handleDialogAction('icon-theme', 'save', theme)}
        />
      )}

      {/* Keyboard Shortcut Editor */}
      {openDialogs.has('shortcuts') && (
        <KeyboardShortcutEditor
          isOpen={true}
          onClose={() => closeDialog('shortcuts')}
          onSave={(profile) => handleDialogAction('shortcuts', 'save', profile)}
        />
      )}

      {/* Unit Preferences */}
      {openDialogs.has('units') && (
        <UnitPreferences
          isOpen={true}
          onClose={() => closeDialog('units')}
          onSave={(units) => handleDialogAction('units', 'save', units)}
        />
      )}

      {/* Performance Settings */}
      {openDialogs.has('performance') && (
        <PerformanceSettings
          isOpen={true}
          onClose={() => closeDialog('performance')}
          onSave={(settings) => handleDialogAction('performance', 'save', settings)}
        />
      )}

      {/* Plugin Settings */}
      {openDialogs.has('plugins') && (
        <PluginSettings
          isOpen={true}
          onClose={() => closeDialog('plugins')}
          onSave={(plugins) => handleDialogAction('plugins', 'save', plugins)}
        />
      )}

      {/* Cloud Settings */}
      {openDialogs.has('cloud') && (
        <CloudSettings
          isOpen={true}
          onClose={() => closeDialog('cloud')}
          onSave={(settings) => handleDialogAction('cloud', 'save', settings)}
        />
      )}

      {/* Public API for parent component */}
      {React.useMemo(() => {
        (window as any).__bendcore_m22 = {
          openDialog,
          closeDialog,
          isDialogOpen: (id: string) => openDialogs.has(id),
          dialogConfigs
        };
        return null;
      }, [openDialogs])}
    </div>
  );
};

// Export helper hook for opening dialogs
export const useM22Dialogs = () => {
  const m22 = (window as any).__bendcore_m22;
  if (!m22) {
    console.warn('M22Integration not mounted');
    return { openDialog: () => {} };
  }
  return {
    openDialog: m22.openDialog,
    closeDialog: m22.closeDialog,
    isDialogOpen: m22.isDialogOpen,
    dialogConfigs: m22.dialogConfigs
  };
};

export default M22Integration;
