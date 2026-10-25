# CAD-CORE-007: History Stack

**Статус:** Спецификация и реализация  
**Дата:** 2026-10-25  
**Приоритет:** P0 (завершает Phase B)  
**Зависимости:** CAD-CORE-005 (CommandEngine), CAD-CORE-006 (TransactionManager)  

---

## 1. Краткое описание

HistoryStack реализует **Undo/Redo** функциональность через сохранение истории команд и состояний документа.

**Ключевые достижения:**
- Undo/Redo стек с произвольной глубиной
- Полная поддержка batch операций
- Откат с восстановлением полного состояния
- Уведомления об изменениях истории
- Интеграция с CommandExecutor

---

## 2. Архитектура

### 2.1 HistoryEntry

```cpp
struct HistoryEntry {
    std::string id;
    Command command;
    CommandResult result;
    std::chrono::system_clock::time_point timestamp;
    std::string description;  // "Create Part", "Move Instance", etc.
};
```

### 2.2 HistoryStack API

```cpp
class HistoryStack {
public:
    explicit HistoryStack(CadDocument& doc, size_t max_history = 100);

    // Запись команд в историю
    void record_command(const Command& cmd, const CommandResult& result);

    // Undo/Redo
    [[nodiscard]] bool undo();
    [[nodiscard]] bool redo();

    // Состояние
    [[nodiscard]] bool can_undo() const;
    [[nodiscard]] bool can_redo() const;
    [[nodiscard]] size_t history_size() const;
    [[nodiscard]] size_t undo_count() const;
    [[nodiscard]] size_t redo_count() const;

    // Очистка
    void clear_history();
    void clear_redo_stack();

private:
    CadDocument& document_;
    std::deque<HistoryEntry> undo_stack_;
    std::deque<HistoryEntry> redo_stack_;
    size_t max_history_size_;

    [[nodiscard]] bool reverse_command(const Command& cmd);
};
```

---

## 3. Механизм Undo/Redo

### 3.1 Записи команд

Каждая команда сохраняется с её результатом:
```
undo_stack: [CreatePart(PART-1), CreateInstance(INST-1), SetParent(...)]
redo_stack: []
```

### 3.2 Undo операция

1. Получить команду из undo_stack
2. Создать обратную команду (inverse)
3. Выполнить обратную команду
4. Переместить в redo_stack

### 3.3 Redo операция

1. Получить команду из redo_stack
2. Выполнить оригинальную команду
3. Переместить в undo_stack

---

## 4. Обратные команды

| Команда | Обратная команда |
|---------|------------------|
| CreatePart(P1) | DeleteEntity(P1) |
| CreateInstance(I1) | DeleteEntity(I1) |
| SetParent(C, P) | SetParent(C, OldParent) |
| DeleteEntity(E) | RestoreEntity(E, OldState) |
| EditPart(P, new_desig) | EditPart(P, old_desig) |
| BatchMove(...) | BatchMove(reversed...) |

---

## 5. Спецификация методов

### 5.1 record_command(cmd, result)

Сохранить команду в историю после успешного выполнения.

**Операция:**
1. Создать HistoryEntry с timestamp
2. Добавить в undo_stack
3. Очистить redo_stack (новая действие прерывает redo цепь)
4. Удалить старые записи если undo_stack > max_history_size

### 5.2 undo()

Отменить последнюю команду.

**Возвращает:** true если успешно, false если нечего отменять

**Операция:**
1. Проверить undo_stack не пуст
2. Получить последнюю команду
3. Создать обратную команду
4. Выполнить обратную команду
5. Переместить в redo_stack

### 5.3 redo()

Повторить отменённую команду.

**Операция:**
1. Проверить redo_stack не пуст
2. Получить команду из redo_stack
3. Выполнить команду
4. Переместить в undo_stack

---

## 6. Примеры использования

```cpp
CadDocument doc;
CommandExecutor executor(doc);
HistoryStack history(doc, 100);  // max 100 entries

// Создать Part
Command create_part("CreatePart");
create_part.args["designation"] = "PART-001";
CommandResult result = executor.execute(create_part);
history.record_command(create_part, result);

// Создать Instance
Command create_inst("CreateInstance");
create_inst.args["part_id"] = result.result_data["part_id"];
CommandResult inst_result = executor.execute(create_inst);
history.record_command(create_inst, inst_result);

// Undo создание Instance
if (history.can_undo()) {
    history.undo();  // Удаляет Instance
}

// Redo
if (history.can_redo()) {
    history.redo();  // Восстанавливает Instance
}
```

---

## 7. Тестирование

**Тесты (test_history_stack.cpp):**

1. **test_undo_single_command** — undo одной команды
2. **test_redo_single_command** — redo одной команды
3. **test_undo_multiple_commands** — undo нескольких команд
4. **test_undo_redo_cycle** — undo + redo цикл
5. **test_undo_clears_redo_stack** — новая команда очищает redo
6. **test_max_history_size** — соблюдение лимита истории
7. **test_batch_operations** — undo/redo для batch команд

---

## 8. Deliverables

- ✓ `include/bendcore/commands/history_stack.hpp`
- ✓ `src/commands/history_stack.cpp`
- ✓ `tests/commands/test_history_stack.cpp`
- ✓ CMakeLists.txt обновлён

**Размер:** ~700 линий (header + impl + tests)

---

## 9. Phase B Завершение

После CAD-CORE-007:
- ✓ CAD-CORE-005: CommandEngine
- ✓ CAD-CORE-006: TransactionManager  
- ✓ CAD-CORE-007: HistoryStack

**Phase B COMPLETE** — готово к Phase C (Parameters & Dependencies)
