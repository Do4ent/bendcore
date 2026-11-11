// Entry of the drawing editor page (web/drawing/webpack.config.js).
import React from 'react';
import ReactDOM from 'react-dom/client';
import DrawingEditorApp from './DrawingEditorApp';

const root = document.getElementById('root');
if (!root) throw new Error('drawing editor: #root element is missing');
ReactDOM.createRoot(root).render(<DrawingEditorApp />);
