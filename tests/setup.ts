import '@testing-library/jest-dom';

/**
 * BendCore Test Setup
 * Global test configuration and mocks
 */

// Mock window.matchMedia (not available in jsdom)
Object.defineProperty(window, 'matchMedia', {
  writable: true,
  value: jest.fn().mockImplementation((query: string) => ({
    matches: false,
    media: query,
    onchange: null,
    addListener: jest.fn(),
    removeListener: jest.fn(),
    addEventListener: jest.fn(),
    removeEventListener: jest.fn(),
    dispatchEvent: jest.fn(),
  })),
});

// Mock ResizeObserver (not available in jsdom)
global.ResizeObserver = jest.fn().mockImplementation(() => ({
  observe: jest.fn(),
  unobserve: jest.fn(),
  disconnect: jest.fn(),
}));

// Mock Electron IPC
(global.window as any).electron = {
  ipc: {
    invoke: jest.fn(),
    send: jest.fn(),
    on: jest.fn(),
    off: jest.fn(),
  },
};

// Suppress console warnings in tests
const originalConsoleWarn = console.warn;
beforeAll(() => {
  console.warn = (...args: any[]) => {
    if (
      typeof args[0] === 'string' &&
      args[0].includes('ReactDOM.render')
    ) {
      return;
    }
    originalConsoleWarn.call(console, ...args);
  };
});

afterAll(() => {
  console.warn = originalConsoleWarn;
});
