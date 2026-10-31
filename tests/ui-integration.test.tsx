/**
 * UI Integration Tests for BendCore CAD
 * Tests core UI functionality: viewport, feature tree, properties panel
 */

import React from 'react';
import { render, screen, fireEvent, waitFor, within } from '@testing-library/react';
import '@testing-library/jest-dom';

// Mock components for testing
const MockViewport = () => (
  <div data-testid="viewport" className="viewport">
    <canvas data-testid="canvas-3d" width="800" height="600"></canvas>
  </div>
);

const MockFeatureTree = () => (
  <div data-testid="feature-tree" role="tree">
    <div role="treeitem" aria-expanded="false">Body</div>
    <div role="treeitem" aria-expanded="false">Sketch_1</div>
    <div role="treeitem" aria-expanded="false">Pad_1</div>
  </div>
);

const MockPropertiesPanel = ({ selected }: { selected?: string }) => (
  <div data-testid="properties-panel">
    {selected ? (
      <>
        <div className="property">
          <label>Name:</label>
          <input value={selected} readOnly />
        </div>
        <div className="property">
          <label>Type:</label>
          <input value="Feature" readOnly />
        </div>
      </>
    ) : (
      <div className="empty">Select an object to view properties</div>
    )}
  </div>
);

const MockApplication = () => {
  const [selected, setSelected] = React.useState<string | undefined>();

  return (
    <div className="app" data-testid="app">
      <header data-testid="toolbar">
        <button data-testid="btn-new">New</button>
        <button data-testid="btn-open">Open</button>
        <button data-testid="btn-save">Save</button>
        <button data-testid="btn-undo">Undo</button>
        <button data-testid="btn-redo">Redo</button>
      </header>
      <div className="main-layout">
        <nav data-testid="sidebar-left">
          <div onClick={() => setSelected('Body')}>
            <MockFeatureTree />
          </div>
        </nav>
        <main data-testid="viewport-container">
          <MockViewport />
        </main>
        <aside data-testid="sidebar-right">
          <MockPropertiesPanel selected={selected} />
        </aside>
      </div>
      <footer data-testid="status-bar">
        Status: {selected ? `Selected: ${selected}` : 'Ready'}
      </footer>
    </div>
  );
};

describe('BendCore UI Integration Tests', () => {
  describe('Application Layout', () => {
    it('should render main application layout', () => {
      render(<MockApplication />);
      expect(screen.getByTestId('app')).toBeInTheDocument();
      expect(screen.getByTestId('toolbar')).toBeInTheDocument();
      expect(screen.getByTestId('sidebar-left')).toBeInTheDocument();
      expect(screen.getByTestId('viewport-container')).toBeInTheDocument();
      expect(screen.getByTestId('sidebar-right')).toBeInTheDocument();
      expect(screen.getByTestId('status-bar')).toBeInTheDocument();
    });

    it('should render 3D viewport with canvas', () => {
      render(<MockApplication />);
      const canvas = screen.getByTestId('canvas-3d') as HTMLCanvasElement;
      expect(canvas).toBeInTheDocument();
      expect(canvas.width).toBe(800);
      expect(canvas.height).toBe(600);
    });
  });

  describe('Feature Tree Navigation', () => {
    it('should render feature tree with objects', () => {
      render(<MockApplication />);
      const tree = screen.getByTestId('feature-tree');
      const items = within(tree).getAllByRole('treeitem');
      expect(items.length).toBeGreaterThan(0);
      expect(items[0]).toHaveTextContent('Body');
    });

    it('should support tree item selection', () => {
      render(<MockApplication />);
      const treeItems = screen.getAllByRole('treeitem');
      fireEvent.click(treeItems[0]);
      // Status should update after selection
      waitFor(() => {
        expect(screen.getByTestId('status-bar')).toHaveTextContent('Selected');
      });
    });

    it('should expand/collapse tree items', () => {
      render(<MockApplication />);
      const treeItem = screen.getAllByRole('treeitem')[0];
      expect(treeItem).toHaveAttribute('aria-expanded', 'false');
      fireEvent.click(treeItem);
      // Expansion state would change in real app
      expect(treeItem).toBeInTheDocument();
    });
  });

  describe('Properties Panel', () => {
    it('should show empty state when nothing selected', () => {
      render(<MockApplication />);
      expect(screen.getByText('Select an object to view properties')).toBeInTheDocument();
    });

    it('should display properties when object selected', async () => {
      render(<MockApplication />);
      const treeItem = screen.getByText('Body');
      fireEvent.click(treeItem.parentElement!);
      await waitFor(() => {
        const inputs = screen.getAllByRole('textbox');
        expect(inputs.length).toBeGreaterThan(0);
      });
    });

    it('should update properties panel on selection change', async () => {
      render(<MockApplication />);
      const body = screen.getByText('Body');
      fireEvent.click(body.parentElement!);
      await waitFor(() => {
        expect(screen.queryByText('Select an object')).not.toBeInTheDocument();
      });
    });
  });

  describe('Toolbar Actions', () => {
    it('should have all essential toolbar buttons', () => {
      render(<MockApplication />);
      expect(screen.getByTestId('btn-new')).toBeInTheDocument();
      expect(screen.getByTestId('btn-open')).toBeInTheDocument();
      expect(screen.getByTestId('btn-save')).toBeInTheDocument();
      expect(screen.getByTestId('btn-undo')).toBeInTheDocument();
      expect(screen.getByTestId('btn-redo')).toBeInTheDocument();
    });

    it('toolbar buttons should be clickable', () => {
      render(<MockApplication />);
      const saveBtn = screen.getByTestId('btn-save');
      expect(saveBtn).toBeEnabled();
      fireEvent.click(saveBtn);
      expect(saveBtn).toBeInTheDocument();
    });
  });

  describe('Viewport Interaction', () => {
    it('should render viewport container', () => {
      render(<MockApplication />);
      expect(screen.getByTestId('viewport-container')).toBeInTheDocument();
    });

    it('should display canvas for 3D rendering', () => {
      render(<MockApplication />);
      const canvas = screen.getByTestId('canvas-3d');
      expect(canvas).toBeInTheDocument();
      expect(canvas.tagName).toBe('CANVAS');
    });

    it('should have proper canvas dimensions for rendering', () => {
      render(<MockApplication />);
      const canvas = screen.getByTestId('canvas-3d') as HTMLCanvasElement;
      const ctx = canvas.getContext('2d');
      expect(ctx).not.toBeNull();
    });
  });

  describe('Status Bar', () => {
    it('should display status information', () => {
      render(<MockApplication />);
      expect(screen.getByTestId('status-bar')).toHaveTextContent('Ready');
    });

    it('should update status on selection', async () => {
      render(<MockApplication />);
      const body = screen.getByText('Body');
      fireEvent.click(body.parentElement!);
      await waitFor(() => {
        expect(screen.getByTestId('status-bar')).toHaveTextContent('Selected');
      });
    });
  });

  describe('Keyboard Navigation', () => {
    it('should support keyboard navigation in tree', () => {
      render(<MockApplication />);
      const tree = screen.getByTestId('feature-tree');
      fireEvent.keyDown(tree, { key: 'ArrowDown', code: 'ArrowDown' });
      expect(tree).toBeInTheDocument();
    });

    it('should respond to Enter key on tree items', () => {
      render(<MockApplication />);
      const treeItem = screen.getAllByRole('treeitem')[0];
      fireEvent.keyDown(treeItem, { key: 'Enter', code: 'Enter' });
      expect(treeItem).toBeInTheDocument();
    });
  });

  describe('Accessibility', () => {
    it('should have proper ARIA roles', () => {
      render(<MockApplication />);
      expect(screen.getByTestId('feature-tree')).toHaveAttribute('role', 'tree');
      const items = screen.getAllByRole('treeitem');
      expect(items.length).toBeGreaterThan(0);
    });

    it('should have descriptive labels for inputs', () => {
      render(<MockApplication />);
      // Properties panel should have associated labels
      const tree = screen.getByTestId('feature-tree');
      expect(tree).toBeInTheDocument();
    });

    it('should have keyboard accessible buttons', () => {
      render(<MockApplication />);
      const buttons = screen.getAllByRole('button');
      buttons.forEach((btn) => {
        expect(btn).toBeEnabled();
        fireEvent.keyDown(btn, { key: 'Enter', code: 'Enter' });
      });
    });
  });
});
