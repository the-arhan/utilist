# ADR 0002: Windows Named Pipes as IPC Transport

## Context
A communication mechanism between the UTILIST host and isolated utility processes is needed. Options considered:
- Windows Named Pipes
- Local loopback TCP sockets
- Shared memory with Win32 events
- Windows Messages (`WM_COPYDATA`)

## Decision
We chose **local Windows Named Pipes** (`\\.\pipe\...`).

### Rationale:
1. **Security**: Named pipes support native Windows Access Control Lists (ACLs) to ensure only the local logged-in user can connect. Loopback TCP sockets can be accessed by any unprivileged local process on the machine unless complex token handshakes are implemented, and may trigger firewall prompts.
2. **Reliability and Framing**: Named pipes integrate directly with Windows I/O completion and overlapped asynchronous I/O.
3. **Simplicity over WM_COPYDATA**: `WM_COPYDATA` requires utilities to maintain a message window and does not work well across integrity levels or for non-GUI worker utilities.

## Consequences
- Requires length-prefixed message framing to handle chunked stream buffers.
- Requires robust pipe disconnection and reconnect handling.
