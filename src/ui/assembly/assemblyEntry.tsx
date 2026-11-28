// Entry of the assembly page (web/assembly/webpack.config.js).
import React from 'react';
import ReactDOM from 'react-dom/client';
import AssemblyApp from './AssemblyApp';

const root = document.getElementById('root');
if (!root) throw new Error('assembly page: #root element is missing');
ReactDOM.createRoot(root).render(<AssemblyApp />);
