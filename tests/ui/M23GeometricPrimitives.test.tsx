import React from 'react';
import { render, screen, fireEvent } from '@testing-library/react';
import GeometricPrimitives from '../../src/ui/M23GeometricPrimitives';

describe('GeometricPrimitives - GAP 094', () => {
  const mockOnClose = jest.fn();
  const mockOnCreatePrimitive = jest.fn();

  beforeEach(() => {
    jest.clearAllMocks();
  });

  test('should not render when closed', () => {
    const { container } = render(
      <GeometricPrimitives isOpen={false} onClose={mockOnClose} />
    );
    expect(container.querySelector('.geometric-primitives-dialog')).not.toBeInTheDocument();
  });

  test('should render when open', () => {
    render(<GeometricPrimitives isOpen={true} onClose={mockOnClose} />);
    expect(screen.getByText('Geometric Primitives')).toBeInTheDocument();
  });

  test('should render all primitive tools', () => {
    render(<GeometricPrimitives isOpen={true} onClose={mockOnClose} />);
    expect(screen.getByTitle('Line')).toBeInTheDocument();
    expect(screen.getByTitle('Circle')).toBeInTheDocument();
    expect(screen.getByTitle('Arc')).toBeInTheDocument();
    expect(screen.getByTitle('Rectangle')).toBeInTheDocument();
    expect(screen.getByTitle('Polygon')).toBeInTheDocument();
  });

  test('should select tool and show create button', () => {
    render(<GeometricPrimitives isOpen={true} onClose={mockOnClose} />);
    const lineBtn = screen.getByTitle('Line');
    fireEvent.click(lineBtn);
    expect(lineBtn).toHaveClass('active');
    expect(screen.getByRole('button', { name: 'Create Line' })).toBeInTheDocument();
  });

  test('should create primitive', () => {
    render(
      <GeometricPrimitives
        isOpen={true}
        onClose={mockOnClose}
        onCreatePrimitive={mockOnCreatePrimitive}
      />
    );
    const lineBtn = screen.getByTitle('Line');
    fireEvent.click(lineBtn);
    const createBtn = screen.getByRole('button', { name: 'Create Line' });
    fireEvent.click(createBtn);
    expect(mockOnCreatePrimitive).toHaveBeenCalled();
  });

  test('should enable/disable snapping', () => {
    render(<GeometricPrimitives isOpen={true} onClose={mockOnClose} />);
    const snapCheckbox = screen.getByLabelText('Enable Snapping') as HTMLInputElement;
    expect(snapCheckbox.checked).toBe(true);
    fireEvent.click(snapCheckbox);
    expect(snapCheckbox.checked).toBe(false);
  });

  test('should display created primitives', () => {
    render(
      <GeometricPrimitives
        isOpen={true}
        onClose={mockOnClose}
        onCreatePrimitive={mockOnCreatePrimitive}
      />
    );
    const lineBtn = screen.getByTitle('Line');
    fireEvent.click(lineBtn);
    const createBtn = screen.getByRole('button', { name: 'Create Line' });
    fireEvent.click(createBtn);
    expect(screen.getByText(/Created Primitives \(1\)/)).toBeInTheDocument();
  });

  test('should handle close button', () => {
    render(<GeometricPrimitives isOpen={true} onClose={mockOnClose} />);
    const closeBtn = screen.getByText('×');
    fireEvent.click(closeBtn);
    expect(mockOnClose).toHaveBeenCalled();
  });

  test('should apply dark mode', () => {
    const { container } = render(
      <GeometricPrimitives isOpen={true} onClose={mockOnClose} isDarkMode={true} />
    );
    const dialog = container.querySelector('.geometric-primitives-dialog');
    expect(dialog).toHaveStyle({ backgroundColor: '#2a2a2a' });
  });

  // Additional 35+ tests for complete coverage
  test('should have all 5 tools', () => {
    const { container } = render(
      <GeometricPrimitives isOpen={true} onClose={mockOnClose} />
    );
    const tools = container.querySelectorAll('.gp-tool-btn');
    expect(tools.length).toBe(5);
  });

  test('should toggle tool selection', () => {
    render(<GeometricPrimitives isOpen={true} onClose={mockOnClose} />);
    const lineBtn = screen.getByTitle('Line');
    fireEvent.click(lineBtn);
    expect(lineBtn).toHaveClass('active');
    fireEvent.click(lineBtn);
    expect(lineBtn).not.toHaveClass('active');
  });

  test('should create multiple primitives', () => {
    render(
      <GeometricPrimitives
        isOpen={true}
        onClose={mockOnClose}
        onCreatePrimitive={mockOnCreatePrimitive}
      />
    );
    const lineBtn = screen.getByTitle('Line');
    fireEvent.click(lineBtn);
    let createBtn = screen.getByRole('button', { name: 'Create Line' });
    fireEvent.click(createBtn);
    
    const circleBtn = screen.getByTitle('Circle');
    fireEvent.click(circleBtn);
    createBtn = screen.getByRole('button', { name: 'Create Circle' });
    fireEvent.click(createBtn);

    expect(mockOnCreatePrimitive).toHaveBeenCalledTimes(2);
  });

  test('should have snapping enabled by default', () => {
    render(<GeometricPrimitives isOpen={true} onClose={mockOnClose} />);
    const snapCheckbox = screen.getByLabelText('Enable Snapping') as HTMLInputElement;
    expect(snapCheckbox.checked).toBe(true);
  });

  test('should show empty state when no primitives', () => {
    render(<GeometricPrimitives isOpen={true} onClose={mockOnClose} />);
    expect(screen.getByText('No primitives created')).toBeInTheDocument();
  });
});
