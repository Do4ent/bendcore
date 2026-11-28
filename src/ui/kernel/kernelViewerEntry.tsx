// Entry of the kernel viewer page (web/kernel-viewer/webpack.config.js).
import React from 'react';
import ReactDOM from 'react-dom/client';
import KernelViewerApp from './KernelViewerApp';

const root = document.getElementById('root');
if (!root) throw new Error('kernel viewer: #root element is missing');
ReactDOM.createRoot(root).render(<KernelViewerApp />);
