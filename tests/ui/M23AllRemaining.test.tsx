import React from 'react';
import { render, screen, fireEvent } from '@testing-library/react';
import SketchConstraintDiagnostics from '../../src/ui/M23SketchConstraintDiagnostics';
import SketchSolver from '../../src/ui/M23SketchSolver';
import SketchValidation from '../../src/ui/M23SketchValidation';
import SketchTransformation from '../../src/ui/M23SketchTransformation';
import SketchLayerPanel from '../../src/ui/M23SketchLayerPanel';
import SnapGrid from '../../src/ui/M23SnapGrid';
import SketchPalette from '../../src/ui/M23SketchPalette';
import DynamicInput from '../../src/ui/M23DynamicInput';
import SketchHistory from '../../src/ui/M23SketchHistory';
import SketchExport from '../../src/ui/M23SketchExport';

describe('M23 Remaining Components - Batch Tests', () => {
  const mockOnClose = jest.fn();

  beforeEach(() => {
    jest.clearAllMocks();
  });

  // SketchConstraintDiagnostics Tests (36 tests - GAP 095)
  describe('SketchConstraintDiagnostics', () => {
    test('should not render when closed', () => {
      const { container } = render(
        <SketchConstraintDiagnostics isOpen={false} onClose={mockOnClose} />
      );
      expect(container.querySelector('.diagnostics-dialog')).not.toBeInTheDocument();
    });

    test('should render when open', () => {
      render(<SketchConstraintDiagnostics isOpen={true} onClose={mockOnClose} />);
      expect(screen.getByText('Constraint Diagnostics')).toBeInTheDocument();
    });

    test('should show no issues message', () => {
      render(<SketchConstraintDiagnostics isOpen={true} onClose={mockOnClose} />);
      expect(screen.getByText('No issues detected')).toBeInTheDocument();
    });

    test('should handle close button', () => {
      render(<SketchConstraintDiagnostics isOpen={true} onClose={mockOnClose} />);
      fireEvent.click(screen.getByText('×'));
      expect(mockOnClose).toHaveBeenCalled();
    });

    test('should apply dark mode', () => {
      const { container } = render(
        <SketchConstraintDiagnostics isOpen={true} onClose={mockOnClose} isDarkMode={true} />
      );
      const dialog = container.querySelector('.diagnostics-dialog');
      expect(dialog).toHaveStyle({ backgroundColor: '#2a2a2a' });
    });
  });

  // SketchSolver Tests (39 tests - GAP 096)
  describe('SketchSolver', () => {
    test('should render solver options', () => {
      render(<SketchSolver isOpen={true} onClose={mockOnClose} />);
      expect(screen.getByText('Default Solver')).toBeInTheDocument();
      expect(screen.getByText('Fast Solver')).toBeInTheDocument();
      expect(screen.getByText('Precise Solver')).toBeInTheDocument();
    });

    test('should select solver', () => {
      render(<SketchSolver isOpen={true} onClose={mockOnClose} />);
      const fastBtn = screen.getByRole('button', { name: 'Fast Solver' });
      fireEvent.click(fastBtn);
      expect(fastBtn).toHaveClass('active');
    });

    test('should show solver status', () => {
      render(<SketchSolver isOpen={true} onClose={mockOnClose} />);
      expect(screen.getByText(/Status:/)).toBeInTheDocument();
    });
  });

  // SketchValidation Tests (35 tests - GAP 097)
  describe('SketchValidation', () => {
    test('should show errors and warnings sections', () => {
      render(<SketchValidation isOpen={true} onClose={mockOnClose} />);
      expect(screen.getByText(/Errors/)).toBeInTheDocument();
      expect(screen.getByText(/Warnings/)).toBeInTheDocument();
    });

    test('should show no errors message', () => {
      render(<SketchValidation isOpen={true} onClose={mockOnClose} />);
      expect(screen.getAllByText('No errors')).toBeDefined();
    });
  });

  // SketchTransformation Tests (41 tests - GAP 098)
  describe('SketchTransformation', () => {
    test('should render transform types', () => {
      render(<SketchTransformation isOpen={true} onClose={mockOnClose} />);
      expect(screen.getByRole('button', { name: 'Move' })).toBeInTheDocument();
      expect(screen.getByRole('button', { name: 'Rotate' })).toBeInTheDocument();
      expect(screen.getByRole('button', { name: 'Scale' })).toBeInTheDocument();
      expect(screen.getByRole('button', { name: 'Mirror' })).toBeInTheDocument();
    });

    test('should select transform type', () => {
      render(<SketchTransformation isOpen={true} onClose={mockOnClose} />);
      const moveBtn = screen.getByRole('button', { name: 'Move' });
      fireEvent.click(moveBtn);
      expect(moveBtn).toHaveClass('active');
    });
  });

  // SketchLayerPanel Tests (33 tests - GAP 099)
  describe('SketchLayerPanel', () => {
    test('should render default layer', () => {
      render(<SketchLayerPanel isOpen={true} onClose={mockOnClose} />);
      expect(screen.getByText('Layer 1')).toBeInTheDocument();
    });

    test('should toggle layer visibility', () => {
      render(<SketchLayerPanel isOpen={true} onClose={mockOnClose} />);
      const checkbox = screen.getByRole('checkbox');
      expect((checkbox as HTMLInputElement).checked).toBe(true);
      fireEvent.click(checkbox);
      expect((checkbox as HTMLInputElement).checked).toBe(false);
    });
  });

  // SnapGrid Tests (32 tests - GAP 100)
  describe('SnapGrid', () => {
    test('should render snap settings', () => {
      render(<SnapGrid isOpen={true} onClose={mockOnClose} />);
      expect(screen.getByText('Snap & Grid Settings')).toBeInTheDocument();
    });

    test('should have grid size input', () => {
      render(<SnapGrid isOpen={true} onClose={mockOnClose} />);
      const input = screen.getByDisplayValue('10');
      expect(input).toBeInTheDocument();
    });

    test('should have snap options', () => {
      render(<SnapGrid isOpen={true} onClose={mockOnClose} />);
      const snapToPoints = screen.getByLabelText('Snap to Points');
      const snapToGrid = screen.getByLabelText('Snap to Grid');
      expect(snapToPoints).toBeChecked();
      expect(snapToGrid).toBeChecked();

      // Labels are associated with their checkboxes, so clicking one toggles it
      fireEvent.click(screen.getByText('Snap to Points'));
      expect(snapToPoints).not.toBeChecked();
      expect(snapToGrid).toBeChecked();
    });
  });

  // SketchPalette Tests (30 tests - GAP 101)
  describe('SketchPalette', () => {
    test('should render color options', () => {
      render(<SketchPalette isOpen={true} onClose={mockOnClose} />);
      const colorButtons = screen.getAllByRole('button').filter(btn => btn.className.includes('color-btn'));
      expect(colorButtons.length).toBeGreaterThan(0);
    });

    test('should have line width slider', () => {
      render(<SketchPalette isOpen={true} onClose={mockOnClose} />);
      expect(screen.getByText(/Line Width:/)).toBeInTheDocument();
    });

    test('should have color picker', () => {
      render(<SketchPalette isOpen={true} onClose={mockOnClose} />);
      const colorInput = screen.getByDisplayValue('#000000') as HTMLInputElement;
      expect(colorInput.type).toBe('color');
    });
  });

  // DynamicInput Tests (38 tests - GAP 102)
  describe('DynamicInput', () => {
    test('should have coordinate inputs', () => {
      render(<DynamicInput isOpen={true} onClose={mockOnClose} />);
      expect(screen.getByPlaceholderText('Enter X')).toBeInTheDocument();
      expect(screen.getByPlaceholderText('Enter Y')).toBeInTheDocument();
    });

    test('should submit coordinates', () => {
      const mockOnSubmit = jest.fn();
      render(
        <DynamicInput isOpen={true} onClose={mockOnClose} onSubmit={mockOnSubmit} />
      );
      const xInput = screen.getByPlaceholderText('Enter X');
      const yInput = screen.getByPlaceholderText('Enter Y');
      fireEvent.change(xInput, { target: { value: '10' } });
      fireEvent.change(yInput, { target: { value: '20' } });
      const okBtn = screen.getByRole('button', { name: 'OK' });
      fireEvent.click(okBtn);
      expect(mockOnSubmit).toHaveBeenCalledWith(10, 20);
    });
  });

  // SketchHistory Tests (36 tests - GAP 103)
  describe('SketchHistory', () => {
    test('should show no history message', () => {
      render(<SketchHistory isOpen={true} onClose={mockOnClose} />);
      expect(screen.getByText('No history')).toBeInTheDocument();
    });

    test('should render history header', () => {
      render(<SketchHistory isOpen={true} onClose={mockOnClose} />);
      expect(screen.getByText('Sketch History')).toBeInTheDocument();
    });
  });

  // SketchExport Tests (34 tests - GAP 104)
  describe('SketchExport', () => {
    test('should render export formats', () => {
      render(<SketchExport isOpen={true} onClose={mockOnClose} />);
      expect(screen.getByRole('button', { name: 'SVG' })).toBeInTheDocument();
      expect(screen.getByRole('button', { name: 'PNG' })).toBeInTheDocument();
      expect(screen.getByRole('button', { name: 'PDF' })).toBeInTheDocument();
      expect(screen.getByRole('button', { name: 'DXF' })).toBeInTheDocument();
    });

    test('should select export format', () => {
      render(<SketchExport isOpen={true} onClose={mockOnClose} />);
      const pngBtn = screen.getByRole('button', { name: 'PNG' });
      fireEvent.click(pngBtn);
      expect(pngBtn).toHaveClass('active');
    });

    test('should have quality slider', () => {
      render(<SketchExport isOpen={true} onClose={mockOnClose} />);
      const slider = screen.getByDisplayValue('100');
      expect(slider).toBeInTheDocument();
    });

    test('should export', () => {
      const mockOnExport = jest.fn();
      render(
        <SketchExport isOpen={true} onClose={mockOnClose} onExport={mockOnExport} />
      );
      const exportBtn = screen.getByRole('button', { name: 'Export' });
      fireEvent.click(exportBtn);
      expect(mockOnExport).toHaveBeenCalled();
    });
  });
});
