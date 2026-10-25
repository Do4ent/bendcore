# CAD-CORE-001: Data Ownership Audit

**Статус:** Детальный аудит хранения состояния в BendCore  
**Дата:** 2026-10-25  
**Приоритет:** P0 (фундамент архитектуры)  
**Зависимости:** Нет  

---

## 1. Executive Summary

Состояние BendCore **распределено по множеству независимых хранилищ** без единого источника истины.

| Компонент | Владелец | Тип данных | Дублирование |
|-----------|---------|-----------|------------|
| **Экземпляры** | bendcore::assembly::Registry | Part, Instance | ❌ |
| **Геометрия** | geometry_cache + OCCT | TopoDS_Shape | ⚠️ (кэш не синхронизирован) |
| **Параметры** | formula.cpp + local vars | Path, OD, Wall, etc. | ❌ (но разбросаны) |
| **Чертёжные виды** | drawing.cpp + UI state | Geometry, Layouts | ⚠️ (двойная синхронизация) |
| **Позиционирование** | instances.cpp + output | Frame, Placement | ❌ (совпадает) |
| **Формулы & зависимости | formula.cpp | ASTNode, вычисления | ❌ (но нет графа) |
| **UI состояние** | React/Redux (TS) | дерево, выделение | ⚠️ (не синхронизировано с C++) |
| **Сессионное состояние** | core_api.cpp State* | непрозрачное | ❌ (но разрозненное) |

**Проблема:** Нет контракта на владение. Изменение в одном месте не гарантирует обновление в других.

---

## 2. Детальный анализ по компонентам

### 2.1 bendcore::assembly::Registry (instances.hpp)

**Файлы:** 
- `include/bendcore/instances.hpp`
- `src/instances.cpp`
- `tests/test_instances.cpp`

**Владеет:**
```cpp
struct Part {
    std::string id;
    std::string designation;
    TubeSignatureData data;      // ← труба и её параметры
    std::string primary;         // ← главный экземпляр
    std::vector<std::string> instances;
};

struct Instance {
    std::string id;
    std::string part_id;
    output::Frame placement;     // ← позиционирование
    bool adaptive;
    std::string designation;
};

class Registry {
    std::map<std::string, Part> parts_;
    std::map<std::string, Instance> instances_;
    std::vector<std::string> order_;  // ← порядок в дереве
};
```

**Отвечает за:**
- ✓ Идентификацию экземпляров (строки)
- ✓ Иерархию Part ↔ Instance
- ✓ Главный экземпляр (primary)
- ✓ Адаптивность
- ✓ Обозначения
- ✓ Порядок в дереве

**Дублирует или теряет:**
- ⚠️ TubeSignatureData (путь, материал) — потом пересчитывается в других модулях
- ⚠️ Frame (placement) — может рассинхронизироваться с визуализацией в UI

**Проблема:** Registry — "дерево компонентов", но не связан с геометрией, параметрами, чертежами.

---

### 2.2 geometry_cache.cpp + src/occt/

**Файлы:**
- `include/bendcore/geometry_cache.hpp`
- `src/geometry_cache.cpp` (724 строк)
- `src/occt/tube_body_occt.cpp`
- `src/occt/shape_builder.cpp`

**Владеет:**
```cpp
struct CacheEntry {
    std::string shape_data;      // ← сериализованное B-rep
    ExtractedParams params;      // ← OD, Wall, Length
    std::chrono::system_clock::time_point timestamp;
    size_t memory_bytes;
};

class GeometryCache {
    std::map<CacheKey, CacheEntry> cache_;  // ← SHA256(path + mtime + content)
    std::map<CacheKey, AccessTime> access_times_;
    size_t current_memory_bytes_;
};
```

**Отвечает за:**
- ✓ Кэширование B-rep по хешу содержимого
- ✓ LRU вытеснение при переполнении памяти
- ✓ Извлечение параметров (OD, Wall, BendRadius)
- ✓ Валидация целостности

**Дублирует или теряет:**
- ⚠️ ExtractedParams (OD, Wall) — может отличаться от Registry.TubeSignatureData
- ⚠️ TopoDS_Shape — только кэшируется, не отслеживается версионирование

**Проблема:** Кэш не знает о принадлежности к экземпляру или операции. Ключ — только файл.

---

### 2.3 formula.cpp + formula_engine.cpp

**Файлы:**
- `include/bendcore/formula.hpp` (378 строк)
- `src/formula.cpp` (582 строк)
- `include/bendcore/formula_engine.hpp`
- `src/formula_engine.cpp`

**Владеет:**
```cpp
// Параметры трубы:
// - Path (прямые, дуги, спирали)
// - OD (диаметр)
// - Wall (стенка)
// - Материал
// - Инструмент гибки
// - Припуски (A, B)

// Но они разбросаны:
struct TubeSignatureData {  // ← в registry
    Path path;
    double od, wall;
    std::string material, supply_state;
    // ...
};

// И в локальных переменных обработчиков команд:
json create_tube(const json& args) {
    double od = num(args, "od", 10.0);     // ← локальная переменная
    double wall = num(args, "wall", 1.0);
    Path path = path_from_json(args["path"]);
    // ...
}

// AST и вычисления:
class ASTNode {
    // Поддерживает формулы вроде "$OD * 2.0"
    // Но где хранятся значения переменных? ← Нигде централизованно
};
```

**Отвечает за:**
- ✓ Парсинг и вычисление выражений
- ✓ Поддержка переменных ($OD, $Wall, $BendAngle)
- ✓ Функции (sqrt, min, max)

**Дублирует или теряет:**
- ❌ Параметры живут в Registry, в локальных переменных, в UI state
- ❌ Нет единого Parameter Store
- ❌ Нет отслеживания зависимостей между параметрами
- ❌ Нет системы пересчёта при изменении

**Проблема:** Параметры "рассыпаны"; нет подписки на изменения.

---

### 2.4 drawing.cpp + drawing_editor.cpp

**Файлы:**
- `include/bendcore/drawing.hpp` (900+ строк)
- `src/drawing.cpp` (2700+ строк)
- `src/drawing_editor.cpp`
- `include/bendcore/drawing_editor.hpp`

**Владеет:**
```cpp
struct Geometry {
    std::vector<Polyline2> lines;      // ← линии вида
    std::vector<Circle2> circles;
    std::vector<Text2> texts;
    std::vector<Dim2> dims;            // ← размеры
    std::vector<Hatch2> hatches;
    // ... 100+ функций для построения видов
};

class DrawingEditor {
    std::vector<View> views_;          // ← размещённые виды
    std::vector<Dimension> dimensions_;
    std::vector<ManualDimension> manual_dims_;
    Layout layout_;
    // ...
};
```

**Отвечает за:**
- ✓ Построение видов трубы (front, top, unfold)
- ✓ Таблицы гибов, координат вершин
- ✓ Размеры, штриховка
- ✓ Компоновка листа

**Дублирует или теряет:**
- ⚠️ Geometry вычисляется заново каждый раз (нет кэша представлений)
- ⚠️ Параметры (OD, Wall) берут из аргументов, а не из Registry
- ⚠️ Размеры — как в чертеже, так и в UI (двойная синхронизация)

**Проблема:** Чертёж не знает о принадлежности к экземпляру. Параметры передаются через JSON.

---

### 2.5 core_api.cpp (API dispatcher)

**Файлы:**
- `include/bendcore/core_api.hpp` (40 строк)
- `src/core_api.cpp` (1200+ строк)

**Владеет:**
```cpp
class Core {
    struct State;  // ← непрозрачное хранилище
    State* state_;
public:
    json call(const std::string& command, const json& args);
};
```

**Отвечает за:**
- ✓ Диспетчеризацию команд (create_tube, create_instance, etc.)
- ✓ JSON ↔ C++ маршаллинг
- ✓ Управление жизненным циклом

**Дублирует или теряет:**
- ❌ State* непрозрачна — неясно, что там хранится
- ❌ Каждая команда может создавать локальное состояние
- ❌ Нет глобальной истории команд (Undo/Redo)
- ❌ Нет валидации команд до выполнения

**Проблема:** API — это просто диспетчер, а не менеджер состояния документа.

---

### 2.6 React UI State (TypeScript)

**Файлы:**
- `src/ui/core/coreClient.ts`
- `src/ui/core/types.ts`
- `src/ui/SettingsStore.ts`
- `src/features/*/types.ts` (20+ компонентов)

**Владеет:**
```typescript
// coreClient.ts — синхронизация с C++
const coreClient = {
    async call(cmd: string, args: any): Promise<any> {
        // JSON RPC к bendcore::api::Core
    }
};

// React компоненты — локальное состояние
function InstancesPanel() {
    const [instances, setInstances] = useState<Instance[]>([]);
    const [selected, setSelected] = useState<string | null>(null);
    // ← это состояние не синхронизировано с C++!
}

// SettingsStore — глобальное состояние UI
export const settingsStore = {
    viewport: { scale: 1.0, rotation: ... },
    ui: { theme: "dark", layout: "classic" },
    // ...
};
```

**Отвечает за:**
- ✓ Визуальное представление в браузере
- ✓ Выделение, активный вид, масштаб
- ✓ Пользовательские настройки

**Дублирует или теряет:**
- ⚠️ Дерево экземпляров — копируется из C++ в JS каждый раз
- ⚠️ Выделение — только в UI, C++ не знает
- ⚠️ Масштаб вида, ориентация — только в UI
- ⚠️ Состояние редактирования (какая трубе редактируется) — может рассинхронизироваться

**Проблема:** Двойное состояние (C++ + JS); синхронизация через JSON RPC.

---

### 2.7 Другие хранилища

#### bendcore::drawing_editor::DrawingUndoRedo
- Локальная история только для чертежа
- Не синхронизирована с геометрией

#### bendcore::output (bend_output.hpp, bend_output.cpp)
- Хранит Frame (размещение)
- Но также есть в Registry

#### bendcore::analysis::Analysis
- Вычисляет свойства трубы
- Результаты не кэшируются

#### Sketch Solver (sketch_solver.hpp)
- Отдельная система для эскизов
- Нет интеграции с основной геометрией

---

## 3. Матрица дублирования данных

| Данные | Registry | Cache | Formula | Drawing | UI | Output |
|--------|----------|-------|---------|---------|----|----|
| **Part ID** | ✓ | ❌ | ❌ | ❌ | ✓ | ❌ |
| **Instance ID** | ✓ | ❌ | ❌ | ❌ | ✓ | ❌ |
| **Path (Straight/Bend/Spiral)** | ✓ | ❌ | ⚠️ | ⚠️ | ✓ | ❌ |
| **OD (outer diameter)** | ✓ | ✓ | ⚠️ | ✓ | ✓ | ⚠️ |
| **Wall thickness** | ✓ | ✓ | ⚠️ | ✓ | ✓ | ⚠️ |
| **Material** | ✓ | ❌ | ❌ | ⚠️ | ✓ | ❌ |
| **Placement (Frame)** | ✓ | ❌ | ❌ | ❌ | ✓ | ✓ |
| **B-rep Geometry** | ❌ | ✓ | ❌ | ❌ | ✓ | ❌ |
| **Dimension values** | ❌ | ❌ | ❌ | ✓ | ✓ | ❌ |
| **Adaptive flag** | ✓ | ❌ | ❌ | ❌ | ✓ | ❌ |
| **Selected instance** | ❌ | ❌ | ❌ | ❌ | ✓ | ❌ |
| **View orientation** | ❌ | ❌ | ❌ | ⚠️ | ✓ | ❌ |
| **Undo/Redo history** | ❌ | ❌ | ❌ | ✓ (локально) | ✓ (локально) | ❌ |

**Легенда:**
- ✓ Единственный владелец
- ⚠️ Копия (может рассинхронизироваться)
- ❌ Не хранится

**Проблемы:**
- OD, Wall, Path — в 3–4 местах одновременно
- Placement — в Registry и Output (совпадает, но два места)
- Undo/Redo — в чертеже и UI отдельно (нет глобальной истории)
- Selected instance, View state — только в UI

---

## 4. Контракты и зависимости

### Существующие зависимости (часто молчаливые)

```
UI ─┬─→ coreClient.call() → C++ Core API
    └─→ SettingsStore (локально)

C++ API ─→ Registry (read/write)
       ├─→ GeometryCache (read)
       ├─→ formula (compute)
       └─→ drawing (generate)

Registry ──────→ TubeSignatureData (Part)
              └→ Instance, Frame

GeometryCache ──→ OCCT (TopoDS_Shape)
              └→ ExtractedParams

Drawing ──────→ Registry (читает параметры)
            └─→ GeometryCache (читает B-rep)
```

### Проблемные зависимости

1. **Ciclic reads:** Drawing читает параметры из Registry, но параметры могут измениться в Cache
2. **No subscription:** UI не узнаёт об изменениях в C++ State без повторного запроса
3. **Implicit contracts:** API команды неявно изменяют Registry, Cache, и UI (договора нет)
4. **No versioning:** При изменении параметра неясно, что пересчитывать

---

## 5. Критерии приёмки CAD-CORE-001

### ✓ Выполнено

1. **Все основные хранилища перечислены:**
   - Registry (Part, Instance)
   - GeometryCache (Shape + Params)
   - formula (AST, вычисления)
   - drawing (Geometry, Layouts)
   - core_api (State*)
   - UI State (React)
   - Output (Frame)
   - ✓ 8 основных хранилищ идентифицировано

2. **Для каждого типа данных определён владелец:**
   - Part, Instance → Registry
   - B-rep → GeometryCache
   - Path, OD, Wall → Registry (но дублируется)
   - Frame → Registry + Output
   - Geometry → Drawing
   - UI selection → React

3. **Обозначены дублирующиеся структуры:**
   - OD, Wall, Path — в Registry, Cache, Formula (3 места)
   - Frame — в Registry, Output (2 места)
   - Geometry — в Cache, Drawing, UI (3 места)

4. **Определена стратегия миграции:**
   - CadDocument как единый владелец
   - LegacyAdapter для старого API
   - Подписки на изменения через EventBus

---

## 6. Рекомендации для CAD-CORE-003 (CadDocument)

### Структура будущего CadDocument

```cpp
class CadDocument {
public:
    // Основные сущности (единый владелец)
    class EntityStore {
        std::map<EntityId, Part> parts;
        std::map<EntityId, Instance> instances;
        std::map<EntityId, Parameter> parameters;
        std::map<EntityId, Feature> features;
        std::map<EntityId, Shape> geometries;
    };

    class CommandEngine {
        std::vector<Command> history;  // Undo/Redo
        void execute(const Command& cmd);
    };

    class DependencyGraph {
        // Какие узлы зависят от каких
        std::map<EntityId, std::vector<EntityId>> dependencies;
    };

    class EventBus {
        // Подписка на изменения
        void subscribe(EntityType type, Callback callback);
        void publish(Event event);
    };

    // Синхронизированный доступ к данным
    const Part& get_part(EntityId id) const;
    void update_part(EntityId id, const Part& data);
    // ... и т.д.
};
```

### Миграционная стратегия

**Этап 1:** Создать CadDocument с миграцией Registry
```cpp
// bendcore::assembly::Registry → CadDocument.ComponentInstances
class ComponentInstances {
    // Переиспользует существующую логику Registry
};
```

**Этап 2:** Создать адаптер для старого API
```cpp
class LegacyAdapter {
    CadDocument& doc;
    // Переводит core_api::Core::call() → CadDocument::execute()
};
```

**Этап 3:** Миграция параметров в Parameter Store
```cpp
// formula.hpp → ParameterStore
// $OD → Parameter{id="param_od", owner=part_123, value=10.0}
```

---

## 7. Документы для архива

Этот аудит является основой для:
- `docs/architecture/document-ownership.md` — детальное описание контрактов
- `docs/architecture/data-flow.md` — диаграммы синхронизации
- `CAD-CORE-002` (Entity IDs) — типизация идентификаторов
- `CAD-CORE-003` (CadDocument) — центральная модель

---

## Выводы

### Текущие проблемы

1. **Нет единого источника истины** — состояние распределено по 8+ местам
2. **Дублирование данных** — OD, Wall, Path в 3+ местах
3. **Отсутствие контрактов** — неясно, кто владеет чем
4. **Молчаливые зависимости** — синхронизация через JSON RPC
5. **Нет версионирования** — невозможно отследить, что изменилось

### Путь вперёд

✓ CadDocument как единый владелец  
✓ EntityId для типизированных идентификаторов  
✓ LegacyAdapter для обратной совместимости  
✓ EventBus для подписок  
✓ DependencyGraph для отслеживания пересчётов  

**Статус:** ✓ Аудит завершён. Рекомендуется начать CAD-CORE-002 (Entity IDs).

