import '@testing-library/jest-dom';
import { TextDecoder as NodeTextDecoder, TextEncoder as NodeTextEncoder } from 'util';

/**
 * BendCore Test Setup
 * Global test configuration and mocks
 */

// jest-environment-jsdom does not expose the WHATWG encoding API that browsers
// and Electron provide; use Node's spec-compliant implementation.
if (typeof globalThis.TextEncoder === 'undefined') {
  globalThis.TextEncoder = NodeTextEncoder as unknown as typeof globalThis.TextEncoder;
}
if (typeof globalThis.TextDecoder === 'undefined') {
  globalThis.TextDecoder = NodeTextDecoder as unknown as typeof globalThis.TextDecoder;
}

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

// Mock HTMLCanvasElement.getContext (WebGL not available in jsdom)
HTMLCanvasElement.prototype.getContext = jest.fn().mockReturnValue({
  clearRect: jest.fn(),
  fillRect: jest.fn(),
  drawImage: jest.fn(),
  beginPath: jest.fn(),
  moveTo: jest.fn(),
  lineTo: jest.fn(),
  stroke: jest.fn(),
  fill: jest.fn(),
  arc: jest.fn(),
  save: jest.fn(),
  restore: jest.fn(),
  scale: jest.fn(),
  translate: jest.fn(),
  rotate: jest.fn(),
  createLinearGradient: jest.fn().mockReturnValue({
    addColorStop: jest.fn(),
  }),
  measureText: jest.fn().mockReturnValue({ width: 0 }),
  fillText: jest.fn(),
  strokeText: jest.fn(),
  strokeRect: jest.fn(),
  closePath: jest.fn(),
  rect: jest.fn(),
  clip: jest.fn(),
  quadraticCurveTo: jest.fn(),
  bezierCurveTo: jest.fn(),
  setLineDash: jest.fn(),
  getLineDash: jest.fn().mockReturnValue([]),
  setTransform: jest.fn(),
  resetTransform: jest.fn(),
  transform: jest.fn(),
  getImageData: jest.fn().mockReturnValue({ data: new Uint8ClampedArray(4) }),
  putImageData: jest.fn(),
  canvas: { width: 800, height: 600 },
});

// Mock global fetch with a default no-op implementation to prevent crashes in tests
// that trigger network calls unexpectedly (e.g. loading a localStorage draft).
// Individual tests can override with jest.fn().mockResolvedValueOnce(...).
global.fetch = jest.fn().mockResolvedValue({
  ok: false,
  status: 503,
  json: async () => ({ message: 'Not mocked' }),
});

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
