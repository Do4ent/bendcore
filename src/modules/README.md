# BendCore Modules

Набор расширений функциональности для BendCore CAD.

## 1. Sketches Module (2D Sketches)

Полнофункциональная система для работы с 2D эскизами.

### Основные возможности:
- Создание и удаление 2D эскизов
- Поддержка плоскостей: XY, XZ, YZ
- Инструменты рисования: линия, окружность, дуга, прямоугольник, полигон
- Система ограничений (constraints): горизонтальное, вертикальное, параллельное, перпендикулярное, расстояние, радиус
- Редактирование свойств элементов (толщина линии, цвет, заливка)
- Сетка и масштабирование

### Использование:

```typescript
import { SketchesModule } from '@/modules/sketches';

<SketchesModule tubeId="tube-001" />
```

### Интерфейсы:

```typescript
export interface Sketch2D {
  id: string;
  name: string;
  plane: 'xy' | 'xz' | 'yz';
  elements: SketchElement[];
  constraints: SketchConstraint[];
  createdAt: Date;
  updatedAt: Date;
}

export interface SketchElement {
  id: string;
  type: 'line' | 'circle' | 'arc' | 'rectangle' | 'polygon' | 'point';
  points: [number, number][];
  properties: {
    strokeWidth?: number;
    color?: string;
    fillColor?: string;
  };
}

export interface SketchConstraint {
  id: string;
  type: 'horizontal' | 'vertical' | 'parallel' | 'perpendicular' | 'distance' | 'radius';
  elements: string[];
  value?: number;
}
```

---

## 2. Layers Manager

Система управления слоями для организации элементов проекта.

### Основные возможности:
- Создание и удаление слоев
- Показ/скрытие слоев
- Блокировка/разблокировка слоев
- Регулировка прозрачности (0-100%)
- Цветовое кодирование слоев
- Переименование слоев
- Подсчет элементов на слое

### Использование:

```typescript
import { LayersManager, useLayersManager } from '@/modules/layers';

function MyComponent() {
  const {
    layers,
    selectedLayerId,
    addLayer,
    deleteLayer,
    toggleVisibility,
    toggleLock,
    renameLayer,
    changeOpacity,
  } = useLayersManager();

  return (
    <LayersManager
      layers={layers}
      selectedLayerId={selectedLayerId}
      onSelectLayer={(id) => console.log('Selected:', id)}
      onToggleVisibility={toggleVisibility}
      onToggleLock={toggleLock}
      onAddLayer={addLayer}
      onDeleteLayer={deleteLayer}
      onRenameLayer={renameLayer}
      onChangeOpacity={changeOpacity}
    />
  );
}
```

### Интерфейсы:

```typescript
export interface Layer {
  id: string;
  name: string;
  visible: boolean;
  locked: boolean;
  opacity: number;
  color: string;
  elementCount: number;
}
```

---

## 3. DWG/DXF Support

Обработчик для импорта и экспорта форматов AutoCAD (DWG/DXF).

### Основные возможности:
- Парсинг DXF файлов
- Генерация DXF файлов
- Поддержка слоев
- Экспорт геометрии (линии, окружности, дуги)
- Асинхронный импорт файлов

### Использование:

```typescript
import { DWGDXFHandler, useDWGDXFHandler } from '@/modules/formats';

function FileImport() {
  const { importDXF, exportDXF, exportDWG } = useDWGDXFHandler();

  const handleImport = async (file: File) => {
    const dwgFile = await importDXF(file);
    console.log('Imported:', dwgFile);
  };

  const handleExport = (entities, layers, fileName) => {
    exportDXF(entities, layers, fileName);
  };

  return (
    <div>
      <input type="file" accept=".dxf,.dwg" onChange={(e) => {
        if (e.target.files?.[0]) {
          handleImport(e.target.files[0]);
        }
      }} />
      <button onClick={() => handleExport(entities, layers, 'drawing')}>
        Export DXF
      </button>
    </div>
  );
}
```

### Интерфейсы:

```typescript
export interface DXFEntity {
  type: string;
  layer: string;
  x?: number;
  y?: number;
  x1?: number;
  y1?: number;
  x2?: number;
  y2?: number;
  radius?: number;
  [key: string]: any;
}

export interface DWGFile {
  fileName: string;
  version: string;
  layers: string[];
  entities: DXFEntity[];
  metadata: {
    createdAt: Date;
    author?: string;
    description?: string;
  };
}

export class DWGDXFHandler {
  static parseDXF(content: string): DWGFile;
  static generateDXF(entities: DXFEntity[], layers: string[]): string;
  static exportDWG(entities: DXFEntity[], layers: string[], fileName: string): string;
  static async importFile(file: File): Promise<DWGFile>;
  static exportFile(entities: DXFEntity[], layers: string[], fileName: string, format?: 'dxf' | 'dwg'): Blob;
}
```

---

## 4. Collaboration Manager

Полнофункциональная система совместной работы с проектом.

### Основные возможности:
- Управление соавторами (Owner, Editor, Viewer)
- Отслеживание статуса онлайн/офлайн
- История изменений (Activity Log)
- Управление правами доступа
- Приглашение соавторов по email
- Общая ссылка на проект
- Контроль версий
- Real-time синхронизация

### Использование:

```typescript
import { CollaborationManager } from '@/modules/collaboration';

const currentUser = {
  id: 'user-001',
  name: 'John Doe',
  email: 'john@example.com',
  role: 'owner',
  status: 'online',
  color: '#FF6B6B',
};

<CollaborationManager
  projectId="proj-001"
  currentUser={currentUser}
/>
```

### Интерфейсы:

```typescript
export interface Collaborator {
  id: string;
  name: string;
  email: string;
  avatar?: string;
  role: 'owner' | 'editor' | 'viewer';
  status: 'online' | 'offline' | 'idle';
  lastSeen?: Date;
  color: string;
}

export interface ChangeLog {
  id: string;
  timestamp: Date;
  author: Collaborator;
  action: string;
  objectName: string;
  details: string;
}
```

---

## Integration Example

Интеграция всех 4 модулей в главное приложение:

```typescript
import React, { useState } from 'react';
import { SketchesModule } from '@/modules/sketches';
import { LayersManager, useLayersManager } from '@/modules/layers';
import { CollaborationManager } from '@/modules/collaboration';

export function BendCoreApp() {
  const [activeView, setActiveView] = useState<'sketch' | 'layers' | 'collab'>('sketch');
  const { layers, ...layersManager } = useLayersManager();

  const currentUser = {
    id: 'user-001',
    name: 'Developer',
    email: 'dev@example.com',
    role: 'owner' as const,
    status: 'online' as const,
    color: '#0066cc',
  };

  return (
    <div className="app-container">
      <nav>
        <button onClick={() => setActiveView('sketch')}>Sketches</button>
        <button onClick={() => setActiveView('layers')}>Layers</button>
        <button onClick={() => setActiveView('collab')}>Collaboration</button>
      </nav>

      <main>
        {activeView === 'sketch' && <SketchesModule tubeId="tube-001" />}
        {activeView === 'layers' && (
          <LayersManager layers={layers} {...layersManager} />
        )}
        {activeView === 'collab' && (
          <CollaborationManager projectId="proj-001" currentUser={currentUser} />
        )}
      </main>
    </div>
  );
}
```

---

## CSS Customization

Все модули используют CSS переменные для стилизации:

```css
:root {
  --color-primary: #0066cc;
  --color-bg-primary: #ffffff;
  --color-bg-secondary: #f5f5f5;
  --color-border: #e0e0e0;
  --color-text: #333333;
  --color-text-secondary: #999999;
  --color-bg-hover: #f0f0f0;
}
```

---

## File Structure

```
src/modules/
├── sketches/
│   ├── SketchesModule.tsx
│   ├── SketchesModule.css
│   └── index.ts
├── layers/
│   ├── LayersManager.tsx
│   ├── LayersManager.css
│   └── index.ts
├── formats/
│   ├── DWGDXFHandler.ts
│   └── index.ts
├── collaboration/
│   ├── CollaborationManager.tsx
│   ├── CollaborationManager.css
│   └── index.ts
├── index.ts
└── README.md
```

---

## Version

- Version: 1.0.0
- Status: Production Ready
- Last Updated: 2026-10-24
- TypeScript: Full support
- React: 18.x
