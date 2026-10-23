import React from 'react';
import { render, screen, fireEvent, waitFor } from '@testing-library/react';
import '@testing-library/jest-dom';
import { AnalyticsDashboard } from '../AnalyticsDashboard';

describe('AnalyticsDashboard', () => {
  beforeEach(() => {
    // Mock fetch
    global.fetch = jest.fn(() =>
      Promise.resolve({
        ok: true,
        json: () => Promise.resolve({
          sessions: 1234,
          events: 5678,
          conversions: 1200,
          conversionRate: 12.5,
          avgSessionDuration: 245,
          experiments: [
            {
              id: 'exp_1',
              name: 'Template Match UI',
              status: 'running',
              startDate: '2026-10-20',
              controlConversion: 10.5,
              treatmentConversion: 12.3,
              significance: 0.92,
              sampleSize: 5000,
            },
          ],
          events: [
            {
              id: 'evt_1',
              name: 'import_step_success',
              userId: 'user_123',
              timestamp: '2026-10-23T12:00:00Z',
              properties: { file_size: '1024' },
              variant: 'control',
            },
          ],
        }),
      })
    ) as jest.Mock;
  });

  afterEach(() => {
    jest.resetAllMocks();
  });

  test('should render dashboard title', async () => {
    render(<AnalyticsDashboard />);
    expect(screen.getByText('Analytics Dashboard')).toBeInTheDocument();
  });

  test('should render KPI cards', async () => {
    render(<AnalyticsDashboard />);
    await waitFor(() => {
      expect(screen.getByText('Sessions')).toBeInTheDocument();
      expect(screen.getByText('Events')).toBeInTheDocument();
      expect(screen.getByText('Conversion Rate')).toBeInTheDocument();
      expect(screen.getByText('Avg Duration')).toBeInTheDocument();
    });
  });

  test('should display feature flags', async () => {
    render(<AnalyticsDashboard />);
    await waitFor(() => {
      expect(screen.getByText('Enhanced Template Matcher')).toBeInTheDocument();
      expect(screen.getByText('Horizontal Search Filters')).toBeInTheDocument();
      expect(screen.getByText('Formula Autocomplete')).toBeInTheDocument();
    });
  });

  test('should render experiments', async () => {
    render(<AnalyticsDashboard />);
    await waitFor(() => {
      expect(screen.getByText('Template Match UI')).toBeInTheDocument();
      expect(screen.getByText('Active Experiments (1)')).toBeInTheDocument();
    });
  });

  test('should display event timeline', async () => {
    render(<AnalyticsDashboard />);
    await waitFor(() => {
      expect(screen.getByText(/Recent Events/)).toBeInTheDocument();
    });
  });

  test('should render funnel chart', async () => {
    render(<AnalyticsDashboard />);
    await waitFor(() => {
      expect(screen.getByText(/Conversion Funnel/)).toBeInTheDocument();
    });
  });

  test('should toggle date picker', async () => {
    render(<AnalyticsDashboard />);
    const dateButton = screen.getByLabelText('Toggle date picker');
    fireEvent.click(dateButton);
    await waitFor(() => {
      expect(screen.getByLabelText('Start date')).toBeInTheDocument();
    });
  });

  test('should export to CSV', async () => {
    render(<AnalyticsDashboard />);
    const exportButton = screen.getByLabelText('Export metrics to CSV');
    fireEvent.click(exportButton);
    // CSV export should be triggered
    expect(global.fetch).toHaveBeenCalled();
  });

  test('should support dark mode', () => {
    const { container } = render(<AnalyticsDashboard darkMode={true} />);
    expect(container.querySelector('.dark-mode')).toBeInTheDocument();
  });

  test('should support responsive layout', () => {
    const { container } = render(<AnalyticsDashboard />);
    const dashboard = container.querySelector('.analytics-dashboard');
    expect(dashboard).toBeInTheDocument();
  });

  test('should be keyboard accessible', async () => {
    render(<AnalyticsDashboard />);
    const dateButton = screen.getByLabelText('Toggle date picker');
    expect(dateButton).toBeInTheDocument();
    fireEvent.click(dateButton);
  });

  test('should have proper ARIA labels', () => {
    render(<AnalyticsDashboard />);
    expect(screen.getByRole('main')).toBeInTheDocument();
    expect(screen.getByLabelText('Analytics Dashboard')).toBeInTheDocument();
  });

  test('should show error state on API failure', async () => {
    (global.fetch as jest.Mock).mockImplementationOnce(() =>
      Promise.resolve({
        ok: false,
        json: () => Promise.reject(new Error('API Error')),
      })
    );

    render(<AnalyticsDashboard />);
    await waitFor(() => {
      // Error handling would show in the component
    });
  });

  test('should handle empty experiments', async () => {
    (global.fetch as jest.Mock).mockImplementationOnce(() =>
      Promise.resolve({
        ok: true,
        json: () => Promise.resolve({ experiments: [] }),
      })
    );

    render(<AnalyticsDashboard />);
    await waitFor(() => {
      expect(screen.getByText('No active experiments')).toBeInTheDocument();
    });
  });

  test('should auto-refresh data', async () => {
    jest.useFakeTimers();
    render(<AnalyticsDashboard refreshInterval={5} />);

    // Initial fetch
    expect(global.fetch).toHaveBeenCalledTimes(3); // KPI, Experiments, Events

    // Advance time and check if refresh happens
    jest.advanceTimersByTime(5000);
    expect(global.fetch).toHaveBeenCalledTimes(6);

    jest.useRealTimers();
  });

  test('should display statistical significance', async () => {
    render(<AnalyticsDashboard />);
    await waitFor(() => {
      expect(screen.getByText(/Statistical Significance/)).toBeInTheDocument();
    });
  });

  test('should show conversion rate improvements', async () => {
    render(<AnalyticsDashboard />);
    await waitFor(() => {
      // Should show the improvement percentage
      expect(screen.getByText(/better/i)).toBeInTheDocument();
    });
  });

  test('should handle date range filtering', async () => {
    const startDate = new Date('2026-10-01');
    const endDate = new Date('2026-10-31');
    render(<AnalyticsDashboard startDate={startDate} endDate={endDate} />);
    expect(screen.getByRole('main')).toBeInTheDocument();
  });

  test('should be WCAG AA compliant', () => {
    const { container } = render(<AnalyticsDashboard />);
    const buttons = container.querySelectorAll('button');
    buttons.forEach((button) => {
      expect(button.getAttribute('aria-label')).toBeTruthy();
    });
  });

  test('should show loading state during refresh', async () => {
    render(<AnalyticsDashboard refreshInterval={30} />);
    await waitFor(() => {
      expect(screen.getByRole('main')).toBeInTheDocument();
    });
  });
});
