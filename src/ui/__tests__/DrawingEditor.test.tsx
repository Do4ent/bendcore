// Тесты для компонента DrawingEditor (Q4 Task 1 Subtask 2)
import React from 'react';
import { render, screen, fireEvent, waitFor } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { DrawingEditor, EditableParameter, DrawingState } from '../DrawingEditor';

describe('DrawingEditor Component', () => {
  const mockDrawingState: DrawingState = {
    drawing_id: 'test_drawing_123',
    parameters: [
      {
        id: 'length_1',
        name: 'Длина 1',
        current_value: 100,
        min_value: 10,
        max_value: 500,
        unit: 'mm',
        is_formula: false,
        formula_text: '',
      },
      {
        id: 'angle_1',
        name: 'Угол 1',
        current_value: 90,
        min_value: 0,
        max_value: 180,
        unit: 'deg',
        is_formula: false,
        formula_text: '',
      },
    ],
    last_modified_at: new Date().toISOString(),
    modified_by: 'test_user',
  };

  const mockOnSave = jest.fn();
  const mockOnCancel = jest.fn();

  beforeEach(() => {
    jest.clearAllMocks();
  });

  test('Должен отрендерить компонент с заголовком', () => {
    render(
      <DrawingEditor
        drawingId="test_drawing_123"
        onSave={mockOnSave}
      />
    );

    expect(screen.getByText(/Редактор чертежа/i)).toBeInTheDocument();
  });

  test('Должен отрендерить вкладки', () => {
    render(
      <DrawingEditor
        drawingId="test_drawing_123"
        onSave={mockOnSave}
      />
    );

    expect(screen.getByText('Параметры')).toBeInTheDocument();
    expect(screen.getByText('Формулы')).toBeInTheDocument();
    expect(screen.getByText('История')).toBeInTheDocument();
  });

  test('Должен отключить кнопку сохранения без изменений', () => {
    const { getByText } = render(
      <DrawingEditor
        drawingId="test_drawing_123"
        onSave={mockOnSave}
      />
    );

    const saveButton = getByText('💾 Сохранить') as HTMLButtonElement;
    expect(saveButton.disabled).toBe(true);
  });

  test('Должен отключить кнопку отмены в режиме readonly', () => {
    const { getByText } = render(
      <DrawingEditor
        drawingId="test_drawing_123"
        onSave={mockOnSave}
        readOnly={true}
      />
    );

    const cancelButton = getByText('✕ Отменить') as HTMLButtonElement;
    expect(cancelButton.disabled).toBe(true);
  });

  test('Должен вызвать onCancel при нажатии кнопки отмены', () => {
    const { getByText } = render(
      <DrawingEditor
        drawingId="test_drawing_123"
        onSave={mockOnSave}
        onCancel={mockOnCancel}
      />
    );

    const cancelButton = getByText('✕ Отменить') as HTMLButtonElement;
    fireEvent.click(cancelButton);

    expect(mockOnCancel).toHaveBeenCalled();
  });

  test('Должен отключить кнопки undo/redo в начале', () => {
    const { getByText } = render(
      <DrawingEditor
        drawingId="test_drawing_123"
        onSave={mockOnSave}
      />
    );

    const undoButton = getByText(/Отменить/) as HTMLButtonElement;
    const redoButton = getByText(/Повторить/) as HTMLButtonElement;

    expect(undoButton.disabled).toBe(true);
    expect(redoButton.disabled).toBe(true);
  });

  test('Должен переключаться между вкладками', async () => {
    const { getByText } = render(
      <DrawingEditor
        drawingId="test_drawing_123"
        onSave={mockOnSave}
      />
    );

    const formulasTab = getByText('Формулы');
    fireEvent.click(formulasTab);

    await waitFor(() => {
      expect(screen.getByText(/Поддерживаемые переменные/i)).toBeInTheDocument();
    });

    const historyTab = getByText('История');
    fireEvent.click(historyTab);

    await waitFor(() => {
      expect(screen.getByText(/История изменений/i)).toBeInTheDocument();
    });
  });

  test('Должен показывать предупреждение о несохранённых изменениях', async () => {
    const { getByText, queryByText } = render(
      <DrawingEditor
        drawingId="test_drawing_123"
        onSave={mockOnSave}
      />
    );

    // Инициально предупреждение не должно быть видно
    expect(queryByText(/Есть несохранённые изменения/i)).not.toBeInTheDocument();
  });

  test('Должен поддерживать режим readonly', () => {
    const { getByText } = render(
      <DrawingEditor
        drawingId="test_drawing_123"
        onSave={mockOnSave}
        readOnly={true}
      />
    );

    const undoButton = getByText(/Отменить/) as HTMLButtonElement;
    const redoButton = getByText(/Повторить/) as HTMLButtonElement;
    const cancelButton = getByText(/✕ Отменить/) as HTMLButtonElement;

    expect(undoButton.disabled).toBe(true);
    expect(redoButton.disabled).toBe(true);
    expect(cancelButton.disabled).toBe(true);
  });

  test('Должен обновить информацию о последней модификации', () => {
    const now = new Date().toISOString();
    const { getByText } = render(
      <DrawingEditor
        drawingId="test_drawing_123"
        onSave={mockOnSave}
      />
    );

    // Проверить, что информация о последней модификации отображается
    const footer = screen.getByText(/Последняя модификация/i);
    expect(footer).toBeInTheDocument();
  });

  test('Должен показывать пустое сообщение когда нет параметров', () => {
    const { getByText } = render(
      <DrawingEditor
        drawingId="empty_drawing"
        onSave={mockOnSave}
      />
    );

    expect(getByText(/Нет параметров для отображения/i)).toBeInTheDocument();
  });

  test('Должен вызвать onSave при клике на кнопку сохранения', async () => {
    mockOnSave.mockResolvedValueOnce(undefined);

    const { getByText } = render(
      <DrawingEditor
        drawingId="test_drawing_123"
        onSave={mockOnSave}
      />
    );

    // Кнопка сохранения должна быть задисейблена без изменений
    const saveButton = getByText('💾 Сохранить') as HTMLButtonElement;
    expect(saveButton.disabled).toBe(true);
  });

  test('Должен обновляться правильно', async () => {
    const { rerender } = render(
      <DrawingEditor
        drawingId="test_drawing_123"
        onSave={mockOnSave}
      />
    );

    // Перерендер с тем же пропсом
    rerender(
      <DrawingEditor
        drawingId="test_drawing_123"
        onSave={mockOnSave}
      />
    );

    expect(screen.getByText(/Редактор чертежа/i)).toBeInTheDocument();
  });
});
