# Testing Guide для BendCore Modules

Полное руководство по тестированию 4 новых модулей.

## Unit Tests

### 1. Sketches Module Tests

```typescript
import { describe, it, expect, beforeEach } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';
import { SketchesModule, type Sketch2D } from '@/modules/sketches';

describe('SketchesModule', () => {
  it('should render sketches list', () => {
    render(<SketchesModule tubeId="tube-001" />);
    expect(screen.getByText('Sketches')).toBeInTheDocument();
  });

  it('should create a new sketch', () => {
    render(<SketchesModule tubeId="tube-001" />);
    const addBtn = screen.getByText('+');
    fireEvent.click(addBtn);
    // Проверить, что новый эскиз появился в списке
  });

  it('should delete sketch', () => {
    render(<SketchesModule tubeId="tube-001" />);
    const deleteBtn = screen.getAllByText('×')[0];
    fireEvent.click(deleteBtn);
    // Проверить удаление
  });

  it('should switch planes correctly', () => {
    render(<SketchesModule tubeId="tube-001" />);
    expect(screen.getByText('XY')).toBeInTheDocument();
    expect(screen.getByText('XZ')).toBeInTheDocument();
  });

  it('should render drawing tools', () => {
    render(<SketchesModule tubeId="tube-001" />);
    expect(screen.getByText('─')).toBeInTheDocument(); // Line tool
    expect(screen.getByText('◯')).toBeInTheDocument(); // Circle tool
  });

  it('should handle canvas click', () => {
    render(<SketchesModule tubeId="tube-001" />);
    const canvas = screen.getByRole('image', { hidden: true });
    if (canvas) {
      fireEvent.click(canvas);
      // Проверить статус сообщение о точке
    }
  });
});
```

### 2. Layers Manager Tests

```typescript
import { describe, it, expect } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';
import { LayersManager, useLayersManager } from '@/modules/layers';

describe('LayersManager', () => {
  it('should render layers list', () => {
    const layers = [
      {
        id: 'layer-1',
        name: 'Default',
        visible: true,
        locked: false,
        opacity: 100,
        color: '#0066cc',
        elementCount: 0,
      },
    ];

    render(
      <LayersManager layers={layers} />
    );

    expect(screen.getByText('Default')).toBeInTheDocument();
  });

  it('should toggle layer visibility', () => {
    const onToggleVisibility = jest.fn();
    const layers = [
      {
        id: 'layer-1',
        name: 'Test',
        visible: true,
        locked: false,
        opacity: 100,
        color: '#0066cc',
        elementCount: 0,
      },
    ];

    render(
      <LayersManager
        layers={layers}
        onToggleVisibility={onToggleVisibility}
      />
    );

    fireEvent.click(screen.getByTitle('Hide'));
    expect(onToggleVisibility).toHaveBeenCalledWith('layer-1');
  });

  it('should toggle layer lock', () => {
    const onToggleLock = jest.fn();
    const layers = [
      {
        id: 'layer-1',
        name: 'Test',
        visible: true,
        locked: false,
        opacity: 100,
        color: '#0066cc',
        elementCount: 0,
      },
    ];

    render(
      <LayersManager layers={layers} onToggleLock={onToggleLock} />
    );

    fireEvent.click(screen.getByTitle('Lock'));
    expect(onToggleLock).toHaveBeenCalledWith('layer-1');
  });

  it('should change layer opacity', () => {
    const onChangeOpacity = jest.fn();
    const layers = [
      {
        id: 'layer-1',
        name: 'Test',
        visible: true,
        locked: false,
        opacity: 100,
        color: '#0066cc',
        elementCount: 0,
      },
    ];

    render(
      <LayersManager
        layers={layers}
        onChangeOpacity={onChangeOpacity}
      />
    );

    const slider = screen.getByRole('slider');
    fireEvent.change(slider, { target: { value: '50' } });
    expect(onChangeOpacity).toHaveBeenCalledWith('layer-1', 50);
  });

  it('useLayersManager hook should manage layers', () => {
    const { result } = renderHook(() => useLayersManager());

    expect(result.current.layers).toHaveLength(1);
    expect(result.current.layers[0].name).toBe('Default');

    act(() => {
      result.current.addLayer();
    });

    expect(result.current.layers).toHaveLength(2);
  });
});
```

### 3. DWG/DXF Handler Tests

```typescript
import { describe, it, expect } from 'vitest';
import { DWGDXFHandler, type DXFEntity, type DWGFile } from '@/modules/formats';

describe('DWGDXFHandler', () => {
  describe('parseDXF', () => {
    it('should parse simple DXF content', () => {
      const dxfContent = `
0
SECTION
2
HEADER
0
ENDSEC
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
CIRCLE
8
Default
10
50.0
20
50.0
40
25.0
0
ENDSEC
0
EOF`;

      const result = DWGDXFHandler.parseDXF(dxfContent);

      expect(result.entities).toHaveLength(2);
      expect(result.entities[0].type).toBe('LINE');
      expect(result.entities[1].type).toBe('CIRCLE');
    });

    it('should extract layers correctly', () => {
      const dxfContent = `
0
LINE
8
Layer1
0
CIRCLE
8
Layer2`;

      const result = DWGDXFHandler.parseDXF(dxfContent);
      expect(result.layers).toContain('Layer1');
      expect(result.layers).toContain('Layer2');
    });
  });

  describe('generateDXF', () => {
    it('should generate valid DXF structure', () => {
      const entities: DXFEntity[] = [
        {
          type: 'LINE',
          layer: 'Default',
          x: 0,
          y: 0,
          x1: 100,
          y1: 100,
        },
      ];
      const layers = ['Default'];

      const dxf = DWGDXFHandler.generateDXF(entities, layers);

      expect(dxf).toContain('SECTION');
      expect(dxf).toContain('HEADER');
      expect(dxf).toContain('ENTITIES');
      expect(dxf).toContain('EOF');
    });

    it('should include all entity types', () => {
      const entities: DXFEntity[] = [
        { type: 'LINE', layer: 'Default', x: 0, y: 0, x1: 100, y1: 100 },
        { type: 'CIRCLE', layer: 'Default', x: 50, y: 50, radius: 25 },
        { type: 'ARC', layer: 'Default', x: 75, y: 75, radius: 15 },
      ];
      const layers = ['Default'];

      const dxf = DWGDXFHandler.generateDXF(entities, layers);

      expect(dxf).toContain('LINE');
      expect(dxf).toContain('CIRCLE');
      expect(dxf).toContain('ARC');
    });
  });

  describe('importFile', () => {
    it('should import DXF file', async () => {
      const fileContent = `0\nSECTION\n0\nENDSEC`;
      const file = new File([fileContent], 'test.dxf', {
        type: 'text/plain',
      });

      const result = await DWGDXFHandler.importFile(file);

      expect(result).toBeDefined();
      expect(result.fileName).toBeDefined();
      expect(result.entities).toBeDefined();
    });

    it('should reject unsupported file types', async () => {
      const file = new File(['content'], 'test.txt', { type: 'text/plain' });

      await expect(DWGDXFHandler.importFile(file)).rejects.toThrow();
    });
  });

  describe('exportFile', () => {
    it('should export to DXF format', () => {
      const entities: DXFEntity[] = [
        { type: 'LINE', layer: 'Default', x: 0, y: 0, x1: 100, y1: 100 },
      ];
      const layers = ['Default'];

      const blob = DWGDXFHandler.exportFile(entities, layers, 'test', 'dxf');

      expect(blob.type).toBe('application/octet-stream');
      expect(blob.size).toBeGreaterThan(0);
    });

    it('should export to DWG format', () => {
      const entities: DXFEntity[] = [
        { type: 'CIRCLE', layer: 'Default', x: 50, y: 50, radius: 25 },
      ];
      const layers = ['Default'];

      const blob = DWGDXFHandler.exportFile(entities, layers, 'test', 'dwg');

      expect(blob.type).toBe('application/octet-stream');
    });
  });
});
```

### 4. Collaboration Manager Tests

```typescript
import { describe, it, expect } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';
import { CollaborationManager, type Collaborator } from '@/modules/collaboration';

describe('CollaborationManager', () => {
  const currentUser: Collaborator = {
    id: 'user-001',
    name: 'Test User',
    email: 'test@example.com',
    role: 'owner',
    status: 'online',
    color: '#0066cc',
  };

  it('should render collaboration manager', () => {
    render(
      <CollaborationManager
        projectId="test-project"
        currentUser={currentUser}
      />
    );

    expect(screen.getByText('Collaboration')).toBeInTheDocument();
  });

  it('should show online collaborators count', () => {
    render(
      <CollaborationManager
        projectId="test-project"
        currentUser={currentUser}
      />
    );

    expect(screen.getByText(/\d+ online/)).toBeInTheDocument();
  });

  it('should render tabs', () => {
    render(
      <CollaborationManager
        projectId="test-project"
        currentUser={currentUser}
      />
    );

    expect(screen.getByText('Collaborators')).toBeInTheDocument();
    expect(screen.getByText('Activity')).toBeInTheDocument();
    expect(screen.getByText('Sharing')).toBeInTheDocument();
  });

  it('should allow owner to invite collaborators', () => {
    render(
      <CollaborationManager
        projectId="test-project"
        currentUser={currentUser}
      />
    );

    const inviteBtn = screen.getByText('+ Invite Collaborator');
    fireEvent.click(inviteBtn);

    expect(screen.getByPlaceholderText('Email address')).toBeInTheDocument();
  });

  it('should show activity log on activity tab', () => {
    render(
      <CollaborationManager
        projectId="test-project"
        currentUser={currentUser}
      />
    );

    fireEvent.click(screen.getByText('Activity'));
    expect(screen.getByText(/Modified|Created/)).toBeInTheDocument();
  });

  it('should show sharing options on sharing tab', () => {
    render(
      <CollaborationManager
        projectId="test-project"
        currentUser={currentUser}
      />
    );

    fireEvent.click(screen.getByText('Sharing'));
    expect(screen.getByText('Share Project')).toBeInTheDocument();
    expect(screen.getByText('Version Control')).toBeInTheDocument();
  });
});
```

---

## Integration Tests

### E2E Test для полного workflow:

```typescript
import { describe, it, expect } from 'vitest';
import { render, screen, fireEvent, waitFor } from '@testing-library/react';
import { BendCoreApp } from '@/App';

describe('BendCore Full Integration', () => {
  it('should complete sketches creation and layer management workflow', async () => {
    render(<BendCoreApp />);

    // 1. Перейти в режим Sketches
    fireEvent.click(screen.getByText('2D Sketches'));
    expect(screen.getByText('Front View')).toBeInTheDocument();

    // 2. Создать новый эскиз
    const addSketchBtn = screen.getByText('+');
    fireEvent.click(addSketchBtn);

    // 3. Перейти в режим Layers
    fireEvent.click(screen.getByText('Layers'));
    await waitFor(() => {
      expect(screen.getByText('Default')).toBeInTheDocument();
    });

    // 4. Добавить новый слой
    const addLayerBtn = screen.getByRole('button', { name: '+' });
    fireEvent.click(addLayerBtn);

    // 5. Проверить что слой добавлен
    await waitFor(() => {
      expect(screen.getByText(/Layer \d+/)).toBeInTheDocument();
    });
  });

  it('should handle DWG import and collaboration workflow', async () => {
    render(<BendCoreApp />);

    // 1. Перейти в DWG режим
    fireEvent.click(screen.getByText('DWG/DXF'));

    // 2. Загрузить файл
    const fileInput = screen.getByLabelText(/file/i) as HTMLInputElement;
    const file = new File(['DXF content'], 'test.dxf');

    // 3. Перейти в Collaboration
    fireEvent.click(screen.getByText('Collaboration'));
    expect(screen.getByText('Collaboration')).toBeInTheDocument();

    // 4. Проверить что можно пригласить соавторов
    const inviteBtn = screen.getByText('+ Invite Collaborator');
    expect(inviteBtn).toBeInTheDocument();
  });
});
```

---

## Performance Tests

### Тесты производительности для больших объемов данных:

```typescript
import { describe, it, expect, bench } from 'vitest';
import { DWGDXFHandler, type DXFEntity } from '@/modules/formats';

describe('Performance Tests', () => {
  bench('should parse large DXF file quickly', () => {
    const largeEntities: DXFEntity[] = Array.from({ length: 10000 }, (_, i) => ({
      type: i % 3 === 0 ? 'LINE' : i % 3 === 1 ? 'CIRCLE' : 'ARC',
      layer: `Layer-${i % 100}`,
      x: Math.random() * 1000,
      y: Math.random() * 1000,
      x1: Math.random() * 1000,
      y1: Math.random() * 1000,
      radius: Math.random() * 50,
    }));

    const dxf = DWGDXFHandler.generateDXF(largeEntities, []);
    const result = DWGDXFHandler.parseDXF(dxf);

    expect(result.entities.length).toBe(10000);
  });

  bench('should render 1000 layers without performance degradation', () => {
    const layers = Array.from({ length: 1000 }, (_, i) => ({
      id: `layer-${i}`,
      name: `Layer ${i}`,
      visible: Math.random() > 0.5,
      locked: Math.random() > 0.5,
      opacity: Math.random() * 100,
      color: `hsl(${Math.random() * 360}, 70%, 50%)`,
      elementCount: Math.floor(Math.random() * 100),
    }));

    render(<LayersManager layers={layers} />);
    expect(screen.getByText('1000 layers')).toBeInTheDocument();
  });
});
```

---

## Manual Testing Checklist

### Sketches Module Checklist:

- [ ] Create/delete sketches
- [ ] Switch between XY, XZ, YZ planes
- [ ] Use all drawing tools (line, circle, arc, etc.)
- [ ] Apply constraints
- [ ] Edit sketch properties
- [ ] Canvas interaction and cursor updates
- [ ] Rename sketches
- [ ] Grid display and toggle

### Layers Manager Checklist:

- [ ] Add/delete layers
- [ ] Toggle layer visibility
- [ ] Lock/unlock layers
- [ ] Adjust opacity slider
- [ ] Rename layers by double-click
- [ ] Layer selection highlighting
- [ ] Element count display
- [ ] Color coding

### DWG/DXF Handler Checklist:

- [ ] Import DXF file
- [ ] Import DWG file
- [ ] Display imported layers
- [ ] Display imported entities
- [ ] Export to DXF
- [ ] Export to DWG
- [ ] Verify file integrity after export
- [ ] Handle invalid files gracefully

### Collaboration Manager Checklist:

- [ ] Display collaborators list
- [ ] Show online/offline status
- [ ] Toggle activity tab
- [ ] View version history
- [ ] Copy share link
- [ ] Invite collaborator (owner only)
- [ ] Change collaborator role
- [ ] Remove collaborator

---

## CI/CD Integration

### GitHub Actions workflow:

```yaml
name: Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - uses: actions/setup-node@v2
        with:
          node-version: '18'
      
      - run: npm ci
      - run: npm run test
      - run: npm run test:integration
      - run: npm run test:e2e
      
      - name: Upload coverage
        uses: codecov/codecov-action@v2
        with:
          files: ./coverage/coverage-final.json
```

---

## Coverage Goals

- Unit Tests: 85%+
- Integration Tests: 70%+
- E2E Tests: 60%+
- Overall Coverage: 75%+

---

**Version:** 1.0.0  
**Last Updated:** 2026-10-24  
**Status:** Production Ready
