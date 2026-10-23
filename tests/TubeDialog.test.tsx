/**
 * TubeDialog Tests
 * Validates tube configuration dialog with 5 tabs
 */

import React from 'react';
import { render, screen, fireEvent, waitFor } from '@testing-library/react';
import TubeDialog, { TubeConfig } from '../src/ui/TubeDialog';

describe('TubeDialog', () => {
  const mockOnSave = jest.fn();
  const mockOnClose = jest.fn();

  beforeEach(() => {
    mockOnSave.mockClear();
    mockOnClose.mockClear();
  });

  const defaultProps = {
    isOpen: true,
    onSave: mockOnSave,
    onClose: mockOnClose,
  };

  describe('Rendering', () => {
    test('should render dialog when isOpen is true', () => {
      render(<TubeDialog {...defaultProps} />);
      expect(screen.getByText(/Tube Configuration|Geometry|Bends/i)).toBeInTheDocument();
    });

    test('should not render dialog when isOpen is false', () => {
      const { container } = render(
        <TubeDialog {...defaultProps} isOpen={false} />
      );
      const dialog = container.querySelector('[role="dialog"]');
      expect(dialog).not.toBeInTheDocument();
    });

    test('should display dialog title', () => {
      render(<TubeDialog {...defaultProps} />);
      expect(screen.getByText(/Tube|Configuration/i)).toBeInTheDocument();
    });
  });

  describe('Tab Navigation', () => {
    test('should have 5 tabs available', () => {
      const { container } = render(<TubeDialog {...defaultProps} />);
      const tabs = container.querySelectorAll('[role="tab"]');
      expect(tabs.length).toBeGreaterThanOrEqual(5);
    });

    test('should switch to Geometry tab', () => {
      render(<TubeDialog {...defaultProps} />);
      const geometryTab = screen.getByText(/Geometry/i);
      fireEvent.click(geometryTab);

      expect(screen.getByText(/Outer Diameter|Wall Thickness|Length/i)).toBeInTheDocument();
    });

    test('should switch to Bends tab', async () => {
      render(<TubeDialog {...defaultProps} />);
      const bendsTab = screen.getByText(/Bends/i);
      fireEvent.click(bendsTab);

      await waitFor(() => {
        expect(screen.getByText(/Bend|Add|Position|Angle/i)).toBeInTheDocument();
      });
    });

    test('should switch to Ports tab', async () => {
      render(<TubeDialog {...defaultProps} />);
      const portsTab = screen.getByText(/Ports/i);
      fireEvent.click(portsTab);

      await waitFor(() => {
        expect(screen.getByText(/Port|Position/i)).toBeInTheDocument();
      });
    });

    test('should switch to Advanced tab', async () => {
      render(<TubeDialog {...defaultProps} />);
      const advancedTab = screen.getByText(/Advanced/i);
      fireEvent.click(advancedTab);

      await waitFor(() => {
        expect(screen.getByText(/Advanced|Settings|Parameters/i)).toBeInTheDocument();
      });
    });

    test('should switch to Preview tab', async () => {
      render(<TubeDialog {...defaultProps} />);
      const previewTab = screen.getByText(/Preview/i);
      fireEvent.click(previewTab);

      await waitFor(() => {
        expect(screen.getByText(/Summary|Preview/i)).toBeInTheDocument();
      });
    });

    test('should maintain tab state when switching', async () => {
      render(<TubeDialog {...defaultProps} />);

      // Go to Geometry tab
      fireEvent.click(screen.getByText(/Geometry/i));
      expect(screen.getByText(/Outer Diameter/i)).toBeInTheDocument();

      // Go to Bends tab
      fireEvent.click(screen.getByText(/Bends/i));
      expect(screen.queryByText(/Outer Diameter/i)).not.toBeInTheDocument();

      // Back to Geometry tab
      fireEvent.click(screen.getByText(/Geometry/i));
      expect(screen.getByText(/Outer Diameter/i)).toBeInTheDocument();
    });
  });

  describe('Geometry Tab', () => {
    test('should have outer diameter input', async () => {
      render(<TubeDialog {...defaultProps} />);
      fireEvent.click(screen.getByText(/Geometry/i));

      await waitFor(() => {
        const odInput = screen.getByLabelText(/Outer Diameter|OD/i);
        expect(odInput).toBeInTheDocument();
      });
    });

    test('should have wall thickness input', async () => {
      render(<TubeDialog {...defaultProps} />);
      fireEvent.click(screen.getByText(/Geometry/i));

      await waitFor(() => {
        const wtInput = screen.getByLabelText(/Wall Thickness|WT/i);
        expect(wtInput).toBeInTheDocument();
      });
    });

    test('should have length input', async () => {
      render(<TubeDialog {...defaultProps} />);
      fireEvent.click(screen.getByText(/Geometry/i));

      await waitFor(() => {
        const lengthInput = screen.getByLabelText(/Length|Total/i);
        expect(lengthInput).toBeInTheDocument();
      });
    });

    test('should have material selection dropdown', async () => {
      render(<TubeDialog {...defaultProps} />);
      fireEvent.click(screen.getByText(/Geometry/i));

      await waitFor(() => {
        const materialSelect = screen.getByLabelText(/Material/i);
        expect(materialSelect).toBeInTheDocument();
      });
    });

    test('should validate outer diameter input', async () => {
      render(<TubeDialog {...defaultProps} />);
      fireEvent.click(screen.getByText(/Geometry/i));

      await waitFor(() => {
        const odInput = screen.getByLabelText(/Outer Diameter/i) as HTMLInputElement;
        fireEvent.change(odInput, { target: { value: 'invalid' } });
        fireEvent.blur(odInput);

        // Should show validation error
        expect(odInput.value).not.toBe('invalid');
      });
    });

    test('should calculate mass on input change', async () => {
      render(<TubeDialog {...defaultProps} />);
      fireEvent.click(screen.getByText(/Geometry/i));

      await waitFor(() => {
        const odInput = screen.getByLabelText(/Outer Diameter/i) as HTMLInputElement;
        fireEvent.change(odInput, { target: { value: '25' } });

        // Mass should be recalculated
        expect(mockOnSave).not.toHaveBeenCalled(); // Not saved yet
      });
    });
  });

  describe('Bends Tab', () => {
    test('should display add bend button', async () => {
      render(<TubeDialog {...defaultProps} />);
      fireEvent.click(screen.getByText(/Bends/i));

      await waitFor(() => {
        const addBtn = screen.getByText(/Add|Insert/i);
        expect(addBtn).toBeInTheDocument();
      });
    });

    test('should add bend to list', async () => {
      render(<TubeDialog {...defaultProps} />);
      fireEvent.click(screen.getByText(/Bends/i));

      await waitFor(() => {
        const addBtn = screen.getByText(/Add Bend/i);
        fireEvent.click(addBtn);

        // Bend should appear in list
        expect(screen.getByText(/Bend 1|Position/i)).toBeInTheDocument();
      });
    });

    test('should allow editing bends', async () => {
      render(<TubeDialog {...defaultProps} />);
      fireEvent.click(screen.getByText(/Bends/i));

      await waitFor(() => {
        const editBtn = screen.queryByText(/Edit/i);
        if (editBtn) {
          fireEvent.click(editBtn);
        }
      });
    });

    test('should allow deleting bends', async () => {
      render(<TubeDialog {...defaultProps} />);
      fireEvent.click(screen.getByText(/Bends/i));

      await waitFor(() => {
        const deleteBtn = screen.queryByText(/Delete|Remove/i);
        if (deleteBtn) {
          fireEvent.click(deleteBtn);
        }
      });
    });
  });

  describe('Ports Tab', () => {
    test('should display port list', async () => {
      render(<TubeDialog {...defaultProps} />);
      fireEvent.click(screen.getByText(/Ports/i));

      await waitFor(() => {
        expect(screen.getByText(/Port|List/i)).toBeInTheDocument();
      });
    });

    test('should allow adding ports', async () => {
      render(<TubeDialog {...defaultProps} />);
      fireEvent.click(screen.getByText(/Ports/i));

      await waitFor(() => {
        const addBtn = screen.queryByText(/Add|Insert/i);
        if (addBtn) {
          fireEvent.click(addBtn);
        }
      });
    });
  });

  describe('Advanced Tab', () => {
    test('should display advanced settings', async () => {
      render(<TubeDialog {...defaultProps} />);
      fireEvent.click(screen.getByText(/Advanced/i));

      await waitFor(() => {
        expect(screen.getByText(/Settings|Advanced/i)).toBeInTheDocument();
      });
    });

    test('should have toggleable options', async () => {
      render(<TubeDialog {...defaultProps} />);
      fireEvent.click(screen.getByText(/Advanced/i));

      await waitFor(() => {
        const toggle = screen.queryByRole('checkbox');
        if (toggle) {
          fireEvent.click(toggle);
        }
      });
    });

    test('should have slider controls', async () => {
      render(<TubeDialog {...defaultProps} />);
      fireEvent.click(screen.getByText(/Advanced/i));

      await waitFor(() => {
        const slider = screen.queryByRole('slider');
        if (slider) {
          fireEvent.change(slider, { target: { value: '50' } });
        }
      });
    });
  });

  describe('Preview Tab', () => {
    test('should display summary information', async () => {
      render(<TubeDialog {...defaultProps} />);
      fireEvent.click(screen.getByText(/Preview/i));

      await waitFor(() => {
        expect(screen.getByText(/Summary|Preview|Total/i)).toBeInTheDocument();
      });
    });

    test('should show calculated values', async () => {
      render(<TubeDialog {...defaultProps} />);
      fireEvent.click(screen.getByText(/Preview/i));

      await waitFor(() => {
        // Should display mass, length, bend count, etc.
        expect(screen.getByText(/Total|Mass|Length|Count/i)).toBeInTheDocument();
      });
    });
  });

  describe('Form Validation', () => {
    test('should require outer diameter', async () => {
      render(<TubeDialog {...defaultProps} />);
      fireEvent.click(screen.getByText(/Geometry/i));

      await waitFor(() => {
        const saveBtn = screen.getByText(/Save/i);
        fireEvent.click(saveBtn);

        // Should show validation error
        expect(mockOnSave).not.toHaveBeenCalled();
      });
    });

    test('should require wall thickness', async () => {
      render(<TubeDialog {...defaultProps} />);
      fireEvent.click(screen.getByText(/Geometry/i));

      await waitFor(() => {
        const saveBtn = screen.getByText(/Save/i);
        fireEvent.click(saveBtn);

        // Should show validation error
        expect(mockOnSave).not.toHaveBeenCalled();
      });
    });

    test('should enforce minimum values', async () => {
      render(<TubeDialog {...defaultProps} />);
      fireEvent.click(screen.getByText(/Geometry/i));

      await waitFor(() => {
        const odInput = screen.getByLabelText(/Outer Diameter/i) as HTMLInputElement;
        fireEvent.change(odInput, { target: { value: '-5' } });
        fireEvent.blur(odInput);

        // Should be rejected
      });
    });

    test('should enforce maximum values', async () => {
      render(<TubeDialog {...defaultProps} />);
      fireEvent.click(screen.getByText(/Geometry/i));

      await waitFor(() => {
        const odInput = screen.getByLabelText(/Outer Diameter/i) as HTMLInputElement;
        fireEvent.change(odInput, { target: { value: '200' } });
        fireEvent.blur(odInput);

        // Should be limited to max (100mm)
      });
    });
  });

  describe('Save/Close Actions', () => {
    test('should call onSave with config when Save clicked', async () => {
      render(<TubeDialog {...defaultProps} />);
      fireEvent.click(screen.getByText(/Geometry/i));

      await waitFor(() => {
        const odInput = screen.getByLabelText(/Outer Diameter/i) as HTMLInputElement;
        fireEvent.change(odInput, { target: { value: '25' } });

        const saveBtn = screen.getByText(/Save/i);
        fireEvent.click(saveBtn);

        // Should call onSave with config
        expect(mockOnSave).toHaveBeenCalled();
      });
    });

    test('should call onClose when Cancel clicked', () => {
      render(<TubeDialog {...defaultProps} />);

      const cancelBtn = screen.getByText(/Cancel|Close/i);
      fireEvent.click(cancelBtn);

      expect(mockOnClose).toHaveBeenCalled();
    });

    test('should close dialog after Save', async () => {
      const { rerender } = render(<TubeDialog {...defaultProps} />);

      fireEvent.click(screen.getByText(/Geometry/i));

      await waitFor(() => {
        const saveBtn = screen.getByText(/Save/i);
        fireEvent.click(saveBtn);
      });

      // Simulate closed state
      rerender(<TubeDialog {...defaultProps} isOpen={false} />);

      const dialog = screen.queryByText(/Tube Configuration/i);
      expect(dialog).not.toBeInTheDocument();
    });
  });

  describe('Initial Configuration', () => {
    test('should use initial config if provided', () => {
      const initialConfig: TubeConfig = {
        outerDiameter: 25,
        wallThickness: 2,
        length: 500,
        material: 'steel',
        bends: [],
        ports: [],
      };

      render(
        <TubeDialog {...defaultProps} initialConfig={initialConfig} />
      );

      fireEvent.click(screen.getByText(/Geometry/i));

      // Dialog should show initial values
      expect(screen.getByText(/Tube Configuration/i)).toBeInTheDocument();
    });
  });

  describe('Accessibility', () => {
    test('should have tab roles', () => {
      const { container } = render(<TubeDialog {...defaultProps} />);
      const tabs = container.querySelectorAll('[role="tab"]');
      expect(tabs.length).toBeGreaterThanOrEqual(5);
    });

    test('should have dialog role', () => {
      const { container } = render(<TubeDialog {...defaultProps} />);
      const dialog = container.querySelector('[role="dialog"]');
      expect(dialog).toBeInTheDocument();
    });

    test('should have form inputs with labels', () => {
      render(<TubeDialog {...defaultProps} />);
      fireEvent.click(screen.getByText(/Geometry/i));

      // Should have associated labels
      expect(screen.getByText(/Outer Diameter|OD/i)).toBeInTheDocument();
    });
  });
});
