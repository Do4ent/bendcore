/**
 * Export and mass properties of part tabs in the shell (docs/ui/DATA_EXCHANGE.md). The part
 * editor keeps its document in its own worker; the shell works on the part JSON the editor
 * reports after every completed change (the same text that is saved and kept as a draft): one
 * shared modeling worker, started on first use, loads that JSON (the part is rebuilt by the
 * kernel) and writes the file or computes the mass properties. The document stays loaded until
 * the JSON changes.
 */

import type { ModelingKernel, PartDocumentHandle } from '../../wasm/bendcore_modeling_api.mjs';
import type { Remote, WasmClient } from '../worker/wasmClient';
import type { ExportFormat, ExportOptions, MassProperties } from './exchange';

export type StartModeling = () => Promise<WasmClient<ModelingKernel>>;

export class PartExchange {
  #start: StartModeling;
  #client: Promise<WasmClient<ModelingKernel>> | null = null;
  #loaded: { text: string; doc: Promise<Remote<PartDocumentHandle>> } | null = null;
  /** Calls run one after another, so that a document is never disposed while in use. */
  #queue: Promise<unknown> = Promise.resolve();

  constructor(start: StartModeling) {
    this.#start = start;
  }

  /** The worker client once started (for the busy indicator and idle()); null before. */
  client(): Promise<WasmClient<ModelingKernel>> | null {
    return this.#client;
  }

  #run<T>(body: () => Promise<T>): Promise<T> {
    const task = this.#queue.then(body);
    this.#queue = task.catch(() => undefined);
    return task;
  }

  async #document(text: string): Promise<Remote<PartDocumentHandle>> {
    if (this.#loaded && this.#loaded.text === text) return this.#loaded.doc;
    if (!this.#client) {
      this.#client = this.#start();
      this.#client.catch(() => {
        this.#client = null;
      });
    }
    const client = await this.#client;
    const previous = this.#loaded;
    const doc = client.root.loadDocument(text).then((r) => r.document);
    this.#loaded = { text, doc };
    doc.catch(() => {
      if (this.#loaded?.doc === doc) this.#loaded = null;
    });
    if (previous) void previous.doc.then((d) => d.dispose()).catch(() => undefined);
    return doc;
  }

  exportFile(text: string, format: ExportFormat, options: ExportOptions): Promise<Uint8Array> {
    return this.#run(async () => (await this.#document(text)).exportFile(format, options));
  }

  /** null when the part has no solid body. */
  massProperties(text: string, density: number): Promise<MassProperties | null> {
    return this.#run(async () => {
      const doc = await this.#document(text);
      const props = await doc.properties('final', null);
      if (props.volume === null) return null;
      return doc.massProperties(density);
    });
  }

  dispose(): void {
    const client = this.#client;
    this.#client = null;
    this.#loaded = null;
    void client?.then((c) => c.dispose()).catch(() => undefined);
  }
}
