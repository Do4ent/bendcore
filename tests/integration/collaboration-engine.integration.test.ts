/**
 * @jest-environment node
 *
 * CollaborationEngine integration tests: a real `ws` server with real
 * WebSocket clients. Runs in the node environment because `ws` resolves to
 * its browser stub under jsdom.
 */

import WebSocket from 'ws';
import { validate as isUuid } from 'uuid';
import { CollaborationEngine } from '../../src/collaboration/CollaborationEngine';

type ServerMessage = { type: string; [key: string]: any };

/** Client that records every message and can wait for a given type. */
class TestClient {
  readonly messages: ServerMessage[] = [];
  private waiters: Array<{ type: string; resolve: (m: ServerMessage) => void }> = [];

  private constructor(readonly ws: WebSocket) {
    ws.on('message', (data) => {
      const msg = JSON.parse(data.toString()) as ServerMessage;
      this.messages.push(msg);
      this.waiters = this.waiters.filter((w) => {
        if (w.type === msg.type) {
          w.resolve(msg);
          return false;
        }
        return true;
      });
    });
  }

  static connect(port: number): Promise<TestClient> {
    return new Promise((resolve, reject) => {
      const ws = new WebSocket(`ws://127.0.0.1:${port}`);
      const client = new TestClient(ws);
      ws.once('open', () => resolve(client));
      ws.once('error', reject);
    });
  }

  /** Resolve with the next (or an already received, unconsumed) message of a type. */
  next(type: string, timeoutMs = 2000): Promise<ServerMessage> {
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => reject(new Error(`timed out waiting for ${type}`)), timeoutMs);
      this.waiters.push({
        type,
        resolve: (m) => {
          clearTimeout(timer);
          resolve(m);
        },
      });
    });
  }

  send(msg: ServerMessage): void {
    this.ws.send(JSON.stringify(msg));
  }

  async join(userId: string, userName: string): Promise<ServerMessage> {
    const sync = this.next('SYNC');
    this.send({ type: 'JOIN', userId, userName });
    return sync;
  }

  close(): Promise<void> {
    return new Promise((resolve) => {
      if (this.ws.readyState === WebSocket.CLOSED) return resolve();
      this.ws.once('close', () => resolve());
      this.ws.close();
    });
  }
}

const waitFor = async (condition: () => boolean, timeoutMs = 2000) => {
  const start = Date.now();
  while (!condition()) {
    if (Date.now() - start > timeoutMs) throw new Error('condition not met in time');
    await new Promise((r) => setTimeout(r, 10));
  }
};

describe('CollaborationEngine (integration)', () => {
  let engine: CollaborationEngine;
  let port: number;
  let clients: TestClient[];

  beforeEach(async () => {
    jest.spyOn(console, 'log').mockImplementation(() => undefined);
    engine = new CollaborationEngine(0);
    await engine.start();
    port = engine.getPort();
    clients = [];
  });

  afterEach(async () => {
    await Promise.all(clients.map((c) => c.close()));
    await engine.stop();
    jest.restoreAllMocks();
  });

  const connect = async () => {
    const client = await TestClient.connect(port);
    clients.push(client);
    return client;
  };

  test('listens on an assigned port and tracks sessions', async () => {
    expect(port).toBeGreaterThan(0);
    await connect();
    await connect();
    await waitFor(() => engine.getSessionCount() === 2);
    expect(engine.getActiveUsers()).toEqual([]);
  });

  test('sends a full sync with a uuid session id on JOIN', async () => {
    const alice = await connect();
    const sync = await alice.join('alice', 'Alice');

    expect(isUuid(sync.sessionId)).toBe(true);
    expect(sync.operations).toEqual([]);
    expect(sync.presence).toEqual([
      expect.objectContaining({ userId: 'alice', userName: 'Alice' }),
    ]);
    expect(engine.getActiveUsers()).toEqual(['alice']);
  });

  test('broadcasts presence when users join and leave', async () => {
    const alice = await connect();
    await alice.join('alice', 'Alice');
    const bob = await connect();

    const presenceAfterJoin = alice.next('PRESENCE_UPDATE');
    await bob.join('bob', 'Bob');
    const joined = await presenceAfterJoin;
    expect(joined.presence.map((p: any) => p.userId).sort()).toEqual(['alice', 'bob']);

    const presenceAfterLeave = alice.next('PRESENCE_UPDATE');
    await bob.close();
    const left = await presenceAfterLeave;
    expect(left.presence.map((p: any) => p.userId)).toEqual(['alice']);
    await waitFor(() => engine.getSessionCount() === 1);
  });

  test('records operations and relays them to other sessions only', async () => {
    const alice = await connect();
    const bob = await connect();
    await alice.join('alice', 'Alice');
    await bob.join('bob', 'Bob');

    const emitted = new Promise<any>((resolve) => engine.once('operation', resolve));
    const relayed = bob.next('OPERATION_UPDATE');
    alice.send({
      type: 'OPERATION',
      operationType: 'UPDATE',
      objectId: 'tube-1',
      property: 'diameter',
      oldValue: 20,
      newValue: 25,
    });

    const { operation } = await relayed;
    expect(isUuid(operation.id)).toBe(true);
    expect(operation).toEqual(
      expect.objectContaining({
        userId: 'alice',
        type: 'UPDATE',
        objectId: 'tube-1',
        property: 'diameter',
        oldValue: 20,
        newValue: 25,
      })
    );
    expect(await emitted).toEqual(operation);
    expect(engine.getOperationLog()).toEqual([operation]);
    expect(alice.messages.some((m) => m.type === 'OPERATION_UPDATE')).toBe(false);

    // A user joining later receives the operation history
    const carol = await connect();
    const sync = await carol.join('carol', 'Carol');
    expect(sync.operations).toEqual([operation]);
  });

  test('ignores operations from sessions that have not joined', async () => {
    const anonymous = await connect();

    // Messages on one connection are handled in order, so once the SYNC for
    // the later JOIN arrives the earlier OPERATION has been processed.
    anonymous.send({ type: 'OPERATION', operationType: 'CREATE', objectId: 'x', property: 'p', newValue: 1 });
    const sync = await anonymous.join('late', 'Late Joiner');

    expect(sync.operations).toEqual([]);
    expect(engine.getOperationLog()).toEqual([]);
  });

  test('relays cursor updates to other users', async () => {
    const alice = await connect();
    const bob = await connect();
    await alice.join('alice', 'Alice');
    await bob.join('bob', 'Bob');

    const cursor = bob.next('CURSOR_UPDATE');
    alice.send({
      type: 'CURSOR_MOVE',
      position: { x: 1, y: 2, z: 3 },
      camera: { x: 10, y: 20, z: 30 },
    });

    expect((await cursor).cursor).toEqual({
      userId: 'alice',
      position: { x: 1, y: 2, z: 3 },
      camera: { x: 10, y: 20, z: 30 },
    });
  });

  test('survives malformed messages', async () => {
    const errorSpy = jest.spyOn(console, 'error').mockImplementation(() => undefined);
    const alice = await connect();
    alice.ws.send('not json');
    await waitFor(() => errorSpy.mock.calls.length > 0);

    const sync = await alice.join('alice', 'Alice');
    expect(sync.type).toBe('SYNC');
  });

  test('clears the operation log', async () => {
    const alice = await connect();
    await alice.join('alice', 'Alice');
    const emitted = new Promise((resolve) => engine.once('operation', resolve));
    alice.send({ type: 'OPERATION', operationType: 'CREATE', objectId: 'a', property: 'p', newValue: 1 });
    await emitted;

    expect(engine.getOperationLog()).toHaveLength(1);
    engine.clearOperationLog();
    expect(engine.getOperationLog()).toEqual([]);
  });
});
