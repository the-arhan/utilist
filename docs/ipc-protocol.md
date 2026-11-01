# UTILIST IPC Protocol Specification (v1.0)

## 1. Transport & Framing

UTILIST and its managed utility processes communicate over local Windows Named Pipes.

### Pipe Name Format
```
\\.\pipe\utilist.<utility_id>.<session_nonce>
```
Example:
```
\\.\pipe\utilist.example.heartbeat.a1b2c3d4
```

### Framing Specification
Every message on the pipe is prefixed by a 32-bit unsigned integer (4 bytes, little-endian) denoting the length of the following JSON payload in bytes:

```
+-----------------------------+------------------------------------+
| Payload Length (uint32_le)  |       JSON Payload (UTF-8)         |
|         4 bytes             |             N bytes                |
+-----------------------------+------------------------------------+
```

- **Maximum payload length**: 64 KB (65,536 bytes) to prevent resource exhaustion attacks.
- Senders must transmit the 4-byte header followed immediately by the payload.
- Receivers buffer data until the specified length is received before parsing.

## 2. Security & Access Control
- Pipes are created with a security descriptor restricting access strictly to the current Windows user SID (Local System / Current User token).
- Network connections to the pipe are prohibited (`PIPE_REJECT_REMOTE_CLIENTS`).

## 3. Message Format

All messages are UTF-8 encoded JSON objects.

### Request Message
```json
{
  "protocol_version": 1,
  "request_id": "req-1001",
  "type": "command",
  "command": "activate",
  "payload": {}
}
```

Fields:
- `protocol_version` (integer, required): Must be `1`.
- `request_id` (string, required): Unique identifier for request/response correlation.
- `type` (string, required): `"command"`, `"query"`, or `"handshake"`.
- `command` (string, required for `"command"`): e.g., `"start"`, `"stop"`, `"activate"`, `"deactivate"`, `"toggle"`, `"pause"`, `"resume"`, `"get_status"`, `"update_settings"`.
- `payload` (object, optional): Arguments specific to the command.

### Response Message
```json
{
  "protocol_version": 1,
  "request_id": "req-1001",
  "type": "response",
  "success": true,
  "error": null,
  "payload": {
    "feature_active": true,
    "status_message": "Feature activated successfully"
  }
}
```

Fields:
- `protocol_version` (integer, required): Must be `1`.
- `request_id` (string, required): Corresponds to the matching request.
- `type` (string, required): `"response"`.
- `success` (boolean, required): `true` if successful, `false` otherwise.
- `error` (object or null, required):
  - `code` (string): Error code (e.g., `"UNSUPPORTED_COMMAND"`, `"INVALID_PAYLOAD"`, `"BUSY"`).
  - `message` (string): Human-readable error description.
- `payload` (object, optional): Result data.

### Status Notification / Status Response Payload
When UTILIST requests status (`get_status`) or the utility sends periodic status updates:
```json
{
  "feature_active": true,
  "state_description": "Input shield active",
  "capabilities": ["status", "activate", "deactivate", "settings"],
  "custom_metrics": {
    "uptime_seconds": 3600
  }
}
```

## 4. Standard Commands

| Command | Capability Required | Description |
|---|---|---|
| `handshake` | (core) | Verifies protocol compatibility and utility identity |
| `get_status` | `status` | Queries current feature state and health |
| `activate` | `activate` | Engages the visual effect or utility feature |
| `deactivate` | `deactivate` | Disengages the visual effect or utility feature |
| `toggle` | `toggle` | Toggles feature activation on/off |
| `pause` | `pause` | Temporarily suspends utility operation |
| `resume` | `resume` | Resumes utility operation |
| `update_settings`| `settings` | Updates runtime utility settings |
| `shutdown` | (core) | Requests graceful utility termination |
