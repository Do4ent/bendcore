// Entry of the part modeling page (web/part-modeler/webpack.config.js).
import React from 'react';
import ReactDOM from 'react-dom/client';
import PartModelerApp from './PartModelerApp';

const root = document.getElementById('root');
if (!root) throw new Error('part modeler: #root element is missing');
ReactDOM.createRoot(root).render(<PartModelerApp />);
