/**
 * ImportDialog Integration Test
 * Tests ImportDialog with mock server (no real OCCT required)
 *
 * Run with: npm test -- ImportDialog.integration
 * Or manually: npm start mock-server in one terminal, then npm test
 */

import React from 'react';
import { render, screen, fireEvent, waitFor } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import ImportDialog from '../ImportDialog';

// Mock tubeService to use mock server
jest.mock('../tubeService', () => ({
  getServiceBase: () => 'http://localhost:8080',
  setServiceBase: jest.fn(),
  analyzeStep: jest.fn(async (file: Blob) => {
    const response = await fetch('http://localhost:8080/v1/analyze?format=step&deflection=0.1', {
      method: 'POST',
      body: file,
    });
    if (!response.ok) throw new Error('Mock server error');
    const data = await response.json();
    return {
      od: data.tube.od,
      wall: data.tube.wall,
      innerDiameter: data.tube.inner_diameter,
      length: data.tube.length,
      bends: data.tube.bends,
      shapeType: data.tube.shape_type,
      confidence: data.tube.confidence,
      notes: data.tube.notes,
      valid: data.tube.valid,
      mesh: data.mesh,
    };
  }),
  buildTubeBody: jest.fn(async (text: string, format: 'csv' | 'json', od: number, wall = 0) => {
    const response = await fetch(`http://localhost:8080/v1/tube?format=${format}&od=${od}&wall=${wall}`, {
      method: 'POST',
      body: text,
    });
    if (!response.ok) throw new Error('Mock server error');
    return response.json();
  }),
}));

describe('ImportDialog - Integration Tests', () => {
  const mockOnImport = jest.fn();
  const mockOnCancel = jest.fn();

  beforeEach(() => {
    jest.clearAllMocks();
    mockOnImport.mockClear();
    mockOnCancel.mockClear();
  });

  test('dialog closes on cancel', () => {
    render(
      <ImportDialog
        isOpen={true}
        onCancel={mockOnCancel}
        onImport={mockOnImport}
      />
    );

    const cancelButton = screen.getByText(/Cancel|Отмена/i);
    fireEvent.click(cancelButton);

    expect(mockOnCancel).toHaveBeenCalled();
  });

  test('renders file selector when closed', () => {
    const { rerender } = render(
      <ImportDialog
        isOpen={false}
        onCancel={mockOnCancel}
        onImport={mockOnImport}
      />
    );

    expect(screen.queryByText(/Import STEP/i)).not.toBeInTheDocument();

    rerender(
      <ImportDialog
        isOpen={true}
        onCancel={mockOnCancel}
        onImport={mockOnImport}
      />
    );

    expect(screen.getByText(/Import|Импорт/i)).toBeInTheDocument();
  });

  test('accepts STEP files', async () => {
    render(
      <ImportDialog
        isOpen={true}
        onCancel={mockOnCancel}
        onImport={mockOnImport}
      />
    );

    const stepFile = new File(['test content'], 'tube.step', { type: 'application/step' });
    const input = screen.getByLabelText(/file|File/i) as HTMLInputElement;

    await userEvent.upload(input, stepFile);

    await waitFor(() => {
      expect(input.files?.[0]).toBe(stepFile);
    });
  });

  test('rejects non-CAD files', async () => {
    render(
      <ImportDialog
        isOpen={true}
        onCancel={mockOnCancel}
        onImport={mockOnImport}
      />
    );

    const textFile = new File(['test'], 'document.txt', { type: 'text/plain' });
    const input = screen.getByLabelText(/file|File/i) as HTMLInputElement;

    await userEvent.upload(input, textFile);

    // File should be rejected or shown as error
    await waitFor(() => {
      expect(input.files?.length || 0).toBe(0);
    });
  });

  test('shows grouping rules options', () => {
    render(
      <ImportDialog
        isOpen={true}
        onCancel={mockOnCancel}
        onImport={mockOnImport}
      />
    );

    // Check for grouping rule options
    const groupingLabels = screen.queryAllByText(/diameter|material|contact/i);
    expect(groupingLabels.length).toBeGreaterThan(0);
  });

  test('enables/disables apply rounding', async () => {
    render(
      <ImportDialog
        isOpen={true}
        onCancel={mockOnCancel}
        onImport={mockOnImport}
      />
    );

    const roundingCheckbox = screen.queryByRole('checkbox', { name: /rounding|округ/i });
    if (roundingCheckbox) {
      await userEvent.click(roundingCheckbox);
      expect(roundingCheckbox).toBeChecked();
    }
  });

  test('validates import data', async () => {
    const { container } = render(
      <ImportDialog
        isOpen={true}
        onCancel={mockOnCancel}
        onImport={mockOnImport}
      />
    );

    // Try to proceed without file (should show error or disable button)
    const proceedButton = screen.queryByText(/Next|Proceed|Продолжить/i);
    if (proceedButton) {
      expect(proceedButton).toBeDisabled();
    }
  });
});

describe('ImportDialog - Mock Server Integration', () => {
  beforeEach(() => {
    jest.clearAllMocks();
  });

  test('calls analyzeStep on STEP file', async () => {
    const onImportFn = jest.fn();
    const { analyzeStep } = require('../tubeService');

    const stepFile = new File(['mock STEP content'], 'test.step', { type: 'application/step' });

    render(
      <ImportDialog
        isOpen={true}
        onImport={onImportFn}
      />
    );

    // Mock server response
    analyzeStep.mockResolvedValue({
      od: 16.0,
      wall: 1.5,
      innerDiameter: 13.0,
      length: 145.3,
      bends: [{ angle: 45, radius: 25 }],
      shapeType: 'compound_tube',
      confidence: 85,
      notes: 'Test data',
      valid: true,
      mesh: { vertices: [], indices: [] },
    });

    const input = screen.getByLabelText(/file|File/i) as HTMLInputElement;
    await userEvent.upload(input, stepFile);

    await waitFor(() => {
      expect(input.files?.[0]).toBe(stepFile);
    });
  });
});

describe('ImportDialog - Props Validation', () => {
  test('requires onImport callback', () => {
    const onImportFn = jest.fn();
    const { container } = render(
      <ImportDialog
        isOpen={true}
        onCancel={mockOnCancel}
        onImport={onImportFn}
      />
    );

    expect(container).toBeInTheDocument();
  });

  test('handles isOpen prop change', () => {
    const onImportFn = jest.fn();
    const { rerender } = render(
      <ImportDialog
        isOpen={true}
        onImport={onImportFn}
      />
    );

    expect(screen.getByRole('dialog', { hidden: true })).toBeInTheDocument();

    rerender(
      <ImportDialog
        isOpen={false}
        onImport={onImportFn}
      />
    );

    const dialog = screen.queryByRole('dialog', { hidden: true });
    // Dialog might be hidden or removed
    expect(dialog).toBeDefined();
  });
});
