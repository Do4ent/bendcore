# BendCore UI Components Library

Professional React + TypeScript component library for CAD application UI.

## 10 Core Components

1. **RibbonBar** - Top toolbar with tabs and commands
2. **TreePanel** - Hierarchical feature tree
3. **PropertiesPanel** - Object property editor
4. **StatusBar** - Status information display
5. **ContextMenu** - Right-click context menu
6. **Tooltip** - Hover tooltips
7. **Icon** - Icon component
8. **Badge** - Status badges
9. **Spinner** - Loading spinner
10. **Divider** - Visual dividers

## Quick Start

### Import Components

```tsx
import { 
  RibbonBar, 
  TreePanel, 
  PropertiesPanel, 
  StatusBar,
  ContextMenu,
  Tooltip,
  Icon,
  Badge,
  Spinner,
  Divider
} from './components';
```

### Import Types

```tsx
import type { 
  RibbonTab, 
  TreeNode, 
  PropertyField,
  ContextMenuItem,
} from './components';
```

## Component Files

```
├── RibbonBar.tsx         # Ribbon toolbar
├── RibbonBar.css
├── TreePanel.tsx         # Feature tree
├── TreePanel.css
├── PropertiesPanel.tsx   # Properties editor
├── PropertiesPanel.css
├── StatusBar.tsx         # Status bar
├── StatusBar.css
├── ContextMenu.tsx       # Context menu
├── ContextMenu.css
├── Tooltip.tsx           # Tooltips
├── Tooltip.css
├── Icon.tsx              # Icons
├── Icon.css
├── Badge.tsx             # Badges
├── Badge.css
├── Spinner.tsx           # Loading spinner
├── Spinner.css
├── Divider.tsx           # Dividers
├── Divider.css
└── index.ts              # Barrel export
```

## Dark Mode Support

All components support dark mode via CSS custom properties:

```css
--color-bg-primary
--color-bg-secondary
--color-text
--color-primary
--color-border
```

## Accessibility

All components follow WCAG 2.1 guidelines:

- Semantic HTML
- ARIA labels and roles
- Keyboard navigation
- Focus management
- Screen reader support

## Performance

- Minimal dependencies
- CSS-based styling
- Optimized re-renders
- ~50KB total bundle size

## Browser Support

- Chrome/Edge 90+
- Firefox 88+
- Safari 14+

## TypeScript Support

Full TypeScript support with exported types for all components.

## License

Proprietary - Part of BendCore CAD System
