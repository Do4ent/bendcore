/**
 * CoreBridge — связь главного процесса Electron с ядром C++ (bendcore_cli).
 *
 * Ядро работает отдельным процессом: запрос и ответ — по одной строке JSON
 * через stdin/stdout. Ответ сопоставляется с запросом по id. Если процесс ядра
 * упал, ожидающие запросы получают ошибку, а следующий запрос запускает ядро заново.
 * Модуль не зависит от Electron, поэтому проверяется обычным Node.
 */
import { spawn, ChildProcessWithoutNullStreams } from 'child_process';

export interface CoreResponse<T = unknown> {
  id: number | null;
  ok: boolean;
  data?: T;
  error?: string;
}

export class CoreError extends Error {
  constructor(message: string, public readonly command: string) {
    super(message);
    this.name = 'CoreError';
  }
}

interface Pending {
  resolve: (data: unknown) => void;
  reject: (err: Error) => void;
  timer: NodeJS.Timeout;
  command: string;
}

export interface CoreBridgeOptions {
  /** Путь к исполняемому файлу bendcore_cli. */
  executable: string;
  /** Тайм-аут одного запроса, мс. */
  timeoutMs?: number;
}

export class CoreBridge {
  private proc: ChildProcessWithoutNullStreams | null = null;
  private buffer = '';
  private nextId = 1;
  private readonly pending = new Map<number, Pending>();
  private readonly timeoutMs: number;

  constructor(private readonly options: CoreBridgeOptions) {
    this.timeoutMs = options.timeoutMs ?? 30_000;
  }

  /** Вызов команды ядра; возвращает data или бросает CoreError с текстом ядра. */
  call<T = unknown>(command: string, args: Record<string, unknown> = {}): Promise<T> {
    const proc = this.ensureProcess();
    const id = this.nextId++;
    return new Promise<T>((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pending.delete(id);
        reject(new CoreError(`ядро не ответило за ${this.timeoutMs} мс`, command));
      }, this.timeoutMs);
      this.pending.set(id, { resolve: resolve as (d: unknown) => void, reject, timer, command });
      proc.stdin.write(JSON.stringify({ id, command, args }) + '\n');
    });
  }

  /** Остановить процесс ядра (при выходе из приложения). */
  dispose(): void {
    this.failAll(new CoreError('ядро остановлено', ''));
    if (this.proc) {
      this.proc.stdin.end();
      this.proc.kill();
      this.proc = null;
    }
  }

  get running(): boolean {
    return this.proc !== null;
  }

  private ensureProcess(): ChildProcessWithoutNullStreams {
    if (this.proc) return this.proc;
    const proc = spawn(this.options.executable, [], { stdio: ['pipe', 'pipe', 'pipe'] });
    proc.stdout.setEncoding('utf8');
    proc.stdout.on('data', (chunk: string) => this.onData(chunk));
    proc.stderr.setEncoding('utf8');
    proc.stderr.on('data', (chunk: string) => console.error(`[bendcore_cli] ${chunk.trim()}`));
    proc.on('error', (err) => {
      this.proc = null;
      this.failAll(new CoreError(`не удалось запустить ядро: ${err.message}`, ''));
    });
    proc.on('exit', (code) => {
      this.proc = null;
      this.buffer = '';
      this.failAll(new CoreError(`процесс ядра завершился (код ${code ?? 'сигнал'})`, ''));
    });
    this.proc = proc;
    return proc;
  }

  private onData(chunk: string): void {
    this.buffer += chunk;
    let nl: number;
    while ((nl = this.buffer.indexOf('\n')) >= 0) {
      const line = this.buffer.slice(0, nl).trim();
      this.buffer = this.buffer.slice(nl + 1);
      if (line) this.onLine(line);
    }
  }

  private onLine(line: string): void {
    let msg: CoreResponse;
    try {
      msg = JSON.parse(line) as CoreResponse;
    } catch {
      console.error(`[bendcore_cli] неразборчивый ответ: ${line.slice(0, 200)}`);
      return;
    }
    if (typeof msg.id !== 'number') return;
    const p = this.pending.get(msg.id);
    if (!p) return;
    this.pending.delete(msg.id);
    clearTimeout(p.timer);
    if (msg.ok) p.resolve(msg.data);
    else p.reject(new CoreError(msg.error ?? 'ошибка ядра', p.command));
  }

  private failAll(err: Error): void {
    for (const [id, p] of this.pending) {
      clearTimeout(p.timer);
      p.reject(err);
      this.pending.delete(id);
    }
  }
}

/** Путь к bendcore_cli: переменная BENDCORE_CLI, ресурсы сборки или каталог build. */
export function defaultCliPath(resourcesPath: string | undefined, appRoot: string, platform: string): string {
  const exe = platform === 'win32' ? 'bendcore_cli.exe' : 'bendcore_cli';
  if (process.env.BENDCORE_CLI) return process.env.BENDCORE_CLI;
  if (resourcesPath && !resourcesPath.includes('node_modules')) return `${resourcesPath}/${exe}`;
  return `${appRoot}/build/${exe}`;
}
