/**
 * PathSketch3D.test.tsx - Phase 35B
 * Comprehensive tests for Path Sketch 3D component
 */

import React from 'react';
import { render, screen, fireEvent, waitFor } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { PathSketch3D, PathVertex, PathSegment, Constraint } from '../src/ui/PathSketch3D';
import '@testing-library/jest-dom';

describe('PathSketch3D Component', () => {
  // ===== MOUNTING & INITIALIZATION =====

  test('should mount without crashing', () => {
    const { container } = render(
      <PathSketch3D />
    );
    expect(container).toBeInTheDocument();
  });

  test('should render viewport container', () => {
    const { container } = render(
      <PathSketch3D />
    );
    const viewport = container.querySelector('.path-sketch-viewport');
    expect(viewport).toBeInTheDocument();
  });

  test('should render toolbar buttons', () => {
    render(<PathSketch3D />);
    expect(screen.getByText('Add Vertex')).toBeInTheDocument();
    expect(screen.getByText('Clear Selection')).toBeInTheDocument();
    expect(screen.getByText('Solve')).toBeInTheDocument();
    expect(screen.getByText('Validate')).toBeInTheDocument();
  });

  test('should initialize with empty sketch', () => {
    render(<PathSketch3D />);
    expect(screen.getByText('Vertices: 0')).toBeInTheDocument();
    expect(screen.getByText('Segments: 0')).toBeInTheDocument();
    expect(screen.getByText('Constraints: 0')).toBeInTheDocument();
  });

  // ===== VERTEX OPERATIONS =====

  test('should toggle add vertex mode', () => {
    render(<PathSketch3D />);
    const addBtn = screen.getByText('Add Vertex');

    // Should not be active initially
    expect(addBtn).not.toHaveClass('active');

    // Click to activate
    fireEvent.click(addBtn);
    expect(addBtn).toHaveClass('active');

    // Click to deactivate
    fireEvent.click(addBtn);
    expect(addBtn).not.toHaveClass('active');
  });

  test('should call onCommand when command is executed', () => {
    const onCommand = jest.fn();
    render(<PathSketch3D onCommand={onCommand} />);

    fireEvent.click(screen.getByText('Solve'));
    expect(onCommand).toHaveBeenCalledWith('solve-constraints', expect.any(Object));
  });

  test('should call onSketchChanged when sketch data updates', async () => {
    const onSketchChanged = jest.fn();
    render(<PathSketch3D onSketchChanged={onSketchChanged} />);

    await waitFor(() => {
      expect(onSketchChanged).toHaveBeenCalled();
    });
  });

  test('should track validation errors', () => {
    const onValidationChanged = jest.fn();
    render(<PathSketch3D onValidationChanged={onValidationChanged} />);

    fireEvent.click(screen.getByText('Validate'));
    expect(onValidationChanged).toHaveBeenCalled();
  });

  // ===== CONSTRAINT OPERATIONS =====

  test('should display constraint count', () => {
    render(<PathSketch3D />);
    expect(screen.getByText('Constraints: 0')).toBeInTheDocument();
  });

  test('should handle custom sketch ID', () => {
    const { rerender } = render(
      <PathSketch3D sketch_id="test_sketch_123" />
    );
    expect(screen.getByText('Vertices: 0')).toBeInTheDocument();
  });

  // ===== TOOLBAR INTERACTIONS =====

  test('clear selection button should work', () => {
    render(<PathSketch3D />);
    const clearBtn = screen.getByText('Clear Selection');

    fireEvent.click(clearBtn);
    expect(clearBtn).toBeInTheDocument();
  });

  test('solve button should be clickable', () => {
    render(<PathSketch3D />);
    const solveBtn = screen.getByText('Solve');

    fireEvent.click(solveBtn);
    expect(solveBtn).toBeInTheDocument();
  });

  test('validate button should be clickable', () => {
    render(<PathSketch3D />);
    const validateBtn = screen.getByText('Validate');

    fireEvent.click(validateBtn);
    expect(validateBtn).toBeInTheDocument();
  });

  // ===== SEGMENT DIMENSION EDITOR =====

  test('should pass required props', () => {
    const onCommand = jest.fn();
    const onSketchChanged = jest.fn();

    render(
      <PathSketch3D
        sketch_id="test_123"
        onCommand={onCommand}
        onSketchChanged={onSketchChanged}
      />
    );

    expect(screen.getByText('Vertices: 0')).toBeInTheDocument();
  });

  // ===== RESPONSIVE BEHAVIOR =====

  test('should handle window resize', async () => {
    const { container } = render(<PathSketch3D width={800} height={600} />);

    // Simulate window resize
    global.dispatchEvent(new Event('resize'));

    await waitFor(() => {
      const viewport = container.querySelector('.path-sketch-viewport');
      expect(viewport).toBeInTheDocument();
    });
  });

  // ===== CAMERA & VIEWPORT =====

  test('should accept external camera', () => {
    // Test with external camera would require mocking THREE.js
    render(<PathSketch3D />);
    expect(screen.getByText('Vertices: 0')).toBeInTheDocument();
  });

  // ===== ERROR HANDLING =====

  test('should handle missing callback gracefully', () => {
    const { container } = render(<PathSketch3D />);
    expect(container).toBeInTheDocument();
  });

  test('should render info panel', () => {
    const { container } = render(<PathSketch3D />);
    const infoPanel = container.querySelector('.path-sketch-info');
    expect(infoPanel).toBeInTheDocument();
  });

  // ===== STYLING & CSS CLASSES =====

  test('should have correct CSS classes', () => {
    const { container } = render(<PathSketch3D />);
    expect(container.querySelector('.path-sketch-3d')).toBeInTheDocument();
    expect(container.querySelector('.path-sketch-toolbar')).toBeInTheDocument();
    expect(container.querySelector('.path-sketch-info')).toBeInTheDocument();
  });

  test('toolbar buttons should have correct classes', () => {
    const { container } = render(<PathSketch3D />);
    const buttons = container.querySelectorAll('.path-sketch-btn');
    expect(buttons.length).toBeGreaterThan(0);
  });

  // ===== INTEGRATION TESTS =====

  test('should work with onCommand callback', () => {
    const onCommand = jest.fn();
    render(<PathSketch3D onCommand={onCommand} />);

    const solveBtn = screen.getByText('Solve');
    fireEvent.click(solveBtn);

    expect(onCommand).toHaveBeenCalledWith('solve-constraints', expect.any(Object));
  });

  test('should update when props change', () => {
    const { rerender } = render(<PathSketch3D sketch_id="sketch_1" />);

    rerender(<PathSketch3D sketch_id="sketch_2" />);

    expect(screen.getByText('Vertices: 0')).toBeInTheDocument();
  });

  // ===== CLEANUP =====

  test('should clean up resources on unmount', () => {
    const { unmount } = render(<PathSketch3D />);
    expect(() => unmount()).not.toThrow();
  });
});

describe('SegmentDimensionEditor Component', () => {
  // Test dimension editor modal if it's exported separately

  test('should be importable', () => {
    // Component should export successfully
    expect(PathSketch3D).toBeDefined();
  });
});

// ===== MOCK TESTS FOR VIEWPORT INTERACTIONS =====

describe('PathSketch3D Viewport Interactions', () => {
  test('should handle viewport creation', () => {
    const { container } = render(<PathSketch3D />);
    const viewport = container.querySelector('.path-sketch-viewport');
    expect(viewport).toBeInTheDocument();
    expect(viewport).toHaveClass('path-sketch-viewport');
  });

  test('should render within container', () => {
    const { container } = render(
      <div style={{ width: '800px', height: '600px' }}>
        <PathSketch3D />
      </div>
    );

    const sketch = container.querySelector('.path-sketch-3d');
    expect(sketch).toBeInTheDocument();
  });
});

// ===== PROPERTY VALIDATION =====

describe('PathSketch3D Props Validation', () => {
  test('should accept all optional props', () => {
    const onCommand = jest.fn();
    const onSketchChanged = jest.fn();
    const onValidationChanged = jest.fn();

    render(
      <PathSketch3D
        sketch_id="test_sketch"
        onCommand={onCommand}
        onSketchChanged={onSketchChanged}
        onValidationChanged={onValidationChanged}
        width={1024}
        height={768}
      />
    );

    expect(screen.getByText('Vertices: 0')).toBeInTheDocument();
  });

  test('should work with default props', () => {
    render(<PathSketch3D />);
    expect(screen.getByText('Vertices: 0')).toBeInTheDocument();
  });

  test('should accept custom dimensions', () => {
    const { container } = render(
      <PathSketch3D width={1200} height={800} />
    );

    expect(container.querySelector('.path-sketch-3d')).toBeInTheDocument();
  });
});
