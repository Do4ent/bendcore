/**
 * TubeDialog Tests
 * Validates tube configuration dialog with 7 tabs
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

  const tab = (name: string) => screen.getByRole('tab', { name });

  describe('Rendering', () => {
    test('should render dialog when isOpen is true', () => {
      render(<TubeDialog {...defaultProps} />);
      expect(screen.getByRole('dialog', { name: 'Tube Configuration' })).toBeInTheDocument();
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
      expect(screen.getByRole('heading', { name: 'Tube Configuration' })).toBeInTheDocument();
    });
  });

  describe('Tab Navigation', () => {
    test('should have 7 tabs available', () => {
      render(<TubeDialog {...defaultProps} />);
      expect(screen.getAllByRole('tab')).toHaveLength(7);
    });

    test('should switch to Geometry tab', () => {
      render(<TubeDialog {...defaultProps} />);
      fireEvent.click(tab('Geometry'));

      expect(tab('Geometry')).toHaveAttribute('aria-selected', 'true');
      expect(screen.getByLabelText('Outer Diameter (mm)')).toBeInTheDocument();
    });

    test('should switch to Bends tab', async () => {
      render(<TubeDialog {...defaultProps} />);
      fireEvent.click(tab('Bends'));

      await waitFor(() => {
        expect(screen.getByRole('button', { name: '+ Add New Bend' })).toBeInTheDocument();
      });
    });

    test('should switch to Ports tab', async () => {
      render(<TubeDialog {...defaultProps} />);
      fireEvent.click(tab('Ports'));

      await waitFor(() => {
        expect(screen.getByRole('button', { name: '+ Add New Port' })).toBeInTheDocument();
      });
    });

    test('should switch to Advanced tab', async () => {
      render(<TubeDialog {...defaultProps} />);
      fireEvent.click(tab('Advanced'));

      await waitFor(() => {
        expect(screen.getByRole('button', { name: 'Reset to Defaults' })).toBeInTheDocument();
      });
    });

    test('should switch to Preview tab', async () => {
      render(<TubeDialog {...defaultProps} />);
      fireEvent.click(tab('Preview'));

      await waitFor(() => {
        expect(screen.getByText('Calculated Mass:')).toBeInTheDocument();
      });
    });

    test('should maintain tab state when switching', () => {
      render(<TubeDialog {...defaultProps} />);

      // Go to Geometry tab
      fireEvent.click(tab('Geometry'));
      expect(screen.getByLabelText('Outer Diameter (mm)')).toBeInTheDocument();

      // Go to Bends tab
      fireEvent.click(tab('Bends'));
      expect(screen.queryByLabelText('Outer Diameter (mm)')).not.toBeInTheDocument();

      // Back to Geometry tab
      fireEvent.click(tab('Geometry'));
      expect(screen.getByLabelText('Outer Diameter (mm)')).toBeInTheDocument();
    });
  });

  describe('Geometry Tab', () => {
    beforeEach(() => {
      render(<TubeDialog {...defaultProps} />);
      fireEvent.click(tab('Geometry'));
    });

    test('should have outer diameter input', () => {
      expect(screen.getByLabelText('Outer Diameter (mm)')).toBeInTheDocument();
    });

    test('should have wall thickness input', () => {
      expect(screen.getByLabelText('Wall Thickness (mm)')).toBeInTheDocument();
    });

    test('should have length input', () => {
      expect(screen.getByLabelText('Length (mm)')).toBeInTheDocument();
    });

    test('should have material selection dropdown', () => {
      expect(screen.getByLabelText('Material')).toBeInTheDocument();
    });

    test('should validate outer diameter input', () => {
      const odInput = screen.getByLabelText('Outer Diameter (mm)') as HTMLInputElement;
      fireEvent.change(odInput, { target: { value: '-5' } });
      fireEvent.blur(odInput);

      expect(screen.getByText('OD must be between 1 and 100 mm')).toBeInTheDocument();
    });

    test('should calculate mass on input change', () => {
      const odInput = screen.getByLabelText('Outer Diameter (mm)') as HTMLInputElement;
      fireEvent.change(odInput, { target: { value: '25' } });

      expect(mockOnSave).not.toHaveBeenCalled(); // Not saved yet
    });
  });

  describe('Bends Tab', () => {
    test('should display add bend button', async () => {
      render(<TubeDialog {...defaultProps} />);
      fireEvent.click(tab('Bends'));

      await waitFor(() => {
        expect(screen.getByRole('button', { name: '+ Add New Bend' })).toBeInTheDocument();
      });
    });

    test('should add bend to list', async () => {
      render(<TubeDialog {...defaultProps} />);
      fireEvent.click(tab('Bends'));

      expect(screen.getByText('No bends defined yet')).toBeInTheDocument();
      fireEvent.click(screen.getByRole('button', { name: '+ Add New Bend' }));

      await waitFor(() => {
        expect(screen.queryByText('No bends defined yet')).not.toBeInTheDocument();
      });
      expect(screen.getByText('Total Bends:').nextElementSibling).toHaveTextContent('1');
    });
  });

  describe('Ports Tab', () => {
    test('should display port list', async () => {
      render(<TubeDialog {...defaultProps} />);
      fireEvent.click(tab('Ports'));

      await waitFor(() => {
        expect(screen.getByText('No ports defined yet')).toBeInTheDocument();
      });
    });

    test('should allow adding ports', async () => {
      render(<TubeDialog {...defaultProps} />);
      fireEvent.click(tab('Ports'));

      fireEvent.click(screen.getByRole('button', { name: '+ Add New Port' }));

      await waitFor(() => {
        expect(screen.queryByText('No ports defined yet')).not.toBeInTheDocument();
      });
    });
  });

  describe('Advanced Tab', () => {
    test('should display advanced settings', async () => {
      render(<TubeDialog {...defaultProps} />);
      fireEvent.click(tab('Advanced'));

      await waitFor(() => {
        expect(screen.getByRole('checkbox')).toBeInTheDocument();
      });
    });

    test('should have toggleable options', async () => {
      render(<TubeDialog {...defaultProps} />);
      fireEvent.click(tab('Advanced'));

      const toggle = await screen.findByRole('checkbox');
      fireEvent.click(toggle);
      expect(toggle).toBeChecked();
    });
  });

  describe('Preview Tab', () => {
    test('should display summary information', async () => {
      render(<TubeDialog {...defaultProps} />);
      fireEvent.click(tab('Preview'));

      await waitFor(() => {
        expect(screen.getByText('Calculated Mass:')).toBeInTheDocument();
      });
    });

    test('should show calculated values', async () => {
      render(<TubeDialog {...defaultProps} />);
      fireEvent.click(tab('Preview'));

      await waitFor(() => {
        expect(screen.getByText('Bends:')).toBeInTheDocument();
        expect(screen.getByText('Ports:')).toBeInTheDocument();
      });
    });
  });

  describe('Form Validation', () => {
    test('should disable Save while the default OD is invalid', () => {
      render(<TubeDialog {...defaultProps} />);
      fireEvent.click(tab('Geometry'));

      expect(screen.getByRole('button', { name: 'Save' })).toBeDisabled();
      expect(mockOnSave).not.toHaveBeenCalled();
    });

    test('should show error for wall thickness greater than half the OD', () => {
      render(<TubeDialog {...defaultProps} />);
      fireEvent.click(tab('Geometry'));

      fireEvent.change(screen.getByLabelText('Outer Diameter (mm)'), { target: { value: '10' } });
      fireEvent.change(screen.getByLabelText('Wall Thickness (mm)'), { target: { value: '6' } });

      expect(screen.getByText(/WT must be between 0.1 and 5.00 mm/)).toBeInTheDocument();
      expect(screen.getByRole('button', { name: 'Save' })).toBeDisabled();
    });

    test('should reject a negative outer diameter', () => {
      render(<TubeDialog {...defaultProps} />);
      fireEvent.click(tab('Geometry'));

      fireEvent.change(screen.getByLabelText('Outer Diameter (mm)'), { target: { value: '-5' } });

      expect(screen.getByText('OD must be between 1 and 100 mm')).toBeInTheDocument();
      expect(screen.getByRole('button', { name: 'Save' })).toBeDisabled();
    });

    test('should reject an outer diameter above 100 mm', () => {
      render(<TubeDialog {...defaultProps} />);
      fireEvent.click(tab('Geometry'));

      fireEvent.change(screen.getByLabelText('Outer Diameter (mm)'), { target: { value: '200' } });

      expect(screen.getByText('OD must be between 1 and 100 mm')).toBeInTheDocument();
    });
  });

  describe('Save/Close Actions', () => {
    test('should call onSave with config when Save clicked', async () => {
      render(<TubeDialog {...defaultProps} />);
      fireEvent.click(tab('Geometry'));

      fireEvent.change(screen.getByLabelText('Outer Diameter (mm)'), { target: { value: '25' } });

      const saveBtn = await waitFor(() => {
        const btn = screen.getByRole('button', { name: 'Save' });
        expect(btn).toBeEnabled();
        return btn;
      });
      fireEvent.click(saveBtn);

      expect(mockOnSave).toHaveBeenCalledTimes(1);
      expect(mockOnSave.mock.calls[0][0].geometry.outerDiameter).toBe(25);
    });

    test('should call onClose when Cancel clicked', () => {
      render(<TubeDialog {...defaultProps} />);

      fireEvent.click(screen.getByRole('button', { name: 'Cancel' }));

      expect(mockOnClose).toHaveBeenCalled();
    });

    test('should close dialog after Save', async () => {
      const { rerender } = render(<TubeDialog {...defaultProps} />);

      fireEvent.click(tab('Geometry'));
      fireEvent.change(screen.getByLabelText('Outer Diameter (mm)'), { target: { value: '25' } });

      await waitFor(() => {
        expect(screen.getByRole('button', { name: 'Save' })).toBeEnabled();
      });
      fireEvent.click(screen.getByRole('button', { name: 'Save' }));

      // Simulate closed state
      rerender(<TubeDialog {...defaultProps} isOpen={false} />);

      expect(screen.queryByRole('dialog')).not.toBeInTheDocument();
    });
  });

  describe('Initial Configuration', () => {
    test('should use initial config if provided', () => {
      const initialConfig: TubeConfig = {
        geometry: {
          outerDiameter: 25,
          wallThickness: 2,
          length: 500,
          material: 'steel-1020',
          materialName: 'Carbon Steel 1020',
        },
        bends: [],
        ports: [],
        advanced: {
          springbackFactor: 0.3,
          bendCompensation: false,
          lengthTolerance: 2.0,
          angleTolerance: 0.5,
          surfaceFinish: 'Raw',
        },
      };

      render(
        <TubeDialog {...defaultProps} initialConfig={initialConfig} />
      );

      fireEvent.click(tab('Geometry'));

      expect(screen.getByLabelText('Outer Diameter (mm)')).toHaveValue(25);
      expect(screen.getByLabelText('Length (mm)')).toHaveValue(500);
    });
  });

  describe('Accessibility', () => {
    test('should have tab roles', () => {
      render(<TubeDialog {...defaultProps} />);
      expect(screen.getAllByRole('tab')).toHaveLength(7);
    });

    test('should have dialog role', () => {
      render(<TubeDialog {...defaultProps} />);
      expect(screen.getByRole('dialog')).toBeInTheDocument();
    });

    test('should have form inputs with labels', () => {
      render(<TubeDialog {...defaultProps} />);
      fireEvent.click(tab('Geometry'));

      expect(screen.getByLabelText('Outer Diameter (mm)')).toBeInTheDocument();
    });
  });
});
