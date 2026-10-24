import React from 'react';
import { render, screen, fireEvent, waitFor, within } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { ParallelOffsetCalculator } from '../ParallelOffsetCalculator';

// Mock fetch
global.fetch = jest.fn();

describe('ParallelOffsetCalculator Component', () => {
  beforeEach(() => {
    jest.clearAllMocks();
  });

  // ======================== ТЕСТЫ РЕНДЕРИНГА ========================

  test('должен рендериться без ошибок', () => {
    const { container } = render(<ParallelOffsetCalculator />);
    expect(container).toBeInTheDocument();
  });

  test('должен отображать заголовок', () => {
    render(<ParallelOffsetCalculator />);
    expect(screen.getByText(/Калькулятор параллельного смещения/)).toBeInTheDocument();
  });

  test('должен отображать все разделы параметров', () => {
    render(<ParallelOffsetCalculator />);
    expect(screen.getByText(/Параметры трубы/)).toBeInTheDocument();
    expect(screen.getByText(/Параметры смещения/)).toBeInTheDocument();
  });

  test('должен содержать кнопку Calculate', () => {
    render(<ParallelOffsetCalculator />);
    expect(screen.getByRole('button', { name: /Рассчитать/ })).toBeInTheDocument();
  });

  // ======================== ТЕСТЫ INPUT ПОЛЕЙ ========================

  test('должен иметь input для диаметра', () => {
    render(<ParallelOffsetCalculator />);
    const input = screen.getByLabelText(/Внешний диаметр/);
    expect(input).toBeInTheDocument();
    expect(input).toHaveValue(20);
  });

  test('должен иметь input для толщины стенки', () => {
    render(<ParallelOffsetCalculator />);
    const input = screen.getByLabelText(/Толщина стенки/);
    expect(input).toBeInTheDocument();
    expect(input).toHaveValue(2);
  });

  test('должен иметь slider для угла гибки', () => {
    render(<ParallelOffsetCalculator />);
    const slider = screen.getByRole('slider');
    expect(slider).toBeInTheDocument();
  });

  test('должен иметь select для материала', () => {
    render(<ParallelOffsetCalculator />);
    const select = screen.getByLabelText(/Материал/);
    expect(select).toBeInTheDocument();
  });

  test('должен содержать dropdown для стандартных диаметров', () => {
    render(<ParallelOffsetCalculator />);
    const selects = screen.getAllByTitle(/Выбрать стандартный размер/);
    expect(selects.length).toBeGreaterThan(0);
  });

  // ======================== ТЕСТЫ ИЗМЕНЕНИЯ ЗНАЧЕНИЙ ========================

  test('должен обновлять значение диаметра при изменении input', async () => {
    render(<ParallelOffsetCalculator />);
    const input = screen.getByLabelText(/Внешний диаметр/) as HTMLInputElement;

    await userEvent.clear(input);
    await userEvent.type(input, '25');

    expect(input.value).toBe('25');
  });

  test('должен обновлять значение угла при изменении slider', async () => {
    render(<ParallelOffsetCalculator />);
    const slider = screen.getByRole('slider') as HTMLInputElement;

    fireEvent.change(slider, { target: { value: '45' } });

    expect(slider.value).toBe('45');
  });

  test('должен обновлять материал при выборе из dropdown', async () => {
    render(<ParallelOffsetCalculator />);
    const select = screen.getByLabelText(/Материал/) as HTMLSelectElement;

    await userEvent.selectOptions(select, 'Aluminum');

    expect(select.value).toBe('Aluminum');
  });

  // ======================== ТЕСТЫ ВАЛИДАЦИИ ========================

  test('должен показывать ошибку при недопустимом диаметре', async () => {
    render(<ParallelOffsetCalculator />);
    const input = screen.getByLabelText(/Внешний диаметр/);
    const button = screen.getByRole('button', { name: /Рассчитать/ });

    await userEvent.clear(input);
    await userEvent.type(input, '2');
    fireEvent.click(button);

    await waitFor(() => {
      expect(screen.getByText(/Диаметр должен быть в диапазоне/)).toBeInTheDocument();
    });
  });

  test('должен показывать ошибку при недопустимом угле', async () => {
    render(<ParallelOffsetCalculator />);
    const angleInput = screen.getByRole('spinbutton', {
      name: /Угол гибки \(5-180°\)/,
    });
    const button = screen.getByRole('button', { name: /Рассчитать/ });

    await userEvent.clear(angleInput);
    await userEvent.type(angleInput, '2');
    fireEvent.click(button);

    await waitFor(() => {
      expect(screen.getByText(/Угол должен быть в диапазоне/)).toBeInTheDocument();
    });
  });

  test('должен показывать ошибку при отрицательном смещении', async () => {
    render(<ParallelOffsetCalculator />);
    const input = screen.getByLabelText(/Расстояние смещения/);
    const button = screen.getByRole('button', { name: /Рассчитать/ });

    await userEvent.clear(input);
    await userEvent.type(input, '-10');
    fireEvent.click(button);

    await waitFor(() => {
      expect(
        screen.getByText(/Смещение должно быть положительным/)
      ).toBeInTheDocument();
    });
  });

  test('должен очищать ошибки при изменении значения', async () => {
    const { rerender } = render(<ParallelOffsetCalculator />);
    const input = screen.getByLabelText(/Внешний диаметр/);
    const button = screen.getByRole('button', { name: /Рассчитать/ });

    // Вызвать ошибку
    await userEvent.clear(input);
    await userEvent.type(input, '2');
    fireEvent.click(button);

    await waitFor(() => {
      expect(screen.getByText(/Диаметр должен быть в диапазоне/)).toBeInTheDocument();
    });

    // Изменить значение и ошибка должна исчезнуть
    await userEvent.clear(input);
    await userEvent.type(input, '25');

    expect(screen.queryByText(/Диаметр должен быть в диапазоне/)).not.toBeInTheDocument();
  });

  // ======================== ТЕСТЫ CALCULATE КНОПКИ ========================

  test('должен отключить кнопку при ошибках валидации', async () => {
    render(<ParallelOffsetCalculator />);
    const input = screen.getByLabelText(/Внешний диаметр/);
    const button = screen.getByRole('button', { name: /Рассчитать/ });

    await userEvent.clear(input);
    await userEvent.type(input, '2');
    fireEvent.click(button);

    await waitFor(() => {
      expect(button).toBeDisabled();
    });
  });

  test('должен вызвать API при клике на Calculate', async () => {
    (global.fetch as jest.Mock).mockResolvedValueOnce({
      ok: true,
      json: async () => ({
        total_length: 100,
        bend_1_length: 20,
        straight_section: 60,
        bend_2_length: 20,
        bend_angles: [90, 90],
        bend_1_pos: [0, 0, 0],
        bend_2_pos: [80, 30, 5],
        confidence: 95,
        warnings: '',
        is_valid: true,
      }),
    });

    render(<ParallelOffsetCalculator />);
    const button = screen.getByRole('button', { name: /Рассчитать/ });

    fireEvent.click(button);

    await waitFor(() => {
      expect(global.fetch).toHaveBeenCalledWith(
        '/api/bendcore/parallel-offset/calculate',
        expect.any(Object)
      );
    });
  });

  test('должен обработать ошибку API', async () => {
    (global.fetch as jest.Mock).mockResolvedValueOnce({
      ok: false,
      status: 500,
    });

    render(<ParallelOffsetCalculator />);
    const button = screen.getByRole('button', { name: /Рассчитать/ });

    fireEvent.click(button);

    await waitFor(() => {
      expect(screen.getByText(/Ошибка при обращении к серверу/)).toBeInTheDocument();
    });
  });

  // ======================== ТЕСТЫ ОТОБРАЖЕНИЯ РЕЗУЛЬТАТОВ ========================

  test('должен отображать результаты после успешного расчёта', async () => {
    (global.fetch as jest.Mock).mockResolvedValueOnce({
      ok: true,
      json: async () => ({
        total_length: 100.5,
        bend_1_length: 20.3,
        straight_section: 60.2,
        bend_2_length: 20.0,
        bend_angles: [90, 90],
        bend_1_pos: [0, 0, 0],
        bend_2_pos: [80, 30, 5],
        confidence: 95.2,
        warnings: '',
        is_valid: true,
      }),
    });

    render(<ParallelOffsetCalculator />);
    const button = screen.getByRole('button', { name: /Рассчитать/ });

    fireEvent.click(button);

    await waitFor(() => {
      expect(screen.getByText(/Результаты расчёта/)).toBeInTheDocument();
      expect(screen.getByText(/100.50/)).toBeInTheDocument(); // total_length
    });
  });

  test('должен отображать все параметры результата', async () => {
    (global.fetch as jest.Mock).mockResolvedValueOnce({
      ok: true,
      json: async () => ({
        total_length: 100,
        bend_1_length: 20,
        straight_section: 60,
        bend_2_length: 20,
        bend_angles: [90, 90],
        bend_1_pos: [0, 0, 0],
        bend_2_pos: [80, 30, 5],
        confidence: 95,
        warnings: 'Test warning',
        is_valid: true,
      }),
    });

    render(<ParallelOffsetCalculator />);
    const button = screen.getByRole('button', { name: /Рассчитать/ });

    fireEvent.click(button);

    await waitFor(() => {
      expect(screen.getByText(/Общая длина трубы/)).toBeInTheDocument();
      expect(screen.getByText(/Длина первой гибки/)).toBeInTheDocument();
      expect(screen.getByText(/Прямой участок/)).toBeInTheDocument();
      expect(screen.getByText(/Длина второй гибки/)).toBeInTheDocument();
      expect(screen.getByText(/Уверенность расчёта/)).toBeInTheDocument();
    });
  });

  test('должен отображать предупреждения если есть', async () => {
    (global.fetch as jest.Mock).mockResolvedValueOnce({
      ok: true,
      json: async () => ({
        total_length: 100,
        bend_1_length: 20,
        straight_section: 60,
        bend_2_length: 20,
        bend_angles: [90, 90],
        bend_1_pos: [0, 0, 0],
        bend_2_pos: [80, 30, 5],
        confidence: 80,
        warnings: 'Радиус гибки близок к минимально допустимому',
        is_valid: true,
      }),
    });

    render(<ParallelOffsetCalculator />);
    const button = screen.getByRole('button', { name: /Рассчитать/ });

    fireEvent.click(button);

    await waitFor(() => {
      expect(screen.getByText(/Предупреждения/)).toBeInTheDocument();
      expect(
        screen.getByText(/Радиус гибки близок к минимально допустимому/)
      ).toBeInTheDocument();
    });
  });

  // ======================== ТЕСТЫ ТЁМНОГО РЕЖИМА ========================

  test('должен поддерживать тёмный режим', async () => {
    const { container } = render(<ParallelOffsetCalculator />);
    const toggle = screen.getByLabelText(/Тёмный режим|Светлый режим/);

    expect(container.querySelector('.light-mode')).toBeInTheDocument();

    fireEvent.click(toggle);

    await waitFor(() => {
      expect(container.querySelector('.dark-mode')).toBeInTheDocument();
    });
  });

  // ======================== ТЕСТЫ ГОРЯЧИХ КЛАВИШ ========================

  test('должен выполнить расчёт при нажатии Enter', async () => {
    (global.fetch as jest.Mock).mockResolvedValueOnce({
      ok: true,
      json: async () => ({
        total_length: 100,
        bend_1_length: 20,
        straight_section: 60,
        bend_2_length: 20,
        bend_angles: [90, 90],
        bend_1_pos: [0, 0, 0],
        bend_2_pos: [80, 30, 5],
        confidence: 95,
        warnings: '',
        is_valid: true,
      }),
    });

    render(<ParallelOffsetCalculator />);

    fireEvent.keyDown(window, { key: 'Enter' });

    await waitFor(() => {
      expect(global.fetch).toHaveBeenCalled();
    });
  });

  // ======================== ТЕСТЫ ДОСТУПНОСТИ ========================

  test('должен иметь правильные ARIA labels', () => {
    render(<ParallelOffsetCalculator />);

    const odInput = screen.getByLabelText(/Внешний диаметр трубы/);
    expect(odInput).toHaveAttribute('aria-label');

    const angleInput = screen.getByLabelText(/Угол гибки/);
    expect(angleInput).toHaveAttribute('aria-label');
  });

  test('должен отмечать ошибочные поля с aria-invalid', async () => {
    render(<ParallelOffsetCalculator />);
    const input = screen.getByLabelText(/Внешний диаметр/);
    const button = screen.getByRole('button', { name: /Рассчитать/ });

    await userEvent.clear(input);
    await userEvent.type(input, '2');
    fireEvent.click(button);

    await waitFor(() => {
      expect(input).toHaveAttribute('aria-invalid', 'true');
    });
  });

  // ======================== ТЕСТЫ ЭКСПОРТА ========================

  test('должен отображать элементы экспорта после расчёта', async () => {
    (global.fetch as jest.Mock).mockResolvedValueOnce({
      ok: true,
      json: async () => ({
        total_length: 100,
        bend_1_length: 20,
        straight_section: 60,
        bend_2_length: 20,
        bend_angles: [90, 90],
        bend_1_pos: [0, 0, 0],
        bend_2_pos: [80, 30, 5],
        confidence: 95,
        warnings: '',
        is_valid: true,
      }),
    });

    render(<ParallelOffsetCalculator />);
    const button = screen.getByRole('button', { name: /Рассчитать/ });

    fireEvent.click(button);

    await waitFor(() => {
      expect(screen.getByText(/Экспорт и сохранение/)).toBeInTheDocument();
      expect(screen.getByText(/Экспортировать/)).toBeInTheDocument();
    });
  });

  test('должен иметь select для формата экспорта', async () => {
    (global.fetch as jest.Mock).mockResolvedValueOnce({
      ok: true,
      json: async () => ({
        total_length: 100,
        bend_1_length: 20,
        straight_section: 60,
        bend_2_length: 20,
        bend_angles: [90, 90],
        bend_1_pos: [0, 0, 0],
        bend_2_pos: [80, 30, 5],
        confidence: 95,
        warnings: '',
        is_valid: true,
      }),
    });

    render(<ParallelOffsetCalculator />);
    const button = screen.getByRole('button', { name: /Рассчитать/ });

    fireEvent.click(button);

    await waitFor(() => {
      const select = screen.getByLabelText(/Формат экспорта/);
      expect(select).toBeInTheDocument();
    });
  });

  // ======================== ТЕСТЫ ШАБЛОНОВ ========================

  test('должен иметь поле для сохранения как шаблон', async () => {
    (global.fetch as jest.Mock).mockResolvedValueOnce({
      ok: true,
      json: async () => ({
        total_length: 100,
        bend_1_length: 20,
        straight_section: 60,
        bend_2_length: 20,
        bend_angles: [90, 90],
        bend_1_pos: [0, 0, 0],
        bend_2_pos: [80, 30, 5],
        confidence: 95,
        warnings: '',
        is_valid: true,
      }),
    });

    render(<ParallelOffsetCalculator />);
    const button = screen.getByRole('button', { name: /Рассчитать/ });

    fireEvent.click(button);

    await waitFor(() => {
      expect(screen.getByPlaceholderText(/Введите имя шаблона/)).toBeInTheDocument();
    });
  });

  // ======================== ТЕСТЫ НАЧАЛЬНЫХ ПАРАМЕТРОВ ========================

  test('должен использовать initialParams если предоставлены', () => {
    render(
      <ParallelOffsetCalculator
        initialParams={{
          outer_diameter: 30,
          bend_angle: 45,
          material: 'Aluminum',
        }}
      />
    );

    const odInput = screen.getByLabelText(/Внешний диаметр/) as HTMLInputElement;
    expect(odInput.value).toBe('30');

    const materialSelect = screen.getByLabelText(/Материал/) as HTMLSelectElement;
    expect(materialSelect.value).toBe('Aluminum');
  });

  // ======================== ТЕСТЫ CALLBACKS ========================

  test('должен вызвать onResultsUpdate callback при успешном расчёте', async () => {
    const mockCallback = jest.fn();
    (global.fetch as jest.Mock).mockResolvedValueOnce({
      ok: true,
      json: async () => ({
        total_length: 100,
        bend_1_length: 20,
        straight_section: 60,
        bend_2_length: 20,
        bend_angles: [90, 90],
        bend_1_pos: [0, 0, 0],
        bend_2_pos: [80, 30, 5],
        confidence: 95,
        warnings: '',
        is_valid: true,
      }),
    });

    render(<ParallelOffsetCalculator onResultsUpdate={mockCallback} />);
    const button = screen.getByRole('button', { name: /Рассчитать/ });

    fireEvent.click(button);

    await waitFor(() => {
      expect(mockCallback).toHaveBeenCalled();
    });
  });
});
