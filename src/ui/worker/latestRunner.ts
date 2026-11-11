/**
 * Runs asynchronous jobs one at a time, keeping only the newest job that arrives while one is
 * running (docs/ui/APP_SHELL.md). Pointer moves solve the sketch in the worker at the rate the
 * worker answers, not at the rate of events: intermediate positions are skipped, the last one is
 * always solved.
 */

export class LatestRunner {
  private running: Promise<void> | null = null;
  private queued: (() => Promise<void>) | null = null;

  /** Starts `job` now, or after the running job (replacing a job queued earlier). */
  schedule(job: () => Promise<void>): void {
    if (this.running) {
      this.queued = job;
      return;
    }
    this.start(job);
  }

  private start(job: () => Promise<void>) {
    this.running = job()
      .catch(() => undefined)
      .then(() => {
        const next = this.queued;
        this.queued = null;
        this.running = null;
        if (next) this.start(next);
      });
  }

  /** Drops the queued job (the running one finishes). */
  clear(): void {
    this.queued = null;
  }

  /** Resolves when no job is running or queued. */
  async settled(): Promise<void> {
    while (this.running) await this.running;
  }
}
