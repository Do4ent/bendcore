# Integration Guide для 4 новых модулей BendCore

Руководство по интеграции новых модулей в главное приложение BendCore CAD.

## Quick Start

### 1. Импорт модулей в главный App.tsx

```typescript
import React, { useState } from 'react';
import {
  SketchesModule,
  LayersManager,
  useLayersManager,
  CollaborationManager,
  DWGDXFHandler,
  useDWGDXFHandler,
} from '@/modules';

export interface AppMode {
  mode: 'sketches' | 'layers' | 'collaboration' | 'dwg-import' | 'main';
}
```

### 2. Добавление навигации

```typescript
function BendCoreApp() {
  const [activeMode, setActiveMode] = useState<AppMode['mode']>('main');
  const { layers, ...layersManager } = useLayersManager();

  const navigationTabs = [
    { id: 'main', label: 'Main', icon: '⬡' },
    { id: 'sketches', label: '2D Sketches', icon: '⬢' },
    { id: 'layers', label: 'Layers', icon: '≡' },
    { id: 'collaboration', label: 'Collaboration', icon: '👥' },
    { id: 'dwg-import', label: 'DWG/DXF', icon: '📥' },
  ];

  return (
    <div className="bendcore-app">
      <nav className="app-nav">
        {navigationTabs.map((tab) => (
          <button
            key={tab.id}
            className={`nav-btn ${activeMode === tab.id ? 'active' : ''}`}
            onClick={() => setActiveMode(tab.id as AppMode['mode'])}
            title={tab.label}
          >
            <span className="icon">{tab.icon}</span>
            <span className="label">{tab.label}</span>
          </button>
        ))}
      </nav>

      <div className="app-content">
        {renderContent(activeMode)}
      </div>
    </div>
  );
}
```

### 3. Рендеринг компонентов по режимам

```typescript
function renderContent(mode: AppMode['mode']) {
  const currentUser = {
    id: 'user-001',
    name: 'Developer User',
    email: 'dev@bendcore.app',
    role: 'owner' as const,
    status: 'online' as const,
    color: '#0066cc',
  };

  switch (mode) {
    case 'sketches':
      return <SketchesModule tubeId="tube-001" />;

    case 'layers':
      return (
        <LayersManager
          layers={layers}
          selectedLayerId={selectedLayerId}
          onSelectLayer={setSelectedLayerId}
          onToggleVisibility={toggleVisibility}
          onToggleLock={toggleLock}
          onAddLayer={addLayer}
          onDeleteLayer={deleteLayer}
          onRenameLayer={renameLayer}
          onChangeOpacity={changeOpacity}
        />
      );

    case 'collaboration':
      return (
        <CollaborationManager
          projectId="bendcore-2026"
          currentUser={currentUser}
        />
      );

    case 'dwg-import':
      return <DWGImportExportUI />;

    default:
      return <MainEditor />;
  }
}
```

---

## Детальная интеграция каждого модуля

### Sketches Module

#### Базовая интеграция:

```typescript
import { SketchesModule, type Sketch2D } from '@/modules/sketches';

interface SketchesIntegrationProps {
  tubeId: string;
  onSketchChange?: (sketch: Sketch2D) => void;
  onSketchDelete?: (sketchId: string) => void;
}

export function SketchesIntegration({
  tubeId,
  onSketchChange,
  onSketchDelete,
}: SketchesIntegrationProps) {
  return (
    <div className="sketches-container">
      <SketchesModule tubeId={tubeId} />
    </div>
  );
}
```

#### Продвинутое использование с сохранением:

```typescript
import { useEffect, useState } from 'react';
import { SketchesModule, type Sketch2D } from '@/modules/sketches';

export function SketchesWithPersistence() {
  const [sketches, setSketches] = useState<Sketch2D[]>([]);

  useEffect(() => {
    // Загрузить эскизы из localStorage
    const saved = localStorage.getItem('bendcore-sketches');
    if (saved) {
      setSketches(JSON.parse(saved));
    }
  }, []);

  useEffect(() => {
    // Сохранить эскизы при изменении
    localStorage.setItem('bendcore-sketches', JSON.stringify(sketches));
  }, [sketches]);

  return <SketchesModule tubeId="tube-001" />;
}
```

---

### Layers Manager

#### Базовая интеграция:

```typescript
import { LayersManager, useLayersManager } from '@/modules/layers';

export function LayersIntegration() {
  const {
    layers,
    selectedLayerId,
    setSelectedLayerId,
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
      onSelectLayer={setSelectedLayerId}
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

#### С синхронизацией элементов:

```typescript
import { useCallback } from 'react';
import { LayersManager, useLayersManager, type Layer } from '@/modules/layers';

interface Element {
  id: string;
  layerId: string;
  name: string;
}

export function LayersSyncWithElements() {
  const { layers, ...layersManager } = useLayersManager();
  const [elements, setElements] = useState<Element[]>([]);

  const handleSelectLayer = useCallback((layerId: string) => {
    layersManager.setSelectedLayerId(layerId);
    // Отфильтровать элементы по слою
    const layerElements = elements.filter((e) => e.layerId === layerId);
    console.log('Layer elements:', layerElements);
  }, []);

  return (
    <>
      <LayersManager
        layers={layers}
        {...layersManager}
        onSelectLayer={handleSelectLayer}
      />
      <ElementsList elements={elements} />
    </>
  );
}
```

---

### DWG/DXF Handler

#### Импорт DXF файла:

```typescript
import { useDWGDXFHandler, type DWGFile } from '@/modules/formats';

export function DWGImportUI() {
  const { importDXF } = useDWGDXFHandler();
  const [importedFile, setImportedFile] = useState<DWGFile | null>(null);

  const handleFileInput = async (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (!file) return;

    try {
      const dwgFile = await importDXF(file);
      setImportedFile(dwgFile);
      console.log('Imported DWG:', dwgFile);
    } catch (error) {
      console.error('Import failed:', error);
    }
  };

  return (
    <div className="dwg-import">
      <input
        type="file"
        accept=".dxf,.dwg"
        onChange={handleFileInput}
      />
      {importedFile && (
        <div className="import-info">
          <h3>{importedFile.fileName}</h3>
          <p>Layers: {importedFile.layers.length}</p>
          <p>Entities: {importedFile.entities.length}</p>
          <div className="layers-list">
            {importedFile.layers.map((layer) => (
              <div key={layer} className="layer-item">
                {layer}
              </div>
            ))}
          </div>
        </div>
      )}
    </div>
  );
}
```

#### Экспорт в DXF:

```typescript
import { useDWGDXFHandler, type DXFEntity } from '@/modules/formats';

export function DWGExportUI() {
  const { exportDXF, exportDWG } = useDWGDXFHandler();

  const sampleEntities: DXFEntity[] = [
    {
      type: 'LINE',
      layer: 'Geometry',
      x: 0,
      y: 0,
      x1: 100,
      y1: 100,
    },
    {
      type: 'CIRCLE',
      layer: 'Geometry',
      x: 50,
      y: 50,
      radius: 25,
    },
  ];

  const layers = ['Geometry', 'Dimensions'];

  return (
    <div className="dwg-export">
      <button
        onClick={() => exportDXF(sampleEntities, layers, 'my-drawing')}
      >
        Export as DXF
      </button>
      <button
        onClick={() => exportDWG(sampleEntities, layers, 'my-drawing')}
      >
        Export as DWG
      </button>
    </div>
  );
}
```

---

### Collaboration Manager

#### Базовая интеграция:

```typescript
import { CollaborationManager, type Collaborator } from '@/modules/collaboration';

export function CollaborationIntegration() {
  const currentUser: Collaborator = {
    id: 'user-001',
    name: 'John Developer',
    email: 'john@bendcore.app',
    role: 'owner',
    status: 'online',
    color: '#0066cc',
  };

  return (
    <CollaborationManager
      projectId="bendcore-project-001"
      currentUser={currentUser}
    />
  );
}
```

#### С WebSocket интеграцией:

```typescript
import { useEffect, useState } from 'react';
import { CollaborationManager, type Collaborator, type ChangeLog } from '@/modules/collaboration';

export function CollaborationWithRealtime() {
  const [collaborators, setCollaborators] = useState<Collaborator[]>([]);
  const [changeLogs, setChangeLogs] = useState<ChangeLog[]>([]);

  useEffect(() => {
    // Подключиться к WebSocket для real-time обновлений
    const ws = new WebSocket('wss://api.bendcore.app/collab/project-001');

    ws.onmessage = (event) => {
      const message = JSON.parse(event.data);

      if (message.type === 'collaborator-update') {
        setCollaborators(message.data);
      }

      if (message.type === 'activity-log') {
        setChangeLogs((prev) => [message.data, ...prev]);
      }
    };

    return () => ws.close();
  }, []);

  const currentUser: Collaborator = {
    id: 'user-001',
    name: 'Developer',
    email: 'dev@bendcore.app',
    role: 'owner',
    status: 'online',
    color: '#0066cc',
  };

  return (
    <CollaborationManager
      projectId="bendcore-project-001"
      currentUser={currentUser}
    />
  );
}
```

---

## CSS Кастомизация

### Переменные темы:

```css
:root {
  /* Primary Colors */
  --color-primary: #0066cc;
  --color-primary-light: #e6f0ff;
  --color-primary-dark: #004499;

  /* Background */
  --color-bg-primary: #ffffff;
  --color-bg-secondary: #f5f5f5;
  --color-bg-tertiary: #efefef;
  --color-bg-hover: #f0f0f0;

  /* Border */
  --color-border: #e0e0e0;
  --color-border-light: #f0f0f0;
  --color-border-dark: #cccccc;

  /* Text */
  --color-text: #333333;
  --color-text-secondary: #999999;
  --color-text-tertiary: #cccccc;

  /* Status */
  --color-success: #4ade80;
  --color-warning: #fbbf24;
  --color-error: #ff6b6b;
  --color-info: #3b82f6;

  /* Shadows */
  --shadow-sm: 0 1px 2px rgba(0, 0, 0, 0.05);
  --shadow-md: 0 4px 6px rgba(0, 0, 0, 0.1);
  --shadow-lg: 0 10px 15px rgba(0, 0, 0, 0.15);
}

@media (prefers-color-scheme: dark) {
  :root {
    --color-bg-primary: #1e1e1e;
    --color-bg-secondary: #2d2d2d;
    --color-bg-tertiary: #3a3a3a;
    --color-border: #404040;
    --color-text: #f0f0f0;
    --color-text-secondary: #999999;
  }
}
```

---

## Производительность

### Оптимизация для больших проектов:

```typescript
import { useMemo } from 'react';
import { LayersManager, useLayersManager } from '@/modules/layers';

export function OptimizedLayersManager() {
  const { layers, ...layersManager } = useLayersManager();

  // Мемоизировать отфильтрованные слои
  const visibleLayers = useMemo(
    () => layers.filter((l) => l.visible),
    [layers]
  );

  // Мемоизировать колбэки для избежания ненужных re-renders
  const handleSelectMemo = useMemo(
    () => (layerId: string) => layersManager.setSelectedLayerId(layerId),
    [layersManager]
  );

  return (
    <LayersManager
      layers={visibleLayers}
      {...layersManager}
      onSelectLayer={handleSelectMemo}
    />
  );
}
```

---

## Тестирование

### Unit тесты для модулей:

```typescript
import { describe, it, expect } from 'vitest';
import { DWGDXFHandler } from '@/modules/formats';

describe('DWGDXFHandler', () => {
  it('should generate valid DXF content', () => {
    const entities = [
      { type: 'LINE', layer: 'Default', x: 0, y: 0, x1: 100, y1: 100 },
    ];
    const layers = ['Default'];

    const dxf = DWGDXFHandler.generateDXF(entities, layers);
    expect(dxf).toContain('SECTION');
    expect(dxf).toContain('LINE');
  });

  it('should parse DXF correctly', () => {
    const dxfContent = `
0
SECTION
2
ENTITIES
0
LINE
8
Default
10
0.0
20
0.0
11
100.0
21
100.0
0
ENDSEC
    `;

    const result = DWGDXFHandler.parseDXF(dxfContent);
    expect(result.entities).toHaveLength(1);
    expect(result.entities[0].type).toBe('LINE');
  });
});
```

---

## Deployment

### Production Build:

```bash
# Сборка проекта
npm run build

# Проверка размера бандла
npm run analyze

# Запуск тестов перед продакшеном
npm run test:all
```

### Docker интеграция:

```dockerfile
FROM node:18-alpine

WORKDIR /app

COPY package*.json ./
RUN npm ci --production

COPY src/ ./src/
COPY public/ ./public/

RUN npm run build

EXPOSE 3000
CMD ["npm", "start"]
```

---

## Support и Issues

При возникновении проблем:

1. Проверьте консоль браузера (F12 → Console)
2. Убедитесь, что все импорты правильные
3. Проверьте CSS переменные в корневом элементе
4. Посмотрите документацию в README.md модуля

---

## Roadmap

Планируемые улучшения для модулей:

- [ ] Поддержка Step файлов для Sketches Module
- [ ] Экспорт слоев в изображения (PNG, SVG)
- [ ] Синхронизация через WebSocket (real-time)
- [ ] Поддержка комментариев и обсуждений в Collaboration
- [ ] Экспорт истории изменений в PDF
- [ ] Мобильное приложение для Collaboration
- [ ] Интеграция с облачным хранилищем (Google Drive, Dropbox)

---

**Версия документации:** 1.0.0  
**Последнее обновление:** 2026-10-24  
**Статус:** Production Ready
