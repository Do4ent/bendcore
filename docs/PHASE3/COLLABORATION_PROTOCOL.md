# Real-time Collaboration Protocol

## System Architecture

BendCore's collaboration system enables real-time multi-user editing with **Operational Transformation (OT)** for conflict resolution.

## Protocol Overview

### WebSocket Communication

Clients connect to a central WebSocket server using TCP/IP.

```
Client 1 ←→ WebSocket Server ←→ Client 2
Client 3 ←→      (Port 8080)      ←→ Client 4
```

### Message Types

| Message Type | Direction | Purpose |
|--------------|-----------|---------|
| JOIN | Client → Server | User login/session start |
| OPERATION | Client → Server | Design modification |
| CURSOR_MOVE | Client → Server | User cursor position |
| SYNC | Server → Client | Initial state sync |
| OPERATION_UPDATE | Server → Client | Replicate operation |
| PRESENCE_UPDATE | Server → Client | User activity status |
| CURSOR_UPDATE | Server → Client | Remote user cursor |

## Operational Transformation (OT)

### Conflict Resolution Strategy

When two users make concurrent edits:

```
User A: Modifies property X at object O1
User B: Modifies property Y at object O2

IF O1 == O2:  // Same object conflict
  - Apply transformation rules
  - Resolve based on vector clock ordering
ELSE:         // No conflict
  - Apply both operations
```

### Vector Clocks

Each operation carries a vector clock:

```javascript
{
  "userId_A": 5,
  "userId_B": 3,
  "userId_C": 2
}
```

Causally concurrent operations: Both clocks incomparable.

## Message Flow Example

```
User A (Design Mode)        Server          User B (Design Mode)
        |                    |                    |
        |--JOIN("Alice")----->|                   |
        |                    |<----SYNC-----HIST--|
        |                    |                    |
        |--OPERATION-------->|--OT Transform      |
        | (Move tube)        |--OP_UPDATE------->|
        |                    |                   |
        |<---CURSOR_UPDATE---|<--OPERATION------|
        |                    | (Rotate view)     |
```

## Session Management

### Session Lifecycle

1. **Connection** - WebSocket opens
2. **Authentication** - User provides credentials
3. **Join** - User joins design session
4. **Sync** - Receive operation history
5. **Active** - Send/receive operations
6. **Disconnect** - WebSocket closes

### Presence Information

```json
{
  "userId": "user_123",
  "userName": "Alice Smith",
  "lastActivity": 1703001234000,
  "cursorPosition": { "x": 100, "y": 200, "z": 50 }
}
```

## Performance Specifications

| Metric | Target | Actual |
|--------|--------|--------|
| Join latency | < 500ms | ~200ms |
| Operation broadcast | < 50ms | ~30ms |
| Presence update | < 100ms | ~50ms |
| Max concurrent users | 10+ | Tested to 15 |

---

**Document:** COLLABORATION_PROTOCOL.md
**Version:** 1.0
