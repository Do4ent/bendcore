module.exports = {
  preset: 'ts-jest',
  testEnvironment: 'jsdom',
  roots: ['<rootDir>/src', '<rootDir>/tests'],
  testMatch: ['**/__tests__/**/*.ts?(x)', '**/?(*.)+(spec|test).ts?(x)'],
  testPathIgnorePatterns: [
    '/node_modules/',
    '/dist/',
    '<rootDir>/tests/e2e/',
    // Playwright specs of the kernel viewer and the part modeler (run by their own configs).
    '<rootDir>/tests/e2e-kernel/',
    '<rootDir>/tests/e2e-modeling/',
    '<rootDir>/tests/e2e-assembly/',
    '<rootDir>/tests/e2e-drawing/',
    '<rootDir>/tests/integration/comprehensive\\.test\\.ts',
    '<rootDir>/tests/integration/sketch-solver\\.integration\\.test\\.ts',
    '<rootDir>/tests/integration/import-export\\.integration\\.test\\.ts',
    '<rootDir>/tests/integration/profile-database\\.integration\\.test\\.ts',
    '<rootDir>/tests/integration/tube-generation\\.integration\\.test\\.ts',
    '<rootDir>/tests/SketchEditor\\.e2e\\.tsx',
    '<rootDir>/tests/comments\\.e2e\\.tsx',
    '<rootDir>/tests/search\\.e2e\\.tsx',
    '<rootDir>/tests/templates\\.e2e\\.tsx',
    '<rootDir>/tests/performance/',
    '<rootDir>/tests/regression/',
    '<rootDir>/tests/node/',
    '<rootDir>/tests/api/',
    '<rootDir>/tests/smoke/',
    // Фикстуры самотеста scripts/check_no_stubs.sh — данные проверки, а не тесты.
    '<rootDir>/tests/qa/fixtures/',
    // Спецификация Playwright (import '@playwright/test'), в Jest не исполняется.
    '<rootDir>/tests/web-app\\.spec\\.ts',
    // Проверка собранного Electron-приложения: нужны dist/ (npm run build) и бинарник
    // Electron; запуск — npm run test:electron-smoke.
    ...(process.env.BENDCORE_ELECTRON_SMOKE ? [] : ['<rootDir>/tests/electron-smoke\\.test\\.ts']),
  ],
  moduleFileExtensions: ['ts', 'tsx', 'js', 'jsx', 'json', 'node'],
  moduleNameMapper: {
    '\\.(css|less|scss|sass)$': 'identity-obj-proxy',
    '^three$': '<rootDir>/tests/__mocks__/three.js',
  },
  testTimeout: 15000,
  setupFilesAfterEnv: ['<rootDir>/tests/setup.ts'],
  collectCoverageFrom: [
    'src/**/*.{ts,tsx}',
    '!src/**/*.d.ts',
    '!src/main.ts',
    '!src/preload.ts',
  ],
  coveragePathIgnorePatterns: [
    '/node_modules/',
    '/dist/',
  ],
  transform: {
    '^.+\\.tsx?$': ['ts-jest', {
      diagnostics: false,
      tsconfig: {
        jsx: 'react-jsx',
        esModuleInterop: true,
        allowSyntheticDefaultImports: true,
        rootDir: '.',
      },
    }],
  },
};
