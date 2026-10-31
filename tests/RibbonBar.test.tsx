/**
 * RibbonBar Tests
 * Validates ribbon layout, tab switching, command handling, and integration
 */

import React from 'react';
import { render, screen, fireEvent, waitFor, within } from '@testing-library/react';
import RibbonBar from '../src/ui/RibbonBar';

describe('RibbonBar', () => {
  const mockOnCommand = jest.fn();

  // Command labels such as "Edit" or "Save" occur in several places (search bar,
  // feature tree, multiple ribbon groups), so queries are scoped to the ribbon's
  // tabs and to the active tab panel.
  const tab = (name: string | RegExp) => screen.getByRole('tab', { name });
  const activePanel = () => screen.getByRole('tabpanel');
  const ribbonButton = (name: string | RegExp) =>
    within(activePanel()).getByRole('button', { name });
  const groupTitles = () =>
    Array.from(activePanel().querySelectorAll('p')).map((p) => p.textContent);

  beforeEach(() => {
    mockOnCommand.mockClear();
  });

  describe('Rendering', () => {
    test('should render ribbon bar', () => {
      const { container } = render(<RibbonBar onCommand={mockOnCommand} />);
      expect(container.firstChild).toBeInTheDocument();
    });

    test('should display Home tab by default', () => {
      render(<RibbonBar onCommand={mockOnCommand} />);
      // Home tab should be active
      expect(tab('Home')).toHaveAttribute('aria-selected', 'true');
      expect(activePanel()).toHaveAttribute('aria-labelledby', tab('Home').id);
      expect(ribbonButton('Options')).toBeInTheDocument();
    });

    test('should display all 5 tabs', () => {
      const { container } = render(<RibbonBar onCommand={mockOnCommand} />);
      // Should have 5 tab buttons (Home, Part, Assembly, Analysis, Drawing)
      const tabs = container.querySelectorAll('[role="tab"]');
      expect(tabs.length).toBeGreaterThanOrEqual(5);
      ['Home', 'Part', 'Assembly', 'Analysis', 'Drawing'].forEach((name) => {
        expect(tab(name)).toBeInTheDocument();
      });
    });
  });

  describe('Tab Navigation', () => {
    test('should switch to Part tab when clicked', async () => {
      render(<RibbonBar onCommand={mockOnCommand} />);
      fireEvent.click(tab('Part'));

      await waitFor(() => {
        expect(ribbonButton('New Tube')).toBeInTheDocument();
      });
      expect(ribbonButton('New Sketch')).toBeInTheDocument();
      expect(tab('Part')).toHaveAttribute('aria-selected', 'true');
      expect(tab('Home')).toHaveAttribute('aria-selected', 'false');
    });

    test('should switch to Assembly tab when clicked', async () => {
      render(<RibbonBar onCommand={mockOnCommand} />);
      fireEvent.click(tab('Assembly'));

      await waitFor(() => {
        expect(ribbonButton('Component')).toBeInTheDocument();
      });
      expect(groupTitles()).toEqual(expect.arrayContaining(['Insert', 'Import']));
    });

    test('should maintain active tab state', () => {
      const { rerender } = render(
        <RibbonBar onCommand={mockOnCommand} />
      );
      // Click to Part tab
      fireEvent.click(tab('Part'));

      // Re-render with same props
      rerender(<RibbonBar onCommand={mockOnCommand} />);

      // Part tab should still be visible
      expect(tab('Part')).toHaveAttribute('aria-selected', 'true');
      expect(ribbonButton('New Sketch')).toBeInTheDocument();
      expect(ribbonButton('New Tube')).toBeInTheDocument();
    });
  });

  describe('Command Handling', () => {
    test('should call onCommand when Home tab button clicked', () => {
      render(<RibbonBar onCommand={mockOnCommand} />);
      fireEvent.click(ribbonButton('New'));

      expect(mockOnCommand).toHaveBeenCalledWith('new-document');
    });

    test('should handle new-tube command', () => {
      render(<RibbonBar onCommand={mockOnCommand} />);
      // Navigate to Part tab
      fireEvent.click(tab('Part'));

      fireEvent.click(ribbonButton('New Tube'));

      expect(mockOnCommand).toHaveBeenCalledWith('new-tube');
    });

    test('should handle save command', () => {
      render(<RibbonBar onCommand={mockOnCommand} />);
      fireEvent.click(ribbonButton('Save'));

      expect(mockOnCommand).toHaveBeenCalledWith('save');
    });

    test('should handle open command', () => {
      render(<RibbonBar onCommand={mockOnCommand} />);
      fireEvent.click(ribbonButton('Open'));

      expect(mockOnCommand).toHaveBeenCalledWith('open');
    });
  });

  describe('Command Groups', () => {
    test('should display File group in Home tab', () => {
      render(<RibbonBar onCommand={mockOnCommand} />);
      expect(groupTitles()).toContain('File');
      ['New', 'Open', 'Save'].forEach((name) => {
        expect(ribbonButton(name)).toBeInTheDocument();
      });
    });

    test('should display Tube group in Part tab', async () => {
      render(<RibbonBar onCommand={mockOnCommand} />);
      fireEvent.click(tab('Part'));

      await waitFor(() => {
        expect(ribbonButton('New Tube')).toBeInTheDocument();
      });
    });

    test('should display Import group in Assembly tab', async () => {
      render(<RibbonBar onCommand={mockOnCommand} />);
      fireEvent.click(tab('Assembly'));

      await waitFor(() => {
        expect(groupTitles()).toContain('Import');
      });
      expect(ribbonButton('STEP')).toBeInTheDocument();
      expect(ribbonButton('DWFx')).toBeInTheDocument();
    });
  });

  describe('Dialog Integration', () => {
    test('should open tube dialog on new-tube command', async () => {
      render(<RibbonBar onCommand={mockOnCommand} />);
      fireEvent.click(tab('Part'));

      fireEvent.click(ribbonButton('New Tube'));

      await waitFor(() => {
        expect(mockOnCommand).toHaveBeenCalledWith('new-tube');
      });
      expect(screen.getByRole('dialog', { name: 'Tube Configuration' })).toBeInTheDocument();
    });

    test('should close dialog when cancelled', async () => {
      render(<RibbonBar onCommand={mockOnCommand} />);
      fireEvent.click(tab('Part'));
      fireEvent.click(ribbonButton('New Tube'));

      // Dialog should open and then be closeable
      const dialog = await screen.findByRole('dialog', { name: 'Tube Configuration' });
      const cancelButtons = within(dialog).getAllByRole('button', { name: 'Cancel' });
      fireEvent.click(cancelButtons[cancelButtons.length - 1]);

      await waitFor(() => {
        expect(screen.queryByRole('dialog', { name: 'Tube Configuration' })).not.toBeInTheDocument();
      });
    });
  });

  describe('Gap Checking Panel', () => {
    test('should handle check-gaps command', () => {
      render(<RibbonBar onCommand={mockOnCommand} />);
      fireEvent.click(tab('Analysis'));

      fireEvent.click(ribbonButton('Gaps'));
      expect(mockOnCommand).toHaveBeenCalledWith('check-gaps');
    });
  });

  describe('Mass Calculation Panel', () => {
    test('should handle calculate-mass command', () => {
      render(<RibbonBar onCommand={mockOnCommand} />);
      fireEvent.click(tab('Analysis'));

      fireEvent.click(ribbonButton('Mass'));
      expect(mockOnCommand).toHaveBeenCalledWith('calculate-mass');
    });
  });

  describe('Responsive Behavior', () => {
    test('should render on different screen sizes', () => {
      const { container } = render(<RibbonBar onCommand={mockOnCommand} />);
      expect(container.firstChild).toBeInTheDocument();

      // Simulate resize
      window.innerWidth = 768;
      window.dispatchEvent(new Event('resize'));

      expect(container.firstChild).toBeInTheDocument();
    });
  });

  describe('Context Sensitivity', () => {
    test('should show appropriate commands based on document type', () => {
      render(
        <RibbonBar
          onCommand={mockOnCommand}
          activeDocument={{ type: 'part' }}
        />
      );
      expect(tab('Part')).toHaveAttribute('aria-selected', 'true');
      expect(ribbonButton('New Sketch')).toBeInTheDocument();
      expect(ribbonButton('New Tube')).toBeInTheDocument();
    });

    test('should show assembly commands for assembly documents', () => {
      render(
        <RibbonBar
          onCommand={mockOnCommand}
          activeDocument={{ type: 'assembly' }}
        />
      );
      expect(tab('Assembly')).toHaveAttribute('aria-selected', 'true');
      expect(groupTitles()).toEqual(expect.arrayContaining(['Insert', 'Import']));
    });
  });

  describe('Accessibility', () => {
    test('tabs should be keyboard navigable', () => {
      render(<RibbonBar onCommand={mockOnCommand} />);
      const homeTab = tab('Home');
      homeTab.focus();

      fireEvent.keyDown(homeTab, { key: 'ArrowRight', code: 'ArrowRight' });
      // Should navigate to next tab
      expect(tab('Part')).toHaveAttribute('aria-selected', 'true');
      expect(document.activeElement).toBe(tab('Part'));

      fireEvent.keyDown(tab('Part'), { key: 'ArrowLeft', code: 'ArrowLeft' });
      expect(tab('Home')).toHaveAttribute('aria-selected', 'true');

      fireEvent.keyDown(tab('Home'), { key: 'End', code: 'End' });
      expect(tab('Drawing')).toHaveAttribute('aria-selected', 'true');
    });

    test('buttons should have accessible labels', () => {
      render(<RibbonBar onCommand={mockOnCommand} />);
      within(activePanel())
        .getAllByRole('button')
        .forEach((button) => {
          expect(button).toHaveAccessibleName();
        });
      expect(ribbonButton('New')).toBeInTheDocument();
      expect(ribbonButton('Open')).toBeInTheDocument();
      expect(ribbonButton('Save')).toBeInTheDocument();
    });
  });
});
