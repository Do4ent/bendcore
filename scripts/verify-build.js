#!/usr/bin/env node
/**
 * Verify that the application is ready for building
 */
const fs = require('fs');
const path = require('path');

const checks = [
  {
    name: 'package.json exists',
    test: () => fs.existsSync('package.json'),
  },
  {
    name: 'dist/main.js exists',
    test: () => fs.existsSync('dist/main.js'),
  },
  {
    name: 'dist/preload.js exists',
    test: () => fs.existsSync('dist/preload.js'),
  },
  {
    name: 'dist/web/index.html exists',
    test: () => fs.existsSync('dist/web/index.html'),
  },
  {
    name: 'webpack bundles present',
    test: () => {
      const files = fs.readdirSync('dist/web');
      return files.some(f => f.match(/main\.\w+\.js$/));
    },
  },
  {
    name: 'node_modules installed',
    test: () => fs.existsSync('node_modules/electron'),
  },
  {
    name: 'electron-builder installed',
    test: () => fs.existsSync('node_modules/electron-builder'),
  },
];

console.log('=== Build Readiness Check ===\n');

let passed = 0;
let failed = 0;

checks.forEach(check => {
  const result = check.test();
  const status = result ? '✅' : '❌';
  console.log(`${status} ${check.name}`);
  if (result) passed++;
  else failed++;
});

console.log(`\n${passed}/${checks.length} checks passed`);

if (failed > 0) {
  console.error(`\n❌ Build verification failed (${failed} checks failed)`);
  process.exit(1);
} else {
  console.log('\n✅ Application is ready for building');
  process.exit(0);
}
