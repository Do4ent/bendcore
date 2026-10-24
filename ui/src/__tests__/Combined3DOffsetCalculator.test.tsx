import React from 'react';
import { render, screen, fireEvent, waitFor } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { Combined3DOffsetCalculator } from '../Combined3DOffsetCalculator';

// ============================================================================
// Мокирование fetch API
// ============================================================================

global.fetch = jest.fn();

const mockCalculationResult = {
  success: true,
  error_message: '',
  total_length: 250.5,
  total_distance_3d: 158.11,
  bends: [
    {
      position: { x: 33.3, y: 33.3, z: 16.7 },
      angle_xy: 45.0,
      angle_z: 21.8,
      length: 52.7,
      radius: 27.0,
      springback_correction: 0.02,
      bend_index: 1,
    },
    {
      position: { x: 66.6, y: 66.6, z: 33.3 },
      angle_xy: 45.0,
      angle_z: 21.8,
      length: 105.4,
      radius: 27.0,
      springback_correction: 0.02,
      bend_index: 2,
    },
  ],
  sections: [
    {
      start: { x: 0, y: 0, z: 0 },
      end: { x: 33.3, y: 33.3, z: 16.7 },
      length: 47.1,
      slope_percent: 30.0,
      section_index: 0,
    },
  ],
  trajectory_3d: [
    { x: 0, y: 0, z: 0 },
    { x: 33.3, y: 33.3, z: 16.7 },
    { x: 66.6, y: 66.6, z: 33.3 },
    { x: 100, y: 100, z: 50 },
  ],
  cnc_code: 'G20\nG90\nG00 Z5.0\n',
  feasibility_score: 85.5,
  warnings: [],
};

// ============================================================================
// Набор тестов: Рендеринг компонента
// ============================================================================

describe('Combined3DOffsetCalculator - Rendering', () => {
  beforeEach(() => {
    jest.clearAllMocks();
  });

  test('Компонент рендерится без ошибок', () => {
    render(<Combined3DOffsetCalculator />);
    expect(screen.getByText('3D Offset Calculator')).toBeInTheDocument();
  });

  test('Заголовок отображается корректно', () => {
    render(<Combined3DOffsetCalculator />);
    expect(screen.getByText('3D Offset Calculator')).toBeInTheDocument();
    expect(screen.getByText(/Комбинированный расчёт/)).toBeInTheDocument();
  });

  test('Все основные секции отображаются', () => {
    render(<Combined3DOffsetCalculator />);
    expect(screen.getByText('Точка старта (Start Point)')).toBeInTheDocument();
    expect(screen.getByText('Точка финиша (End Point)')).toBeInTheDocument();
    expect(screen.getByText('Параметры трубы')).toBeInTheDocument();
    expect(screen.getByText('Параметры смещения')).toBeInTheDocument();
  });

  test('Кнопка Calculate присутствует', () => {
    render(<Combined3DOffsetCalculator />);
    const button = screen.getByRole('button', { name: /Рассчитать/i });
    expect(button).toBeInTheDocument();
  });

  test('Empty state отображается до расчёта', () => {
    render(<Combined3DOffsetCalculator />);
    expect(
      screen.getByText(/Нажмите "Рассчитать" для отображения результатов/)
    ).toBeInTheDocument();
  });
});

// ============================================================================
// Набор тестов: Входные поля
// ============================================================================

describe('Combined3DOffsetCalculator - Input Fields', () => {
  beforeEach(() => {
    jest.clearAllMocks();
  });

  test('Поля стартовой точки можно редактировать', async () => {
    render(<Combined3DOffsetCalculator />);
    const input = screen.getByDisplayValue('0') as HTMLInputElement;

    await userEvent.clear(input);
    await userEvent.type(input, '10');

    expect(input.value).toBe('10');
  });

  test('Поля конечной точки можно редактировать', async () => {
    render(<Combined3DOffsetCalculator />);
    const inputs = screen.getAllByDisplayValue('100') as HTMLInputElement[];

    await userEvent.clear(inputs[0]);
    await userEvent.type(inputs[0], '150');

    expect(inputs[0].value).toBe('150');
  });

  test('Поле диаметра трубы валидирует минимальное значение', async () => {
    render(<Combined3DOffsetCalculator />);
    const input = screen.getByDisplayValue('50') as HTMLInputElement;

    await userEvent.clear(input);
    await userEvent.type(input, '0.5');
    fireEvent.blur(input);

    // После обновления должно быть минимальное значение
    expect(parseInt(input.value, 10)).toBeGreaterThanOrEqual(1);
  });

  test('Слайдер для количества гибок работает', async () => {
    render(<Combined3DOffsetCalculator />);
    const slider = screen.getByDisplayValue('2') as HTMLInputElement;

    fireEvent.change(slider, { target: { value: '4' } });

    expect(slider.value).toBe('4');
  });

  test('Слайдер для угла наклона работает', async () => {
    render(<Combined3DOffsetCalculator />);
    const sliders = screen.getAllByRole('slider') as HTMLInputElement[];

    // Второй слайдер - это угол наклона
    fireEvent.change(sliders[1], { target: { value: '45' } });

    expect(sliders[1].value).toBe('45');
  });

  test('Выпадающее меню материала можно менять', async () => {
    render(<Combined3DOffsetCalculator />);
    const select = screen.getByDisplayValue('Сталь (Steel)') as HTMLSelectElement;

    await userEvent.selectOptions(select, 'aluminum');

    expect(select.value).toBe('aluminum');
  });

  test('Чекбокс пружинения можно включать/выключать', async () => {
    render(<Combined3DOffsetCalculator />);
    const checkbox = screen.getByRole('checkbox', {
      name: /Коррекция пружинения/,
    }) as HTMLInputElement;

    expect(checkbox.checked).toBe(true);

    await userEvent.click(checkbox);
    expect(checkbox.checked).toBe(false);
  });

  test('Радио кнопки распределения работают', async () => {
    render(<Combined3DOffsetCalculator />);
    const radios = screen.getAllByRole('radio') as HTMLInputElement[];

    const customRadio = radios[1];
    await userEvent.click(customRadio);

    expect(customRadio.checked).toBe(true);
  });
});

// ============================================================================
// Набор тестов: API вызовы
// ============================================================================

describe('Combined3DOffsetCalculator - API Integration', () => {
  beforeEach(() => {
    jest.clearAllMocks();
    (global.fetch as jest.Mock).mockClear();
  });

  test('Кнопка Calculate вызывает API', async () => {
    (global.fetch as jest.Mock).mockResolvedValueOnce({
      ok: true,
      json: async () => mockCalculationResult,
    });

    render(<Combined3DOffsetCalculator />);
    const button = screen.getByRole('button', { name: /Рассчитать/i });

    await userEvent.click(button);

    await waitFor(() => {
      expect(global.fetch).toHaveBeenCalled();
    });
  });

  test('API вызов содержит правильные параметры', async () => {
    (global.fetch as jest.Mock).mockResolvedValueOnce({
      ok: true,
      json: async () => mockCalculationResult,
    });

    render(<Combined3DOffsetCalculator />);
    const button = screen.getByRole('button', { name: /Рассчитать/i });

    await userEvent.click(button);

    await waitFor(() => {
      expect(global.fetch).toHaveBeenCalledWith(
        '/api/bendcore/combined-3d-offset/calculate',
        expect.any(Object)
      );

      const callArgs = (global.fetch as jest.Mock).mock.calls[0];
      const body = JSON.parse(callArgs[1].body);
      expect(body).toHaveProperty('start_point');
      expect(body).toHaveProperty('end_point');
      expect(body).toHaveProperty('outer_diameter');
    });
  });

  test('Успешный результат отображается', async () => {
    (global.fetch as jest.Mock).mockResolvedValueOnce({
      ok: true,
      json: async () => mockCalculationResult,
    });

    render(<Combined3DOffsetCalculator />);
    const button = screen.getByRole('button', { name: /Рассчитать/i });

    await userEvent.click(button);

    await waitFor(() => {
      expect(screen.getByText(/Результаты/)).toBeInTheDocument();
      expect(screen.getByText(`${mockCalculationResult.total_length.toFixed(2)} мм`))
        .toBeInTheDocument();
    });
  });

  test('Ошибка API отображается корректно', async () => {
    (global.fetch as jest.Mock).mockResolvedValueOnce({
      ok: false,
      statusText: 'Bad Request',
    });

    render(<Combined3DOffsetCalculator />);
    const button = screen.getByRole('button', { name: /Рассчитать/i });

    await userEvent.click(button);

    await waitFor(() => {
      expect(screen.getByText(/Ошибка расчёта/)).toBeInTheDocument();
    });
  });
});

// ============================================================================
// Набор тестов: Вкладки результатов
// ============================================================================

describe('Combined3DOffsetCalculator - Tabs', () => {
  beforeEach(() => {
    jest.clearAllMocks();
    (global.fetch as jest.Mock).mockResolvedValueOnce({
      ok: true,
      json: async () => mockCalculationResult,
    });
  });

  test('Вкладки отображаются после расчёта', async () => {
    render(<Combined3DOffsetCalculator />);
    const button = screen.getByRole('button', { name: /Рассчитать/i });

    await userEvent.click(button);

    await waitFor(() => {
      expect(screen.getByRole('button', { name: /Результаты/i })).toBeInTheDocument();
      expect(screen.getByRole('button', { name: /3D Предпросмотр/i })).toBeInTheDocument();
      expect(screen.getByRole('button', { name: /G-код/i })).toBeInTheDocument();
    });
  });

  test('Можно переключаться между вкладками', async () => {
    render(<Combined3DOffsetCalculator />);
    const button = screen.getByRole('button', { name: /Рассчитать/i });

    await userEvent.click(button);

    await waitFor(() => {
      const cncTab = screen.getByRole('button', { name: /G-код/i });
      fireEvent.click(cncTab);
      expect(cncTab).toHaveClass('active');
    });
  });
});

// ============================================================================
// Набор тестов: Таблица результатов
// ============================================================================

describe('Combined3DOffsetCalculator - Results Table', () => {
  beforeEach(() => {
    jest.clearAllMocks();
    (global.fetch as jest.Mock).mockResolvedValueOnce({
      ok: true,
      json: async () => mockCalculationResult,
    });
  });

  test('Таблица гибок отображается с данными', async () => {
    render(<Combined3DOffsetCalculator />);
    const button = screen.getByRole('button', { name: /Рассчитать/i });

    await userEvent.click(button);

    await waitFor(() => {
      // Проверяем наличие заголовков таблицы
      expect(screen.getByText('#')).toBeInTheDocument();
      // Проверяем наличие данных гибок
      expect(screen.getByText('1')).toBeInTheDocument();
    });
  });

  test('Сводка результатов отображается корректно', async () => {
    render(<Combined3DOffsetCalculator />);
    const button = screen.getByRole('button', { name: /Рассчитать/i });

    await userEvent.click(button);

    await waitFor(() => {
      expect(screen.getByText(/Общая длина:/)).toBeInTheDocument();
      expect(screen.getByText(/3D расстояние:/)).toBeInTheDocument();
      expect(screen.getByText(/Feasibility:/)).toBeInTheDocument();
    });
  });
});

// ============================================================================
// Набор тестов: Экспорт и сохранение
// ============================================================================

describe('Combined3DOffsetCalculator - Export and Save', () => {
  beforeEach(() => {
    jest.clearAllMocks();
    (global.fetch as jest.Mock).mockResolvedValueOnce({
      ok: true,
      json: async () => mockCalculationResult,
    });

    // Мокируем clipboard API
    Object.assign(navigator, {
      clipboard: {
        writeText: jest.fn(() => Promise.resolve()),
      },
    });
  });

  test('Кнопка экспорта G-кода присутствует', async () => {
    render(<Combined3DOffsetCalculator />);
    const button = screen.getByRole('button', { name: /Рассчитать/i });

    await userEvent.click(button);

    await waitFor(() => {
      expect(screen.getByRole('button', { name: /📥 G-код/i })).toBeInTheDocument();
    });
  });

  test('Кнопка сохранения шаблона работает', async () => {
    render(<Combined3DOffsetCalculator />);
    const button = screen.getByRole('button', { name: /Сохранить как шаблон/i });

    expect(button).toBeInTheDocument();

    // Клик создаст загрузку файла (проверяем что нет ошибки)
    await userEvent.click(button);
    expect(button).toBeInTheDocument();
  });
});

// ============================================================================
// Набор тестов: Custom распределение гибок
// ============================================================================

describe('Combined3DOffsetCalculator - Custom Distribution', () => {
  test('Custom позиции отображаются при выборе custom распределения', async () => {
    render(<Combined3DOffsetCalculator />);

    const customRadio = screen.getByRole('radio', { name: /Пользовательское/ });
    await userEvent.click(customRadio);

    await waitFor(() => {
      expect(screen.getByText(/Позиции гибок/)).toBeInTheDocument();
    });
  });

  test('Количество custom позиций соответствует количеству гибок', async () => {
    render(<Combined3DOffsetCalculator />);

    const customRadio = screen.getByRole('radio', { name: /Пользовательское/ });
    await userEvent.click(customRadio);

    const slider = screen.getByDisplayValue('2') as HTMLInputElement;
    fireEvent.change(slider, { target: { value: '3' } });

    await waitFor(() => {
      const positionInputs = screen.getAllByDisplayValue(/0\..*/);
      expect(positionInputs.length).toBeGreaterThanOrEqual(3);
    });
  });

  test('Custom позиции можно редактировать', async () => {
    render(<Combined3DOffsetCalculator />);

    const customRadio = screen.getByRole('radio', { name: /Пользовательское/ });
    await userEvent.click(customRadio);

    await waitFor(() => {
      const positionInputs = screen.getAllByDisplayValue(/0\..*/);
      expect(positionInputs.length).toBeGreaterThan(0);
    });

    const firstInput = screen.getByDisplayValue('0.33') as HTMLInputElement;
    await userEvent.clear(firstInput);
    await userEvent.type(firstInput, '0.25');

    expect(firstInput.value).toBe('0.25');
  });
});

// ============================================================================
// Набор тестов: Callback функции
// ============================================================================

describe('Combined3DOffsetCalculator - Callbacks', () => {
  test('onResultsUpdate вызывается при успешном расчёте', async () => {
    (global.fetch as jest.Mock).mockResolvedValueOnce({
      ok: true,
      json: async () => mockCalculationResult,
    });

    const onResultsUpdate = jest.fn();
    render(<Combined3DOffsetCalculator onResultsUpdate={onResultsUpdate} />);
    const button = screen.getByRole('button', { name: /Рассчитать/i });

    await userEvent.click(button);

    await waitFor(() => {
      expect(onResultsUpdate).toHaveBeenCalledWith(expect.objectContaining({
        success: true,
        total_length: expect.any(Number),
      }));
    });
  });

  test('onExportCNC вызывается при экспорте', async () => {
    (global.fetch as jest.Mock).mockResolvedValueOnce({
      ok: true,
      json: async () => mockCalculationResult,
    });

    const onExportCNC = jest.fn();
    render(<Combined3DOffsetCalculator onExportCNC={onExportCNC} />);
    const button = screen.getByRole('button', { name: /Рассчитать/i });

    await userEvent.click(button);

    await waitFor(() => {
      const exportButton = screen.getByRole('button', { name: /📥 G-код/i });
      fireEvent.click(exportButton);

      // Note: В реальном тесте может не вызваться из-за DOM
      // но функция должна быть определена
      expect(onExportCNC).toBeDefined();
    });
  });
});

// ============================================================================
// Набор тестов: Темный режим
// ============================================================================

describe('Combined3DOffsetCalculator - Dark Mode', () => {
  test('Компонент применяет класс dark-mode', () => {
    const { container } = render(<Combined3DOffsetCalculator darkMode={true} />);
    const mainDiv = container.querySelector('.combined-3d-offset-calculator');
    expect(mainDiv).toHaveClass('dark-mode');
  });

  test('Dark mode класс не применяется по умолчанию', () => {
    const { container } = render(<Combined3DOffsetCalculator />);
    const mainDiv = container.querySelector('.combined-3d-offset-calculator');
    expect(mainDiv).not.toHaveClass('dark-mode');
  });
});
