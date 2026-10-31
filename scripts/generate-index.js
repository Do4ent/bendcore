#!/usr/bin/env node
/**
 * Generate index.html for dist/web with current webpack bundle names
 */
const fs = require('fs');
const path = require('path');

const webDir = path.join(__dirname, '../dist/web');
const indexPath = path.join(webDir, 'index.html');

// Find webpack bundle files
const files = fs.readdirSync(webDir).filter(f => !f.endsWith('.map') && !f.endsWith('.LICENSE.txt'));
const cssFiles = files.filter(f => f.endsWith('.css'));
const jsFiles = files.filter(f => f.endsWith('.js'));

// Order: runtime, react-vendors, vendors, app
const orderedJs = [
  ...jsFiles.filter(f => f.includes('runtime.')),
  ...jsFiles.filter(f => f.includes('react-vendors.')),
  ...jsFiles.filter(f => f.includes('vendors.') && !f.includes('react-vendors.')),
  ...jsFiles.filter(f => !f.includes('runtime.') && !f.includes('react-vendors.') && !f.includes('vendors.')),
];

const html = `<!DOCTYPE html>
<html lang="en">
  <head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <title>BendCore CAD</title>
    ${cssFiles.map(f => `    <link rel="stylesheet" href="./${f}" />`).join('\n')}
  </head>
  <body>
    <div id="root"></div>
    ${orderedJs.map(f => `    <script src="./${f}"><\/script>`).join('\n')}
  </body>
</html>
`;

fs.writeFileSync(indexPath, html, 'utf-8');
console.log(`Generated ${indexPath}`);
