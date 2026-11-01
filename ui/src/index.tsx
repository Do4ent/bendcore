/**
 * BendCore UI Entry Point
 * Main application root component
 */

import React from 'react';
import { createRoot } from 'react-dom/client';
import { ImportWithViewport } from './components/Import/ImportWithViewport';
import './styles/global.css';

// Get root DOM element
const rootElement = document.getElementById('root');

if (!rootElement) {
  throw new Error('Root element not found. Make sure index.html has a <div id="root"></div>');
}

// Create React root and render app
const root = createRoot(rootElement);

root.render(
  <React.StrictMode>
    <div style={{
      width: '100%',
      height: '100vh',
      fontFamily: '"Segoe UI", Tahoma, Geneva, Verdana, sans-serif',
      backgroundColor: '#fafafa',
      color: '#333'
    }}>
      <ImportWithViewport />
    </div>
  </React.StrictMode>
);

export default ImportWithViewport;
