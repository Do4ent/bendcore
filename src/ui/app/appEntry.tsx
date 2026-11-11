// Entry of the application shell (web/app/webpack.config.js, docs/ui/APP_SHELL.md).
import React from 'react';
import ReactDOM from 'react-dom/client';
import AppShell from './AppShell';

const root = document.getElementById('root');
if (!root) throw new Error('app shell: #root element is missing');
ReactDOM.createRoot(root).render(<AppShell />);
