import '@testing-library/jest-dom';

/**
 * BendCore Test Setup
 * Global test configuration and mocks
 */

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
