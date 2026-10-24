/**
 * BendCore UI Component: RibbonBar
 * Top toolbar with tabs and commands (Microsoft Office Ribbon style)
 *
 * @component
 * @example
 * const [activeTab, setActiveTab] = useState('file');
 * <RibbonBar
 *   activeTab={activeTab}
 *   onTabChange={setActiveTab}
 *   onCommand={handleCommand}
 *   isDarkMode={false}
 * />
 */

import React, { useState, useCallback, useEffect } from 'react';
import './RibbonBar.css';

// ============ Types ============

export interface RibbonCommand {
  id: string;
  label: string;
  icon: string;
  tooltip: string;
  shortcut?: string;
  action: () => void;
  submenu?: RibbonCommand[];
  disabled?: boolean;
  variant?: 'primary' | 'secondary' | 'danger';
}

export interface RibbonTab {
  id: string;
  label: string;
  commands: RibbonCommand[];
}

export interface RibbonBarProps {
  activeTab?: string;
  onTabChange?: (tab: string) => void;
  onCommand?: (command: string, args?: any) => void;
  isDarkMode?: boolean;
  tabs?: RibbonTab[];
}

// ============ Default Commands ============

const DEFAULT_TABS: RibbonTab[] = [
  {
    id: 'file',
    label: 'Файл',
    commands: [
      {
        id: 'new',
        label: 'Создать',
        icon: '📄',
        tooltip: 'Создать новый проект',
        shortcut: 'Ctrl+N',
        action: () => {},
      },
      {
        id: 'open',
        label: 'Открыть',
        icon: '📁',
        tooltip: 'Открыть существующий проект',
        shortcut: 'Ctrl+O',
        action: () => {},
      },
      {
        id: 'save',
        label: 'Сохранить',
        icon: '💾',
        tooltip: 'Сохранить проект',
        shortcut: 'Ctrl+S',
        action: () => {},
      },
      {
        id: 'save_as',
        label: 'Сохранить как...',
        icon: '💾',
        tooltip: 'Сохранить проект с новым именем',
        shortcut: 'Ctrl+Shift+S',
        action: () => {},
      },
    ],
  },
  {
    id: 'edit',
    label: 'Правка',
    commands: [
      {
        id: 'undo',
        label: 'Отменить',
        icon: '↶',
        tooltip: 'Отменить последнее действие',
        shortcut: 'Ctrl+Z',
        action: () => {},
      },
      {
        id: 'redo',
        label: 'Повторить',
        icon: '↷',
        tooltip: 'Повторить отменённое действие',
        shortcut: 'Ctrl+Y',
        action: () => {},
      },
      {
        id: 'cut',
        label: 'Вырезать',
        icon: '✂️',
        tooltip: 'Вырезать выделенное',
        shortcut: 'Ctrl+X',
        action: () => {},
      },
      {
        id: 'copy',
        label: 'Копировать',
        icon: '📋',
        tooltip: 'Копировать выделенное',
        shortcut: 'Ctrl+C',
        action: () => {},
      },
      {
        id: 'paste',
        label: 'Вставить',
        icon: '📌',
        tooltip: 'Вставить из буфера обмена',
        shortcut: 'Ctrl+V',
        action: () => {},
      },
    ],
  },
  {
    id: 'view',
    label: 'Вид',
    commands: [
      {
        id: 'fit_all',
        label: 'По размеру',
        icon: '🔍',
        tooltip: 'Вписать всё в окно',
        shortcut: 'Home',
        action: () => {},
      },
      {
        id: 'zoom_in',
        label: 'Приблизить',
        icon: '🔍+',
        tooltip: 'Приблизить вид',
        shortcut: 'Ctrl++',
        action: () => {},
      },
      {
        id: 'zoom_out',
        label: 'Удалить',
        icon: '🔍-',
        tooltip: 'Удалить вид',
        shortcut: 'Ctrl+-',
        action: () => {},
      },
      {
        id: 'wireframe',
        label: 'Каркас',
        icon: '▦',
        tooltip: 'Каркасный вид',
        action: () => {},
      },
      {
        id: 'shaded',
        label: 'Затенённый',
        icon: '◆',
        tooltip: 'Затенённый вид',
        action: () => {},
      },
    ],
  },
  {
    id: 'insert',
    label: 'Вставка',
    commands: [
      {
        id: 'tube',
        label: 'Труба',
        icon: '⊙',
        tooltip: 'Вставить новую трубу',
        action: () => {},
      },
      {
        id: 'sketch',
        label: 'Эскиз',
        icon: '📐',
        tooltip: 'Создать эскиз',
        action: () => {},
      },
      {
        id: 'point',
        label: 'Точка',
        icon: '•',
        tooltip: 'Вставить точку',
        action: () => {},
      },
    ],
  },
  {
    id: 'sketch',
    label: 'Эскиз',
    commands: [
      {
        id: 'line',
        label: 'Линия',
        icon: '─',
        tooltip: 'Рисовать линию',
        action: () => {},
      },
      {
        id: 'circle',
        label: 'Окружность',
        icon: '○',
        tooltip: 'Рисовать окружность',
        action: () => {},
      },
      {
        id: 'rectangle',
        label: 'Прямоугольник',
        icon: '▭',
        tooltip: 'Рисовать прямоугольник',
        action: () => {},
      },
      {
        id: 'arc',
        label: 'Дуга',
        icon: '⌒',
        tooltip: 'Рисовать дугу',
        action: () => {},
      },
    ],
  },
];

// ============ Component ============

export const RibbonBar: React.FC<RibbonBarProps> = ({
  activeTab = 'file',
  onTabChange,
  onCommand,
  isDarkMode = false,
  tabs = DEFAULT_TABS,
}) => {
  const [openSubmenu, setOpenSubmenu] = useState<string | null>(null);
  const [isMobileMenuOpen, setIsMobileMenuOpen] = useState(false);

  const handleTabClick = useCallback(
    (tabId: string) => {
      onTabChange?.(tabId);
      setIsMobileMenuOpen(false);
    },
    [onTabChange]
  );

  const handleCommandClick = useCallback(
    (command: RibbonCommand) => {
      if (!command.disabled) {
        command.action?.();
        onCommand?.(command.id);
        setOpenSubmenu(null);
      }
    },
    [onCommand]
  );

  const handleKeyDown = useCallback(
    (e: React.KeyboardEvent, command: RibbonCommand) => {
      if (e.key === 'Enter' || e.key === ' ') {
        e.preventDefault();
        handleCommandClick(command);
      } else if (e.key === 'ArrowRight' && command.submenu) {
        e.preventDefault();
        setOpenSubmenu(command.id);
      } else if (e.key === 'ArrowLeft') {
        e.preventDefault();
        setOpenSubmenu(null);
      }
    },
    [handleCommandClick]
  );

  const currentTab = tabs.find((tab) => tab.id === activeTab);
  const currentCommands = currentTab?.commands || [];

  // Обработчик горячих клавиш
  useEffect(() => {
    const handleGlobalKeyDown = (e: KeyboardEvent) => {
      // Найти всех доступные команды со всех табов
      const allCommands: { command: RibbonCommand; tab: RibbonTab }[] = [];

      for (const tab of tabs) {
        for (const cmd of tab.commands) {
          allCommands.push({ command: cmd, tab });
        }
      }

      // Проверить каждую команду на её горячую клавишу
      for (const { command } of allCommands) {
        if (!command.shortcut || command.disabled) continue;

        // Парсить горячую клавишу (e.g., "Ctrl+S")
        const keys = command.shortcut.toLowerCase().split('+');
        const isCtrl = keys.includes('ctrl') && e.ctrlKey;
        const isShift = keys.includes('shift') && e.shiftKey;
        const isAlt = keys.includes('alt') && e.altKey;

        // Найти клавишу (последний элемент)
        const keyChar = keys[keys.length - 1];
        const eventKey = e.key.toLowerCase();

        // Проверить соответствие
        if (isCtrl && (isShift || !keys.includes('shift')) && (isAlt || !keys.includes('alt'))) {
          if (eventKey === keyChar || eventKey === keyChar.toLocaleUpperCase()) {
            e.preventDefault();
            command.action?.();
            onCommand?.(command.id);
            return;
          }
        }
      }
    };

    document.addEventListener('keydown', handleGlobalKeyDown);
    return () => document.removeEventListener('keydown', handleGlobalKeyDown);
  }, [tabs, onCommand]);

  return (
    <div
      className={`ribbon-bar ${isDarkMode ? 'ribbon-bar--dark' : ''}`}
      role="toolbar"
      aria-label="Главная панель инструментов"
    >
      {/* === Mobile Menu Button === */}
      <button
        className="ribbon-bar__mobile-toggle"
        aria-label="Переключить меню"
        aria-expanded={isMobileMenuOpen}
        onClick={() => setIsMobileMenuOpen(!isMobileMenuOpen)}
        aria-controls="ribbon-tabs"
      >
        ☰
      </button>

      {/* === Tab List === */}
      <div
        id="ribbon-tabs"
        className={`ribbon-bar__tabs ${isMobileMenuOpen ? 'ribbon-bar__tabs--open' : ''}`}
        role="tablist"
      >
        {tabs.map((tab) => (
          <button
            key={tab.id}
            className={`ribbon-bar__tab ${
              activeTab === tab.id ? 'ribbon-bar__tab--active' : ''
            }`}
            onClick={() => handleTabClick(tab.id)}
            role="tab"
            aria-selected={activeTab === tab.id}
            aria-controls={`ribbon-panel-${tab.id}`}
          >
            {tab.label}
          </button>
        ))}
      </div>

      {/* === Commands Panel === */}
      <div
        id={`ribbon-panel-${activeTab}`}
        className="ribbon-bar__commands"
        role="tabpanel"
        aria-labelledby={`ribbon-tab-${activeTab}`}
      >
        <div className="ribbon-bar__commands-group">
          {currentCommands.map((command) => (
            <div key={command.id} className="ribbon-bar__command-wrapper">
              <button
                className={`ribbon-bar__command ${
                  command.variant ? `ribbon-bar__command--${command.variant}` : ''
                } ${command.disabled ? 'ribbon-bar__command--disabled' : ''}`}
                onClick={() => handleCommandClick(command)}
                onKeyDown={(e) => handleKeyDown(e, command)}
                disabled={command.disabled}
                title={`${command.tooltip}${command.shortcut ? ` (${command.shortcut})` : ''}`}
                aria-label={`${command.label}${command.shortcut ? `, клавиша ${command.shortcut}` : ''}`}
              >
                <span className="ribbon-bar__command-icon">{command.icon}</span>
                <span className="ribbon-bar__command-label">{command.label}</span>
              </button>

              {/* === Submenu === */}
              {command.submenu && openSubmenu === command.id && (
                <div className="ribbon-bar__submenu" role="menu">
                  {command.submenu.map((item) => (
                    <button
                      key={item.id}
                      className="ribbon-bar__submenu-item"
                      onClick={() => handleCommandClick(item)}
                      disabled={item.disabled}
                      role="menuitem"
                      aria-label={item.label}
                    >
                      <span className="ribbon-bar__submenu-icon">{item.icon}</span>
                      <span className="ribbon-bar__submenu-label">{item.label}</span>
                      {item.shortcut && (
                        <span className="ribbon-bar__shortcut">{item.shortcut}</span>
                      )}
                    </button>
                  ))}
                </div>
              )}
            </div>
          ))}
        </div>
      </div>
    </div>
  );
};

export default RibbonBar;
