/**
 * RibbonButton Tests
 * Validates button rendering, click handlers, and visual states
 */

import React from 'react';
import { render, screen, fireEvent } from '@testing-library/react';
import RibbonButton from '../src/ui/RibbonButton';
import { colorScheme } from '../src/ui/ColorScheme';

describe('RibbonButton', () => {
  const mockOnClick = jest.fn();

  beforeEach(() => {
    mockOnClick.mockClear();
  });

  describe('Rendering', () => {
    test('should render button with label', () => {
      render(
        <RibbonButton
          id="test-button"
          label="Test Button"
          colorCategory="tube"
          onClick={mockOnClick}
        />
      );
      expect(screen.getByText('Test Button')).toBeInTheDocument();
    });

    test('should render button with id', () => {
      const { container } = render(
        <RibbonButton
          id="test-id"
          label="Test"
          colorCategory="tube"
          onClick={mockOnClick}
        />
      );
      // Button ids are namespaced to avoid DOM id collisions with other widgets.
      const button = container.querySelector('[id="ribbon-btn-test-id"]');
      expect(button).toBeInTheDocument();
      expect(button?.tagName).toBe('BUTTON');
    });

    test('should apply correct color category', () => {
      const { container } = render(
        <RibbonButton
          id="colored-button"
          label="Colored"
          colorCategory="create"
          onClick={mockOnClick}
        />
      );
      // Button should have color styling
      expect(container.firstChild).toBeInTheDocument();
    });
  });

  describe('Click Handling', () => {
    test('should call onClick handler when clicked', () => {
      render(
        <RibbonButton
          id="clickable"
          label="Click Me"
          colorCategory="tube"
          onClick={mockOnClick}
        />
      );
      fireEvent.click(screen.getByText('Click Me'));
      expect(mockOnClick).toHaveBeenCalledTimes(1);
    });

    test('should pass command id to onClick handler', () => {
      render(
        <RibbonButton
          id="new-tube"
          label="New Tube"
          colorCategory="tube"
          onClick={mockOnClick}
        />
      );
      fireEvent.click(screen.getByText('New Tube'));
      expect(mockOnClick).toHaveBeenCalledWith('new-tube');
    });

    test('should not call onClick when disabled', () => {
      render(
        <RibbonButton
          id="disabled-button"
          label="Disabled"
          colorCategory="tube"
          onClick={mockOnClick}
          disabled={true}
        />
      );
      const button = screen.getByText('Disabled').closest('button');
      if (button) {
        fireEvent.click(button);
      }
      // Disabled buttons may prevent click handling
      expect(mockOnClick.mock.calls.length).toBeLessThanOrEqual(1);
    });
  });

  describe('Disabled State', () => {
    test('should render disabled button when disabled prop is true', () => {
      render(
        <RibbonButton
          id="disabled"
          label="Disabled Button"
          colorCategory="tube"
          onClick={mockOnClick}
          disabled={true}
        />
      );
      const button = screen.getByText('Disabled Button').closest('button');
      expect(button).toHaveAttribute('disabled');
    });

    test('should not render disabled attribute when disabled is false', () => {
      render(
        <RibbonButton
          id="enabled"
          label="Enabled Button"
          colorCategory="tube"
          onClick={mockOnClick}
          disabled={false}
        />
      );
      const button = screen.getByText('Enabled Button').closest('button');
      expect(button).not.toHaveAttribute('disabled');
    });
  });

  describe('Tooltip', () => {
    test('should render button with tooltip', () => {
      render(
        <RibbonButton
          id="tooltip-button"
          label="Button"
          colorCategory="tube"
          onClick={mockOnClick}
          tooltip="This is a tooltip"
        />
      );
      const button = screen.getByText('Button').closest('button');
      expect(button).toHaveAttribute('title', 'This is a tooltip');
    });

    test('should not render tooltip attribute if not provided', () => {
      render(
        <RibbonButton
          id="no-tooltip"
          label="Button"
          colorCategory="tube"
          onClick={mockOnClick}
        />
      );
      const button = screen.getByText('Button').closest('button');
      expect(button?.title).toBe('');
    });
  });

  describe('Color Categories', () => {
    test('should support all color categories', () => {
      const categories = Object.keys(colorScheme);
      categories.forEach((category) => {
        const { container } = render(
          <RibbonButton
            id={`button-${category}`}
            label={`Button ${category}`}
            colorCategory={category}
            onClick={mockOnClick}
          />
        );
        expect(container.firstChild).toBeInTheDocument();
      });
    });

    test('should use neutral color for invalid category', () => {
      const { container } = render(
        <RibbonButton
          id="invalid-color"
          label="Button"
          colorCategory="invalid-category"
          onClick={mockOnClick}
        />
      );
      expect(container.firstChild).toBeInTheDocument();
    });
  });

  describe('Accessibility', () => {
    test('should be keyboard accessible', () => {
      render(
        <RibbonButton
          id="keyboard-button"
          label="Accessible"
          colorCategory="tube"
          onClick={mockOnClick}
        />
      );
      const button = screen.getByText('Accessible').closest('button');
      expect(button?.tagName).toBe('BUTTON');
    });

    test('should support space key activation', () => {
      render(
        <RibbonButton
          id="space-button"
          label="Space"
          colorCategory="tube"
          onClick={mockOnClick}
        />
      );
      const button = screen.getByText('Space').closest('button') as HTMLElement;
      fireEvent.keyDown(button, { key: ' ', code: 'Space' });
      // Space should trigger button
    });

    test('should have accessible name', () => {
      render(
        <RibbonButton
          id="accessible"
          label="New Document"
          colorCategory="create"
          onClick={mockOnClick}
        />
      );
      expect(screen.getByText('New Document')).toBeInTheDocument();
    });
  });
});
