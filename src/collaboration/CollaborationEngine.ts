import WebSocket, { RawData } from 'ws';
import type { AddressInfo } from 'net';
import { EventEmitter } from 'events';
import * as uuid from 'uuid';

// Type definitions

interface Session {
  id: string;
  userId?: string;
  userName?: string;
  ws: WebSocket;
  lastActivity: number;
  vectorTime: Record<string, number>;
}

interface Operation {
  id: string;
  userId: string;
  timestamp: number;
  type: 'CREATE' | 'UPDATE' | 'DELETE' | 'MOVE';
  objectId: string;
  property: string;
  oldValue: any;
  newValue: any;
  vectorTime: Record<string, number>;
}

interface Message {
  type:
    | 'JOIN'
    | 'OPERATION'
    | 'CURSOR_MOVE'
    | 'CURSOR_UPDATE'
    | 'SYNC'
    | 'PRESENCE_UPDATE'
    | 'OPERATION_UPDATE';
  [key: string]: any;
}

interface PresenceInfo {
  userId: string;
  userName: string;
  lastActivity: number;
  cursorPosition?: { x: number; y: number; z: number };
}

interface CursorUpdate {
  userId: string;
  position: { x: number; y: number; z: number };
  camera: { x: number; y: number; z: number };
}

// Main collaboration engine

export class CollaborationEngine extends EventEmitter {
  private server: WebSocket.Server;
  private sessions: Map<string, Session> = new Map();
  private operationLog: Operation[] = [];
  private vectorClocks: Map<string, number> = new Map();
  private port: number;

  constructor(port: number = 8080) {
    super();
    this.port = port;
    this.server = new WebSocket.Server({ port });
  }

  // Start the collaboration server
  async start(): Promise<void> {
    return new Promise((resolve) => {
      this.server.on('connection', (ws: WebSocket) => {
        this.handleNewConnection(ws);
      });

      const onListening = () => {
        console.log(`Collaboration server listening on port ${this.getPort()}`);
        resolve();
      };
      // The server starts listening from the constructor; if that already
      // happened, 'listening' will not fire again.
      if (this.server.address()) {
        onListening();
      } else {
        this.server.once('listening', onListening);
      }
    });
  }

  // Handle new WebSocket connection
  private handleNewConnection(ws: WebSocket): void {
    const sessionId = uuid.v4();
    const session: Session = {
      id: sessionId,
      ws,
      lastActivity: Date.now(),
      vectorTime: {}
    };

    this.sessions.set(sessionId, session);
    console.log(`New session: ${sessionId}`);

    ws.on('message', (data: RawData) => {
      try {
        const msg: Message = JSON.parse(data.toString());
        this.handleMessage(sessionId, msg);
      } catch (err) {
        console.error('Error parsing message:', err);
      }
    });

    ws.on('close', () => {
      this.sessions.delete(sessionId);
      this.broadcastPresence();
      console.log(`Session closed: ${sessionId}`);
    });

    ws.on('error', (error) => {
      console.error(`WebSocket error for session ${sessionId}:`, error);
    });
  }

  // Route messages
  private handleMessage(sessionId: string, msg: Message): void {
    switch (msg.type) {
      case 'JOIN':
        this.handleJoin(sessionId, msg);
        break;
      case 'OPERATION':
        this.handleOperation(sessionId, msg);
        break;
      case 'CURSOR_MOVE':
        this.handleCursorMove(sessionId, msg);
        break;
      default:
        console.warn(`Unknown message type: ${msg.type}`);
    }
  }

  // Handle JOIN message
  private handleJoin(sessionId: string, msg: any): void {
    const session = this.sessions.get(sessionId);
    if (!session) return;

    session.userId = msg.userId;
    session.userName = msg.userName;
    this.vectorClocks.set(sessionId, 0);

    // Send full sync to new user
    const syncMessage: Message = {
      type: 'SYNC',
      operations: this.operationLog,
      presence: this.getPresence(),
      sessionId: sessionId
    };

    try {
      session.ws.send(JSON.stringify(syncMessage));
    } catch (err) {
      console.error('Error sending sync:', err);
    }

    this.broadcastPresence();
  }

  // Handle OPERATION message
  private handleOperation(sessionId: string, msg: any): void {
    const session = this.sessions.get(sessionId);
    if (!session || !session.userId) return;

    session.lastActivity = Date.now();

    // Create operation
    const vectorTime = { ...session.vectorTime };
    const current_time = (this.vectorClocks.get(sessionId) || 0) + 1;
    this.vectorClocks.set(sessionId, current_time);
    vectorTime[sessionId] = current_time;

    const operation: Operation = {
      id: uuid.v4(),
      userId: session.userId,
      timestamp: Date.now(),
      type: msg.operationType,
      objectId: msg.objectId,
      property: msg.property,
      oldValue: msg.oldValue,
      newValue: msg.newValue,
      vectorTime
    };

    // Transform operation against concurrent operations
    const transformed = this.transformOperation(operation);

    // Add to log
    this.operationLog.push(transformed);

    // Broadcast to all other sessions
    this.broadcastToOthers(sessionId, {
      type: 'OPERATION_UPDATE',
      operation: transformed
    });

    // Emit event
    this.emit('operation', transformed);
  }

  // Operational Transformation (OT) conflict resolution
  private transformOperation(op: Operation): Operation {
    const transformed = { ...op };

    // Check against all previous operations
    for (let i = this.operationLog.length - 1; i >= 0; i--) {
      const existing = this.operationLog[i];

      if (this.isCauslyConcurrent(op.vectorTime, existing.vectorTime)) {
        // Apply transformation rules
        transformed.newValue = this.resolveConflict(
          op.objectId,
          existing.objectId,
          op.newValue,
          existing.newValue,
          op.type,
          existing.type
        );
      }
    }

    return transformed;
  }

  // Check if two operations are causally concurrent
  private isCauslyConcurrent(
    vc1: Record<string, number>,
    vc2: Record<string, number>
  ): boolean {
    let vc1_greater = false;
    let vc2_greater = false;

    const all_keys = new Set([...Object.keys(vc1), ...Object.keys(vc2)]);

    for (const key of all_keys) {
      const v1 = vc1[key] || 0;
      const v2 = vc2[key] || 0;

      if (v1 > v2) vc1_greater = true;
      if (v2 > v1) vc2_greater = true;
    }

    return vc1_greater && vc2_greater;
  }

  // Conflict resolution strategy
  private resolveConflict(
    id1: string,
    id2: string,
    val1: any,
    val2: any,
    type1: string,
    type2: string
  ): any {
    // Simple conflict resolution: lexicographic ordering by object ID
    if (id1 > id2) {
      return val1;
    } else if (id1 < id2) {
      return val2;
    } else {
      // Same object: last write wins
      return val1;
    }
  }

  // Handle cursor movement
  private handleCursorMove(sessionId: string, msg: any): void {
    const session = this.sessions.get(sessionId);
    if (!session || !session.userId) return;

    session.lastActivity = Date.now();

    const cursorUpdate: CursorUpdate = {
      userId: session.userId,
      position: msg.position,
      camera: msg.camera
    };

    // Broadcast to all other sessions
    this.broadcastToOthers(sessionId, {
      type: 'CURSOR_UPDATE',
      cursor: cursorUpdate
    });
  }

  // Broadcast presence information
  private broadcastPresence(): void {
    const presence = this.getPresence();

    for (const session of this.sessions.values()) {
      try {
        session.ws.send(JSON.stringify({
          type: 'PRESENCE_UPDATE',
          presence
        }));
      } catch (err) {
        console.error('Error sending presence update:', err);
      }
    }

    this.emit('presence', presence);
  }

  // Get current presence info
  private getPresence(): PresenceInfo[] {
    return Array.from(this.sessions.values())
      .filter(s => s.userId)
      .map(s => ({
        userId: s.userId!,
        userName: s.userName || 'Anonymous',
        lastActivity: s.lastActivity
      }));
  }

  // Broadcast to all except sender
  private broadcastToOthers(excludeSessionId: string, msg: Message): void {
    for (const [sessionId, session] of this.sessions) {
      if (sessionId !== excludeSessionId) {
        try {
          session.ws.send(JSON.stringify(msg));
        } catch (err) {
          console.error(`Error broadcasting to session ${sessionId}:`, err);
        }
      }
    }
  }

  // Public API methods

  /** Port the server is bound to (resolves port 0 to the assigned port). */
  public getPort(): number {
    const address = this.server.address();
    return address && typeof address === 'object' ? (address as AddressInfo).port : this.port;
  }

  public getSessionCount(): number {
    return this.sessions.size;
  }

  public getActiveUsers(): string[] {
    return Array.from(this.sessions.values())
      .filter(s => s.userId)
      .map(s => s.userId!);
  }

  public getOperationLog(): Operation[] {
    return [...this.operationLog];
  }

  public clearOperationLog(): void {
    this.operationLog = [];
  }

  // Graceful shutdown
  public async stop(): Promise<void> {
    return new Promise((resolve) => {
      // Close all connections
      for (const session of this.sessions.values()) {
        session.ws.close();
      }
      this.sessions.clear();

      // Close server
      this.server.close(() => {
        console.log('Collaboration server closed');
        resolve();
      });
    });
  }
}

// Export factory function
export function createCollaborationEngine(port: number = 8080): CollaborationEngine {
  return new CollaborationEngine(port);
}
