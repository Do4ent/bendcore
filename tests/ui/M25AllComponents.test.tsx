import React from 'react';
import { render, screen, fireEvent } from '@testing-library/react';
import M25ManufacturingPanel from '../../src/ui/M25ManufacturingPanel';
import M25NCCodeViewer from '../../src/ui/M25NCCodeViewer';
import M25ToolpathPreview from '../../src/ui/M25ToolpathPreview';
import M25BOMGenerator from '../../src/ui/M25BOMGenerator';
import M25ManufacturingAnalysis from '../../src/ui/M25ManufacturingAnalysis';
import M25FEAResultsViewer from '../../src/ui/M25FEAResultsViewer';
import M25ReportGenerator from '../../src/ui/M25ReportGenerator';
import M25CostCalculator from '../../src/ui/M25CostCalculator';
import M25DrawingGenerator from '../../src/ui/M25DrawingGenerator';

describe('M25 Manufacturing & Analysis Components', () => {
  const mockOnClose = jest.fn();

  beforeEach(() => { jest.clearAllMocks(); });

  test('ManufacturingPanel renders', () => {
    render(<M25ManufacturingPanel isOpen={true} onClose={mockOnClose} />);
    expect(screen.getByText('Manufacturing Panel')).toBeInTheDocument();
  });

  test('NCCodeViewer renders', () => {
    render(<M25NCCodeViewer isOpen={true} onClose={mockOnClose} />);
    expect(screen.getByText('NC Code Viewer')).toBeInTheDocument();
  });

  test('ToolpathPreview renders', () => {
    render(<M25ToolpathPreview isOpen={true} onClose={mockOnClose} />);
    expect(screen.getByText('Toolpath Preview')).toBeInTheDocument();
  });

  test('BOMGenerator renders', () => {
    render(<M25BOMGenerator isOpen={true} onClose={mockOnClose} />);
    expect(screen.getByText('BOM Generator')).toBeInTheDocument();
  });

  test('ManufacturingAnalysis renders', () => {
    render(<M25ManufacturingAnalysis isOpen={true} onClose={mockOnClose} />);
    expect(screen.getByText('Manufacturing Analysis')).toBeInTheDocument();
  });

  test('FEAResultsViewer renders', () => {
    render(<M25FEAResultsViewer isOpen={true} onClose={mockOnClose} />);
    expect(screen.getByText('FEA Results')).toBeInTheDocument();
  });

  test('ReportGenerator renders', () => {
    render(<M25ReportGenerator isOpen={true} onClose={mockOnClose} />);
    expect(screen.getByText('Report Generator')).toBeInTheDocument();
  });

  test('CostCalculator renders', () => {
    render(<M25CostCalculator isOpen={true} onClose={mockOnClose} />);
    expect(screen.getByText('Cost Calculator')).toBeInTheDocument();
  });

  test('DrawingGenerator renders', () => {
    render(<M25DrawingGenerator isOpen={true} onClose={mockOnClose} />);
    expect(screen.getByText('Drawing Generator')).toBeInTheDocument();
  });

  test('CostCalculator calculates total', () => {
    render(<M25CostCalculator isOpen={true} onClose={mockOnClose} />);
    const inputs = screen.getAllByRole('spinbutton');
    fireEvent.change(inputs[0], { target: { value: '100' } });
    fireEvent.change(inputs[1], { target: { value: '50' } });
    expect(screen.getByText('Total: $150.00')).toBeInTheDocument();
  });

  test('FEAResultsViewer toggles result type', () => {
    render(<M25FEAResultsViewer isOpen={true} onClose={mockOnClose} />);
    const stressBtn = screen.getByRole('button', { name: 'Stress' });
    fireEvent.click(stressBtn);
    expect(stressBtn).toHaveClass('active');
  });

  test('Close buttons work', () => {
    render(<M25ManufacturingPanel isOpen={true} onClose={mockOnClose} />);
    fireEvent.click(screen.getByText('×'));
    expect(mockOnClose).toHaveBeenCalled();
  });

  test('Dark mode support', () => {
    const { container } = render(<M25ManufacturingPanel isOpen={true} onClose={mockOnClose} isDarkMode={true} />);
    const dialog = container.querySelector('.mfg-panel-dialog');
    expect(dialog).toHaveStyle({ backgroundColor: '#2a2a2a' });
  });
});
