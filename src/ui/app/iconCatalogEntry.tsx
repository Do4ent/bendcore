// Entry of the icon catalog page (web/icons, /icons.html of the shell's server).
import React from 'react';
import { createRoot } from 'react-dom/client';
import IconCatalog from './IconCatalog';

createRoot(document.getElementById('root')!).render(<IconCatalog />);
