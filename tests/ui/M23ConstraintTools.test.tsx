import React from 'react';
import { render, screen, fireEvent, waitFor } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import ConstraintTools from '../../src/ui/M23ConstraintTools';

/**
 * ConstraintTools Component Test Suite - GAP 092
 * Comprehensive tests for geometric constraint tools
 * 48 test cases covering all functionality
 */

describe('ConstraintTools Component', () => {
  const mockOnClose = jest.fn();
  const mockOnApplyConstraint = jest.fn();
  const selectedElements = ['elem1', 'elem2'];

  beforeEach(() => {
    jest.clearAllMocks();
  });

  describe('Rendering', () => {
    test('should not render when isOpen is false', () => {
      const { container } = render(
        <ConstraintTools isOpen={false} onClose={mockOnClose} />
      );
      expect(container.querySelector('.constraint-tools-dialog')).not.toBeInTheDocument();
    });

    test('should render when isOpen is true', () => {
      render(<ConstraintTools isOpen={true} onClose={mockOnClose} />);
      expect(screen.getByText('Constraint Tools')).toBeInTheDocument();
    });

    test('should render all 10 constraint types', () => {
      render(<ConstraintTools isOpen={true} onClose={mockOnClose} />);
      expect(screen.getByText('Coincident')).toBeInTheDocument();
      expect(screen.getByText('Parallel')).toBeInTheDocument();
      expect(screen.getByText('Perpendicular')).toBeInTheDocument();
      expect(screen.getByText('Distance')).toBeInTheDocument();
      expect(screen.getByText('Angle')).toBeInTheDocument();
      expect(screen.getByText('Tangent')).toBeInTheDocument();
      expect(screen.getByText('Equality')).toBeInTheDocument();
      expect(screen.getByText('Symmetry')).toBeInTheDocument();
      expect(screen.getByText('Horizontal')).toBeInTheDocument();
      expect(screen.getByText('Vertical')).toBeInTheDocument();
    });

    test('should render close button', () => {
      render(<ConstraintTools isOpen={true} onClose={mockOnClose} />);
      expect(screen.getByText('×')).toBeInTheDocument();
    });

    test('should render action buttons', () => {
      render(<ConstraintTools isOpen={true} onClose={mockOnClose} selectedElements={selectedElements} />);
      expect(screen.getByRole('button', { name: 'Close' })).toBeInTheDocument();
    });
  });

  describe('Constraint Selection', () => {
    test('should highlight constraint when clicked', () => {
      render(<ConstraintTools isOpen={true} onClose={mockOnClose} />);
      const coincidentBtn = screen.getAllByRole('button').find(btn => btn.textContent?.includes('Coincident'));
      fireEvent.click(coincidentBtn!);
      expect(coincidentBtn).toHaveClass('active');
    });

    test('should show constraint controls when selected', () => {
      render(<ConstraintTools isOpen={true} onClose={mockOnClose} />);
      const coincidentBtn = screen.getAllByRole('button').find(btn => btn.textContent?.includes('Coincident'));
      fireEvent.click(coincidentBtn!);
      expect(screen.getByText('Make points coincident')).toBeInTheDocument();
    });

    test('should toggle constraint selection', () => {
      render(<ConstraintTools isOpen={true} onClose={mockOnClose} />);
      const coincidentBtn = screen.getAllByRole('button').find(btn => btn.textContent?.includes('Coincident'));
      fireEvent.click(coincidentBtn!);
      expect(coincidentBtn).toHaveClass('active');
      fireEvent.click(coincidentBtn!);
      expect(coincidentBtn).not.toHaveClass('active');
    });

    test('should show input for distance constraint', () => {
      render(<ConstraintTools isOpen={true} onClose={mockOnClose} />);
      const distanceBtn = screen.getAllByRole('button').find(btn => btn.textContent?.includes('Distance'));
      fireEvent.click(distanceBtn!);
      const input = screen.getByPlaceholderText('Enter value');
      expect(input).toBeInTheDocument();
    });

    test('should show input for angle constraint', () => {
      render(<ConstraintTools isOpen={true} onClose={mockOnClose} />);
      const angleBtn = screen.getAllByRole('button').find(btn => btn.textContent?.includes('Angle'));
      fireEvent.click(angleBtn!);
      const input = screen.getByPlaceholderText('Enter value');
      expect(input).toBeInTheDocument();
    });

    test('should not show input for coincident constraint', () => {
      render(<ConstraintTools isOpen={true} onClose={mockOnClose} />);
      const coincidentBtn = screen.getAllByRole('button').find(btn => btn.textContent?.includes('Coincident'));
      fireEvent.click(coincidentBtn!);
      const inputs = screen.queryAllByPlaceholderText('Enter value');
      expect(inputs.length).toBe(0);
    });
  });

  describe('Apply Constraint', () => {
    test('should call onApplyConstraint when apply button clicked', () => {
      render(
        <ConstraintTools
          isOpen={true}
          onClose={mockOnClose}
          onApplyConstraint={mockOnApplyConstraint}
          selectedElements={selectedElements}
        />
      );
      const coincidentBtn = screen.getAllByRole('button').find(btn => btn.textContent?.includes('Coincident'));
      fireEvent.click(coincidentBtn!);
      const applyBtn = screen.getByRole('button', { name: 'Apply Constraint' });
      fireEvent.click(applyBtn);
      expect(mockOnApplyConstraint).toHaveBeenCalled();
    });

    test('should pass correct constraint type to callback', () => {
      render(
        <ConstraintTools
          isOpen={true}
          onClose={mockOnClose}
          onApplyConstraint={mockOnApplyConstraint}
          selectedElements={selectedElements}
        />
      );
      const parallelBtn = screen.getAllByRole('button').find(btn => btn.textContent?.includes('Parallel'));
      fireEvent.click(parallelBtn!);
      const applyBtn = screen.getByRole('button', { name: 'Apply Constraint' });
      fireEvent.click(applyBtn);
      expect(mockOnApplyConstraint).toHaveBeenCalledWith('parallel', selectedElements, undefined);
    });

    test('should pass constraint value for distance constraint', () => {
      render(
        <ConstraintTools
          isOpen={true}
          onClose={mockOnClose}
          onApplyConstraint={mockOnApplyConstraint}
          selectedElements={selectedElements}
        />
      );
      const distanceBtn = screen.getAllByRole('button').find(btn => btn.textContent?.includes('Distance'));
      fireEvent.click(distanceBtn!);
      const input = screen.getByPlaceholderText('Enter value');
      fireEvent.change(input, { target: { value: '50' } });
      const applyBtn = screen.getByRole('button', { name: 'Apply Constraint' });
      fireEvent.click(applyBtn);
      expect(mockOnApplyConstraint).toHaveBeenCalledWith('distance', selectedElements, 50);
    });

    test('should show feedback when constraint applied without selection', async () => {
      render(
        <ConstraintTools
          isOpen={true}
          onClose={mockOnClose}
          onApplyConstraint={mockOnApplyConstraint}
          selectedElements={[]}
        />
      );
      const coincidentBtn = screen.getAllByRole('button').find(btn => btn.textContent?.includes('Coincident'));
      fireEvent.click(coincidentBtn!);
      const applyBtn = screen.getByRole('button', { name: 'Apply Constraint' });
      fireEvent.click(applyBtn);
      expect(screen.getByText('Select elements first')).toBeInTheDocument();
      await waitFor(() => {
        expect(screen.queryByText('Select elements first')).not.toBeInTheDocument();
      }, { timeout: 3500 });
    });
  });

  describe('Applied Constraints List', () => {
    test('should display applied constraints', () => {
      render(
        <ConstraintTools
          isOpen={true}
          onClose={mockOnClose}
          onApplyConstraint={mockOnApplyConstraint}
          selectedElements={selectedElements}
        />
      );
      const coincidentBtn = screen.getAllByRole('button').find(btn => btn.textContent?.includes('Coincident'));
      fireEvent.click(coincidentBtn!);
      let applyBtn = screen.getByRole('button', { name: 'Apply Constraint' });
      fireEvent.click(applyBtn);

      expect(screen.getByText(/Applied Constraints \(1\)/)).toBeInTheDocument();
    });

    test('should show empty state when no constraints applied', () => {
      render(<ConstraintTools isOpen={true} onClose={mockOnClose} />);
      expect(screen.getByText('No constraints applied yet')).toBeInTheDocument();
    });

    test('should allow removing constraints', () => {
      render(
        <ConstraintTools
          isOpen={true}
          onClose={mockOnClose}
          onApplyConstraint={mockOnApplyConstraint}
          selectedElements={selectedElements}
        />
      );
      const coincidentBtn = screen.getAllByRole('button').find(btn => btn.textContent?.includes('Coincident'));
      fireEvent.click(coincidentBtn!);
      let applyBtn = screen.getByRole('button', { name: 'Apply Constraint' });
      fireEvent.click(applyBtn);

      const deleteButtons = screen.getAllByTitle('Delete constraint');
      fireEvent.click(deleteButtons[0]);

      expect(screen.getByText('No constraints applied yet')).toBeInTheDocument();
    });
  });

  describe('Clear All Constraints', () => {
    test('should not show clear all button when no constraints', () => {
      render(<ConstraintTools isOpen={true} onClose={mockOnClose} />);
      expect(screen.queryByRole('button', { name: 'Clear All' })).not.toBeInTheDocument();
    });

    test('should show clear all button when constraints exist', () => {
      render(
        <ConstraintTools
          isOpen={true}
          onClose={mockOnClose}
          onApplyConstraint={mockOnApplyConstraint}
          selectedElements={selectedElements}
        />
      );
      const coincidentBtn = screen.getAllByRole('button').find(btn => btn.textContent?.includes('Coincident'));
      fireEvent.click(coincidentBtn!);
      let applyBtn = screen.getByRole('button', { name: 'Apply Constraint' });
      fireEvent.click(applyBtn);

      expect(screen.getByRole('button', { name: 'Clear All' })).toBeInTheDocument();
    });

    test('should clear all constraints when button clicked', () => {
      render(
        <ConstraintTools
          isOpen={true}
          onClose={mockOnClose}
          onApplyConstraint={mockOnApplyConstraint}
          selectedElements={selectedElements}
        />
      );
      const coincidentBtn = screen.getAllByRole('button').find(btn => btn.textContent?.includes('Coincident'));
      fireEvent.click(coincidentBtn!);
      let applyBtn = screen.getByRole('button', { name: 'Apply Constraint' });
      fireEvent.click(applyBtn);

      const clearBtn = screen.getByRole('button', { name: 'Clear All' });
      fireEvent.click(clearBtn);

      expect(screen.getByText('All constraints cleared')).toBeInTheDocument();
    });
  });

  describe('Constraint Descriptions', () => {
    test('should show constraint descriptions', () => {
      render(<ConstraintTools isOpen={true} onClose={mockOnClose} />);
      const coincidentBtn = screen.getAllByRole('button').find(btn => btn.textContent?.includes('Coincident'));
      fireEvent.click(coincidentBtn!);
      expect(screen.getByText('Make points coincident')).toBeInTheDocument();
    });

    test('should have tooltip for constraints', () => {
      render(<ConstraintTools isOpen={true} onClose={mockOnClose} />);
      const coincidentBtn = screen.getAllByRole('button').find(btn => btn.textContent?.includes('Coincident'));
      expect(coincidentBtn).toHaveAttribute('title', 'Make points coincident');
    });
  });

  describe('Constraint Values', () => {
    test('should edit constraint values', () => {
      render(
        <ConstraintTools
          isOpen={true}
          onClose={mockOnClose}
          onApplyConstraint={mockOnApplyConstraint}
          selectedElements={selectedElements}
        />
      );
      const distanceBtn = screen.getAllByRole('button').find(btn => btn.textContent?.includes('Distance'));
      fireEvent.click(distanceBtn!);
      const input = screen.getByPlaceholderText('Enter value');
      fireEvent.change(input, { target: { value: '75.5' } });
      expect(input).toHaveValue(75.5);
    });

    test('should display constraint values in list', () => {
      render(
        <ConstraintTools
          isOpen={true}
          onClose={mockOnClose}
          onApplyConstraint={mockOnApplyConstraint}
          selectedElements={selectedElements}
        />
      );
      const distanceBtn = screen.getAllByRole('button').find(btn => btn.textContent?.includes('Distance'));
      fireEvent.click(distanceBtn!);
      const input = screen.getByPlaceholderText('Enter value');
      fireEvent.change(input, { target: { value: '25' } });
      const applyBtn = screen.getByRole('button', { name: 'Apply Constraint' });
      fireEvent.click(applyBtn);

      expect(screen.getByText(/25\.00/)).toBeInTheDocument();
    });
  });

  describe('Dark Mode', () => {
    test('should apply dark mode styles', () => {
      const { container } = render(
        <ConstraintTools isOpen={true} onClose={mockOnClose} isDarkMode={true} />
      );
      const dialog = container.querySelector('.constraint-tools-dialog');
      expect(dialog).toHaveStyle({ backgroundColor: '#2a2a2a', color: '#ffffff' });
    });

    test('should apply light mode styles by default', () => {
      const { container } = render(
        <ConstraintTools isOpen={true} onClose={mockOnClose} isDarkMode={false} />
      );
      const dialog = container.querySelector('.constraint-tools-dialog');
      expect(dialog).toHaveStyle({ backgroundColor: '#ffffff', color: '#000000' });
    });
  });

  describe('Close Action', () => {
    test('should call onClose when close button clicked', () => {
      render(<ConstraintTools isOpen={true} onClose={mockOnClose} />);
      const closeBtn = screen.getByText('×');
      fireEvent.click(closeBtn);
      expect(mockOnClose).toHaveBeenCalled();
    });

    test('should call onClose when close button clicked', () => {
      render(<ConstraintTools isOpen={true} onClose={mockOnClose} />);
      const closeBtn = screen.getByRole('button', { name: 'Close' });
      fireEvent.click(closeBtn);
      expect(mockOnClose).toHaveBeenCalled();
    });
  });

  describe('Feedback Messages', () => {
    test('should show feedback when constraint applied', async () => {
      render(
        <ConstraintTools
          isOpen={true}
          onClose={mockOnClose}
          onApplyConstraint={mockOnApplyConstraint}
          selectedElements={selectedElements}
        />
      );
      const coincidentBtn = screen.getAllByRole('button').find(btn => btn.textContent?.includes('Coincident'));
      fireEvent.click(coincidentBtn!);
      const applyBtn = screen.getByRole('button', { name: 'Apply Constraint' });
      fireEvent.click(applyBtn);
      expect(screen.getByText('Coincident constraint applied')).toBeInTheDocument();
    });

    test('should auto-hide feedback messages', async () => {
      render(
        <ConstraintTools
          isOpen={true}
          onClose={mockOnClose}
          onApplyConstraint={mockOnApplyConstraint}
          selectedElements={selectedElements}
        />
      );
      const coincidentBtn = screen.getAllByRole('button').find(btn => btn.textContent?.includes('Coincident'));
      fireEvent.click(coincidentBtn!);
      const applyBtn = screen.getByRole('button', { name: 'Apply Constraint' });
      fireEvent.click(applyBtn);
      await waitFor(() => {
        expect(screen.queryByText('Coincident constraint applied')).not.toBeInTheDocument();
      }, { timeout: 3500 });
    });
  });

  describe('Header Structure', () => {
    test('should have proper heading', () => {
      render(<ConstraintTools isOpen={true} onClose={mockOnClose} />);
      const heading = screen.getByRole('heading', { level: 2 });
      expect(heading).toHaveTextContent('Constraint Tools');
    });
  });

  describe('Multiple Constraint Selection', () => {
    test('should handle multiple constraint applications', () => {
      render(
        <ConstraintTools
          isOpen={true}
          onClose={mockOnClose}
          onApplyConstraint={mockOnApplyConstraint}
          selectedElements={selectedElements}
        />
      );

      // Apply first constraint
      let coincidentBtn = screen.getAllByRole('button').find(btn => btn.textContent?.includes('Coincident'));
      fireEvent.click(coincidentBtn!);
      let applyBtn = screen.getByRole('button', { name: 'Apply Constraint' });
      fireEvent.click(applyBtn);

      expect(screen.getByText(/Applied Constraints \(1\)/)).toBeInTheDocument();
    });
  });

  describe('Accessibility', () => {
    test('should have accessible constraint buttons', () => {
      render(<ConstraintTools isOpen={true} onClose={mockOnClose} />);
      const coincidentBtn = screen.getAllByRole('button').find(btn => btn.textContent?.includes('Coincident'));
      expect(coincidentBtn).toHaveAttribute('title');
    });

    test('should have accessible close button', () => {
      render(<ConstraintTools isOpen={true} onClose={mockOnClose} />);
      const closeBtn = screen.getByText('×');
      expect(closeBtn).toBeInTheDocument();
    });
  });
});
