/**
 * BendCore Formula Editor Component Tests - Q3 2027 Task 1 Phase 2
 * Jest + React Testing Library test suite
 *
 * Test Coverage:
 * - Component rendering
 * - User interactions (input, button clicks)
 * - Formula evaluation
 * - Error handling
 * - localStorage persistence
 * - Keyboard shortcuts
 */

import React from 'react';
import { render, screen, fireEvent, waitFor, within } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { FormulaEditor } from '../FormulaEditor';

// Mock the API
global.fetch = jest.fn();

describe('FormulaEditor Component', () => {
  const mockVariables = [
    { name: '$OD', value: 25.0, unit: 'mm', description: 'Outer Diameter' },
    { name: '$Wall', value: 2.5, unit: 'mm', description: 'Wall Thickness' },
    { name: '$Length', value: 100.0, unit: 'mm', description: 'Tube Length' },
  ];

  const mockOnApply = jest.fn();
  const mockOnCancel = jest.fn();

  beforeEach(() => {
    jest.clearAllMocks();
    // reset (not just clear) so persistent per-test responses do not leak
    (global.fetch as jest.Mock).mockReset();
    localStorage.clear();
  });

  describe('Rendering', () => {
    test('should render FormulaEditor component', () => {
      render(
        <FormulaEditor
          dimensionId="dim_001"
          variables={mockVariables}
          onApply={mockOnApply}
          onCancel={mockOnCancel}
        />
      );

      expect(screen.getByText('Parametric Dimension Formula')).toBeInTheDocument();
    });

    test('should render all variable buttons', () => {
      render(
        <FormulaEditor
          dimensionId="dim_001"
          variables={mockVariables}
          onApply={mockOnApply}
          onCancel={mockOnCancel}
        />
      );

      mockVariables.forEach((v) => {
        expect(screen.getByText(v.name)).toBeInTheDocument();
      });
    });

    test('should render operator buttons', () => {
      render(
        <FormulaEditor
          dimensionId="dim_001"
          variables={mockVariables}
          onApply={mockOnApply}
          onCancel={mockOnCancel}
        />
      );

      expect(screen.getByText('+')).toBeInTheDocument();
      expect(screen.getByText('-')).toBeInTheDocument();
      expect(screen.getByText('*')).toBeInTheDocument();
      expect(screen.getByText('/')).toBeInTheDocument();
      expect(screen.getByText('^')).toBeInTheDocument();
    });

    test('should render function buttons', () => {
      render(
        <FormulaEditor
          dimensionId="dim_001"
          variables={mockVariables}
          onApply={mockOnApply}
          onCancel={mockOnCancel}
        />
      );

      expect(screen.getByText('sqrt()')).toBeInTheDocument();
      expect(screen.getByText('abs()')).toBeInTheDocument();
      expect(screen.getByText('min()')).toBeInTheDocument();
      expect(screen.getByText('max()')).toBeInTheDocument();
    });

    test('should render Apply and Cancel buttons', () => {
      render(
        <FormulaEditor
          dimensionId="dim_001"
          variables={mockVariables}
          onApply={mockOnApply}
          onCancel={mockOnCancel}
        />
      );

      expect(screen.getByRole('button', { name: /apply/i })).toBeInTheDocument();
      expect(screen.getByRole('button', { name: /cancel/i })).toBeInTheDocument();
    });

    test('should apply initial formula if provided', () => {
      const initialFormula = '$OD * 2 + 5';
      render(
        <FormulaEditor
          dimensionId="dim_001"
          initialFormula={initialFormula}
          variables={mockVariables}
          onApply={mockOnApply}
          onCancel={mockOnCancel}
        />
      );

      const input = screen.getByPlaceholderText('e.g., $OD * 2 + 5') as HTMLTextAreaElement;
      expect(input.value).toBe(initialFormula);
    });
  });

  describe('User Input', () => {
    test('should update formula on text input', async () => {
      const user = userEvent.setup({ delay: null });
      render(
        <FormulaEditor
          dimensionId="dim_001"
          variables={mockVariables}
          onApply={mockOnApply}
          onCancel={mockOnCancel}
        />
      );

      const input = screen.getByPlaceholderText('e.g., $OD * 2 + 5');
      await user.type(input, '$OD * 2');

      expect(input).toHaveValue('$OD * 2');
    });

    test('should insert variable when clicking variable button', async () => {
      const user = userEvent.setup({ delay: null });
      render(
        <FormulaEditor
          dimensionId="dim_001"
          variables={mockVariables}
          onApply={mockOnApply}
          onCancel={mockOnCancel}
        />
      );

      const odButton = screen.getByText('$OD');
      await user.click(odButton);

      const input = screen.getByPlaceholderText('e.g., $OD * 2 + 5') as HTMLTextAreaElement;
      expect(input.value).toContain('$OD');
    });

    test('should insert operator with spaces', async () => {
      const user = userEvent.setup({ delay: null });
      render(
        <FormulaEditor
          dimensionId="dim_001"
          variables={mockVariables}
          onApply={mockOnApply}
          onCancel={mockOnCancel}
        />
      );

      const input = screen.getByPlaceholderText('e.g., $OD * 2 + 5') as HTMLTextAreaElement;
      await user.click(input);

      const plusButton = screen.getByRole('button', { name: '+' });
      await user.click(plusButton);

      expect(input.value).toContain('+');
    });

    test('should insert function call', async () => {
      const user = userEvent.setup({ delay: null });
      render(
        <FormulaEditor
          dimensionId="dim_001"
          variables={mockVariables}
          onApply={mockOnApply}
          onCancel={mockOnCancel}
        />
      );

      const sqrtButton = screen.getByText('sqrt()');
      await user.click(sqrtButton);

      const input = screen.getByPlaceholderText('e.g., $OD * 2 + 5') as HTMLTextAreaElement;
      expect(input.value).toContain('sqrt()');
    });
  });

  describe('Formula Evaluation', () => {
    test('should evaluate simple numeric formula', async () => {
      (global.fetch as jest.Mock).mockResolvedValue({
        ok: true,
        json: async () => ({ result: 10 }),
      });

      const user = userEvent.setup({ delay: null });
      render(
        <FormulaEditor
          dimensionId="dim_001"
          variables={mockVariables}
          onApply={mockOnApply}
          onCancel={mockOnCancel}
        />
      );

      const input = screen.getByPlaceholderText('e.g., $OD * 2 + 5');
      await user.type(input, '5 + 5');

      await waitFor(() => {
        expect(global.fetch).toHaveBeenCalled();
      });

      await waitFor(() => {
        expect(screen.getByText(/10/)).toBeInTheDocument();
      });
    });

    test('should evaluate formula with variables', async () => {
      (global.fetch as jest.Mock).mockResolvedValue({
        ok: true,
        json: async () => ({ result: 55 }),
      });

      const user = userEvent.setup({ delay: null });
      render(
        <FormulaEditor
          dimensionId="dim_001"
          variables={mockVariables}
          onApply={mockOnApply}
          onCancel={mockOnCancel}
        />
      );

      const input = screen.getByPlaceholderText('e.g., $OD * 2 + 5');
      await user.type(input, '$OD * 2 + 5');

      await waitFor(() => {
        expect(global.fetch).toHaveBeenCalledWith(
          '/api/bendcore/evaluate-formula',
          expect.objectContaining({
            method: 'POST',
            body: expect.stringContaining('"formula":"$OD * 2 + 5"'),
          })
        );
      });
    });

    test('should display error on invalid formula', async () => {
      (global.fetch as jest.Mock).mockResolvedValue({
        ok: false,
        json: async () => ({
          message: 'Division by zero',
          suggestions: ['Check for division by zero'],
        }),
      });

      const user = userEvent.setup({ delay: null });
      render(
        <FormulaEditor
          dimensionId="dim_001"
          variables={mockVariables}
          onApply={mockOnApply}
          onCancel={mockOnCancel}
        />
      );

      const input = screen.getByPlaceholderText('e.g., $OD * 2 + 5');
      await user.type(input, '10 / 0');

      await waitFor(() => {
        // The suggestion list also mentions division by zero, so scope to the message box
        expect(screen.getByText(/division by zero/i, { selector: '.error-message' })).toBeInTheDocument();
      });
    });

    test('should display error suggestions', async () => {
      (global.fetch as jest.Mock).mockResolvedValue({
        ok: false,
        json: async () => ({
          message: 'Undefined variable',
          suggestions: ['Check variable names', 'Available variables: $OD, $Wall'],
        }),
      });

      const user = userEvent.setup({ delay: null });
      render(
        <FormulaEditor
          dimensionId="dim_001"
          variables={mockVariables}
          onApply={mockOnApply}
          onCancel={mockOnCancel}
        />
      );

      const input = screen.getByPlaceholderText('e.g., $OD * 2 + 5');
      await user.type(input, '$UNKNOWN');

      await waitFor(() => {
        expect(screen.getByText(/Suggestions:/i)).toBeInTheDocument();
      });
    });
  });

  describe('Keyboard Shortcuts', () => {
    test('should apply formula on Ctrl+Enter', async () => {
      (global.fetch as jest.Mock).mockResolvedValue({
        ok: true,
        json: async () => ({ result: 10 }),
      });

      const user = userEvent.setup({ delay: null });
      render(
        <FormulaEditor
          dimensionId="dim_001"
          variables={mockVariables}
          onApply={mockOnApply}
          onCancel={mockOnCancel}
        />
      );

      const input = screen.getByPlaceholderText('e.g., $OD * 2 + 5');
      await user.type(input, '5 + 5');

      await waitFor(() => {
        expect(screen.getByText(/10/)).toBeInTheDocument();
      });

      await user.keyboard('{Control>}{Enter}{/Control}');

      await waitFor(() => {
        expect(mockOnApply).toHaveBeenCalledWith('5 + 5', 10);
      });
    });

    test('should cancel on Escape', async () => {
      const user = userEvent.setup({ delay: null });
      render(
        <FormulaEditor
          dimensionId="dim_001"
          variables={mockVariables}
          onApply={mockOnApply}
          onCancel={mockOnCancel}
        />
      );

      const input = screen.getByPlaceholderText('e.g., $OD * 2 + 5');
      await user.click(input);
      await user.keyboard('Escape');

      expect(mockOnCancel).toHaveBeenCalled();
    });
  });

  describe('localStorage Draft', () => {
    test('should save formula to localStorage on input', async () => {
      const user = userEvent.setup({ delay: null });
      render(
        <FormulaEditor
          dimensionId="dim_001"
          variables={mockVariables}
          onApply={mockOnApply}
          onCancel={mockOnCancel}
        />
      );

      const input = screen.getByPlaceholderText('e.g., $OD * 2 + 5');
      await user.type(input, '$OD * 2');

      expect(localStorage.getItem('bendcore_formula_draft')).toBe('$OD * 2');
    });

    test('should load draft from localStorage on mount', () => {
      localStorage.setItem('bendcore_formula_draft', '$Wall * 3');

      render(
        <FormulaEditor
          dimensionId="dim_001"
          variables={mockVariables}
          onApply={mockOnApply}
          onCancel={mockOnCancel}
        />
      );

      const input = screen.getByPlaceholderText('e.g., $OD * 2 + 5') as HTMLTextAreaElement;
      expect(input.value).toBe('$Wall * 3');
      expect(screen.getByText(/draft/i)).toBeInTheDocument();
    });

    test('should clear draft on apply', async () => {
      (global.fetch as jest.Mock).mockResolvedValue({
        ok: true,
        json: async () => ({ result: 7.5 }),
      });

      const user = userEvent.setup({ delay: null });
      render(
        <FormulaEditor
          dimensionId="dim_001"
          variables={mockVariables}
          onApply={mockOnApply}
          onCancel={mockOnCancel}
        />
      );

      const input = screen.getByPlaceholderText('e.g., $OD * 2 + 5');
      await user.type(input, '$Wall * 3');

      await waitFor(() => {
        expect(screen.getByText(/7.5/)).toBeInTheDocument();
      });

      const applyButton = screen.getByRole('button', { name: /apply/i });
      await user.click(applyButton);

      expect(localStorage.getItem('bendcore_formula_draft')).toBeNull();
    });

    test('should clear draft on cancel', async () => {
      const user = userEvent.setup({ delay: null });
      render(
        <FormulaEditor
          dimensionId="dim_001"
          variables={mockVariables}
          onApply={mockOnApply}
          onCancel={mockOnCancel}
        />
      );

      const input = screen.getByPlaceholderText('e.g., $OD * 2 + 5');
      await user.type(input, '$OD * 2');

      expect(localStorage.getItem('bendcore_formula_draft')).toBe('$OD * 2');

      const cancelButton = screen.getByRole('button', { name: /cancel/i });
      await user.click(cancelButton);

      expect(localStorage.getItem('bendcore_formula_draft')).toBeNull();
      expect(mockOnCancel).toHaveBeenCalled();
    });
  });

  describe('Apply Button State', () => {
    test('should disable Apply button when formula is invalid', async () => {
      (global.fetch as jest.Mock).mockResolvedValue({
        ok: false,
        json: async () => ({ message: 'Error' }),
      });

      const user = userEvent.setup({ delay: null });
      render(
        <FormulaEditor
          dimensionId="dim_001"
          variables={mockVariables}
          onApply={mockOnApply}
          onCancel={mockOnCancel}
        />
      );

      const input = screen.getByPlaceholderText('e.g., $OD * 2 + 5');
      await user.type(input, '10 / 0');

      await waitFor(() => {
        const applyButton = screen.getByRole('button', { name: /apply/i });
        expect(applyButton).toBeDisabled();
      });
    });

    test('should enable Apply button when formula is valid', async () => {
      (global.fetch as jest.Mock).mockResolvedValue({
        ok: true,
        json: async () => ({ result: 10 }),
      });

      const user = userEvent.setup({ delay: null });
      render(
        <FormulaEditor
          dimensionId="dim_001"
          variables={mockVariables}
          onApply={mockOnApply}
          onCancel={mockOnCancel}
        />
      );

      const input = screen.getByPlaceholderText('e.g., $OD * 2 + 5');
      await user.type(input, '5 + 5');

      await waitFor(() => {
        const applyButton = screen.getByRole('button', { name: /apply/i });
        expect(applyButton).not.toBeDisabled();
      });
    });
  });

  describe('Dark Mode', () => {
    test('should apply dark mode class when enabled', () => {
      const { container } = render(
        <FormulaEditor
          dimensionId="dim_001"
          variables={mockVariables}
          onApply={mockOnApply}
          onCancel={mockOnCancel}
          darkMode={true}
        />
      );

      const editor = container.querySelector('.formula-editor');
      expect(editor).toHaveClass('dark-mode');
    });

    test('should apply light mode class by default', () => {
      const { container } = render(
        <FormulaEditor
          dimensionId="dim_001"
          variables={mockVariables}
          onApply={mockOnApply}
          onCancel={mockOnCancel}
          darkMode={false}
        />
      );

      const editor = container.querySelector('.formula-editor');
      expect(editor).toHaveClass('light-mode');
    });
  });

  describe('Accessibility', () => {
    test('should have proper label associations', () => {
      render(
        <FormulaEditor
          dimensionId="dim_001"
          variables={mockVariables}
          onApply={mockOnApply}
          onCancel={mockOnCancel}
        />
      );

      const input = screen.getByPlaceholderText('e.g., $OD * 2 + 5');
      expect(input).toBeInTheDocument();
    });

    test('should have button tooltips', () => {
      render(
        <FormulaEditor
          dimensionId="dim_001"
          variables={mockVariables}
          onApply={mockOnApply}
          onCancel={mockOnCancel}
        />
      );

      const odButton = screen.getByText('$OD');
      expect(odButton).toHaveAttribute('title');
    });

    test('should have keyboard navigation', async () => {
      const user = userEvent.setup({ delay: null });
      render(
        <FormulaEditor
          dimensionId="dim_001"
          variables={mockVariables}
          onApply={mockOnApply}
          onCancel={mockOnCancel}
        />
      );

      const applyButton = screen.getByRole('button', { name: /apply/i });
      applyButton.focus();
      expect(document.activeElement).toBe(applyButton);
    });
  });
});
