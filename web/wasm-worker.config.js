// The WASM worker (src/ui/worker/wasmWorker.ts, docs/ui/APP_SHELL.md) is an extra entry of every
// page configuration; it is emitted under the fixed name the page starts it by (wasm-worker.js).
const WORKER_ENTRY = 'wasm-worker';

function workerFileName(pageFileName) {
  return (pathData) => (pathData.chunk && pathData.chunk.name === WORKER_ENTRY ? `${WORKER_ENTRY}.js` : pageFileName);
}

// The worker is started as one classic script: it carries its own runtime (runtime: false keeps
// it out of the shared runtime chunk) and nothing of it is split into shared chunks.
const workerEntry = { import: './src/ui/worker/wasmWorker.ts', runtime: false };

function workerOptimization(optimization) {
  return {
    ...optimization,
    splitChunks: { ...optimization.splitChunks, chunks: (chunk) => chunk.name !== WORKER_ENTRY },
  };
}

module.exports = { WORKER_ENTRY, workerEntry, workerFileName, workerOptimization };
