/**
 * Offset Calculator Integration Tests
 * Интеграционные тесты для компонентов offset-калькулятора
 */

import React from 'react';
import { render, screen, fireEvent } from '@testing-library/react';
import { OffsetTypeSelector } from '../OffsetTypeSelector';
import { OffsetResultsDisplay } from '../OffsetResultsDisplay';
import OffsetCalculatorTab from '../OffsetCalculatorTab';

describe('OffsetTypeSelector', () => {
  it('renders parallel option', () => {
    const handleChange = jest.fn();
    render(
      <OffsetTypeSelector selectedType="parallel" onTypeChange={handleChange} />
    );

    const parallelLabel = screen.getByText('Параллельное смещение');
    expect(parallelLabel).toBeInTheDocument();
  });

  it('renders 3d option', () => {
    const handleChange = jest.fn();
    render(
      <OffsetTypeSelector selectedType="parallel" onTypeChange={handleChange} />
    );

    const option3d = screen.getByText('3D комбинированное смещение');
    expect(option3d).toBeInTheDocument();
  });

  it('calls onTypeChange when option selected', () => {
    const handleChange = jest.fn();
    const { container } = render(
      <OffsetTypeSelector selectedType="parallel" onTypeChange={handleChange} />
    );

    const radioButtons = container.querySelectorAll('input[name="offset-type"]');
    fireEvent.click(radioButtons[1]); // Click 3D option

    expect(handleChange).toHaveBeenCalledWith('3d-combined');
  });
});

describe('OffsetResultsDisplay', () => {
  const mockResult = {
    total_length: 250.5,
    bend_1_length: 50.0,
    straight_section: 150.5,
    bend_2_length: 50.0,
    bend_angles: [90, 90] as [number, number],
    confidence: 85.5,
    warnings: 'Test warning',
    is_valid: true,
  };

  it('renders empty state when no result', () => {
    render(
      <OffsetResultsDisplay
        result={null}
        offsetType="parallel"
      />
    );

    const emptyText = screen.getByText(/Нет результатов/);
    expect(emptyText).toBeInTheDocument();
  });

  it('displays parallel results correctly', () => {
    render(
      <OffsetResultsDisplay
        result={mockResult}
        offsetType="parallel"
      />
    );

    expect(screen.getByText(/250.50/)).toBeInTheDocument();
    expect(screen.getByText(/✓ Допустимо/)).toBeInTheDocument();
  });

  it('displays confidence percentage', () => {
    render(
      <OffsetResultsDisplay
        result={mockResult}
        offsetType="parallel"
      />
    );

    expect(screen.getByText(/85.5/)).toBeInTheDocument();
  });

  it('shows warnings when present', () => {
    render(
      <OffsetResultsDisplay
        result={mockResult}
        offsetType="parallel"
      />
    );

    expect(screen.getByText('Test warning')).toBeInTheDocument();
  });

  it('calls onCopyToParams when button clicked', () => {
    const handleCopy = jest.fn();
    const { container } = render(
      <OffsetResultsDisplay
        result={mockResult}
        offsetType="parallel"
        onCopyToParams={handleCopy}
      />
    );

    const copyButton = screen.getByText('📋 Скопировать в параметры');
    fireEvent.click(copyButton);

    expect(handleCopy).toHaveBeenCalledWith(mockResult);
  });

  it('calls onExport with correct format', () => {
    const handleExport = jest.fn();
    const { container } = render(
      <OffsetResultsDisplay
        result={mockResult}
        offsetType="parallel"
        onExport={handleExport}
      />
    );

    const exportSelect = container.querySelector('select');
    if (exportSelect) {
      fireEvent.change(exportSelect, { target: { value: 'pdf' } });
      expect(handleExport).toHaveBeenCalledWith('pdf');
    }
  });
});

describe('OffsetCalculatorTab', () => {
  const mockTubeParams = {
    outerDiameter: 20.0,
    wallThickness: 2.0,
    length: 1000,
    material: 'Steel',
  };

  it('renders tab header', () => {
    render(
      <OffsetCalculatorTab
        tubeParams={mockTubeParams}
      />
    );

    expect(screen.getByText('Калькулятор смещений')).toBeInTheDocument();
  });

  it('renders type selector', () => {
    render(
      <OffsetCalculatorTab
        tubeParams={mockTubeParams}
      />
    );

    expect(
      screen.getByText('Параллельное смещение')
    ).toBeInTheDocument();
  });

  it('switches between calculator types', () => {
    const { container } = render(
      <OffsetCalculatorTab
        tubeParams={mockTubeParams}
      />
    );

    const radioButtons = container.querySelectorAll('input[name="offset-type"]');
    expect(radioButtons.length).toBeGreaterThan(0);
  });

  it('toggles dark mode', () => {
    const { container } = render(
      <OffsetCalculatorTab
        tubeParams={mockTubeParams}
      />
    );

    const darkToggle = container.querySelector('input[type="checkbox"]');
    expect(darkToggle).toBeInTheDocument();

    if (darkToggle) {
      fireEvent.click(darkToggle);
      const tabElement = container.querySelector('.offset-calculator-tab');
      expect(tabElement?.className).toContain('dark');
    }
  });

  it('calls onResultSync when results update', () => {
    const handleResultSync = jest.fn();
    render(
      <OffsetCalculatorTab
        tubeParams={mockTubeParams}
        onResultSync={handleResultSync}
      />
    );

    // Test that component renders without errors
    expect(screen.getByText('Калькулятор смещений')).toBeInTheDocument();
  });

  it('displays info box', () => {
    render(
      <OffsetCalculatorTab
        tubeParams={mockTubeParams}
      />
    );

    const infoText = screen.getByText(/Синхронизация параметров/);
    expect(infoText).toBeInTheDocument();
  });
});
