// Стенд новых панелей: интерфейс + настоящее ядро (bendcore_cli за локальным HTTP-сервером).
// Сборка: esbuild; запуск: node server.mjs; проверка: shots.mjs (Playwright).
import React, { useState } from 'react';
import { createRoot } from 'react-dom/client';
import { CoreClient, type CoreTransport } from '../../src/ui/core/coreClient';
import CheckPanel from '../../src/ui/CheckPanel';
import BendDataDialog from '../../src/ui/BendDataDialog';
import InstancesPanel from '../../src/ui/InstancesPanel';
import DrawingWorkspace from '../../src/ui/DrawingWorkspace';
import { SketchConfirmCorner, AxisTriad } from '../../src/ui/SketchControls';
import type { Axis } from '../../src/ui/core/axisLock';
import type { CheckTubeInput, PathItem } from '../../src/ui/core/types';

const http: CoreTransport = async (command, args) => {
  const r = await fetch('/call', { method: 'POST', body: JSON.stringify({ command, args }) });
  const j = await r.json();
  if (!j.ok) throw new Error(j.error);
  return j.data;
};
const client = new CoreClient(http);

const spatial: PathItem[] = [
  { kind: 'straight', length: 120 }, { kind: 'bend', radius: 40, angle: 90, rotation: 0 },
  { kind: 'straight', length: 80 }, { kind: 'bend', radius: 40, angle: 45, rotation: 90 },
  { kind: 'straight', length: 60 }, { kind: 'bend', radius: 40, angle: 60, rotation: -30 },
  { kind: 'straight', length: 150 },
];
const tubes: CheckTubeInput[] = [
  { id: 'Т-01', path: spatial, od: 20, L_min: 40, B_max: 120 },
  { id: 'Т-02', path: [{ kind: 'straight', length: 300 }], od: 20, placement: { origin: [0, -24, 0] } },
  { id: 'Т-03', path: [{ kind: 'straight', length: 100 }, { kind: 'bend', radius: 40, angle: 90 }, { kind: 'straight', length: 15 },
    { kind: 'bend', radius: 40, angle: 90 }, { kind: 'straight', length: 100 }], od: 20, L_min: 40, placement: { origin: [0, 400, 0] } },
  { id: 'Т-04', path: [{ kind: 'straight', length: 200 }, { kind: 'bend', radius: 40, angle: 150 }, { kind: 'straight', length: 100 }],
    od: 20, B_max: 120, placement: { origin: [0, 800, 0] } },
];

const App: React.FC = () => {
  const view = new URLSearchParams(location.search).get('view') ?? 'check';
  const [axis, setAxis] = useState<Axis | null>('z');
  const [msg, setMsg] = useState('');
  return (
    <div style={{ height: '100vh', display: 'flex', flexDirection: 'column' }}>
      {view === 'check' && <CheckPanel tubes={tubes} client={client} onNavigate={(t, e) => setMsg(`→ ${t} #${e}`)} />}
      {view === 'bend' && (
        <BendDataDialog open client={client} onClose={() => {}} onSave={(f, c) => setMsg(`${f}\n${c}`)}
          tube={{ name: 'БК.100.001', path: spatial, E_A: 10, E_B: 12, bothOrders: true }} />
      )}
      {view === 'bend-blocked' && (
        <BendDataDialog open client={client} onClose={() => {}} tube={{ name: 'Т-04', path: tubes[3].path, blocksOutput: true }} />
      )}
      {view === 'instances' && <InstancesPanel client={client} />}
      {view === 'drawing' && (
        <DrawingWorkspace client={client} role="Выпускающий" user="Петров"
          tube={{ path: spatial, od: 20, wall: 1, title: { 'Обозначение': 'БК.100.001', 'Наименование': 'Трубка', 'Материал': 'Труба Cu-DHP 20×1 EN 12449' },
            unfold: { N_first: 160, E_A: 10, E_B: 12, round_step: 0.5 } }} />
      )}
      {view === 'sketch' && (
        <div style={{ position: 'relative', flex: 1, background: '#dfe6ec' }}>
          <SketchConfirmCorner onFinish={() => setMsg('finish')} onCancel={() => setMsg('cancel')} />
          <AxisTriad locked={axis} onChange={setAxis} />
        </div>
      )}
      <pre id="msg" style={{ position: 'fixed', bottom: 0, right: 0, margin: 0, fontSize: 11 }}>{msg}</pre>
    </div>
  );
};
createRoot(document.getElementById('root')!).render(<App />);
