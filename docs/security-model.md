# UTILIST Security Model

## 1. Principle of Least Privilege
- UTILIST runs as standard user privilege. It does not require or request UAC administrator elevation for normal operation.
- Managed utilities inherit standard user privilege.

## 2. Process Execution Boundaries
- Arbitrary executable paths are prohibited. UTILIST only executes binaries found within validated utility directories with an authentic `manifest.json`.
- Utilities must not contain relative paths escaping their root (`..` path traversal is rejected).
- Process creation uses Win32 `CreateProcessW` directly without passing strings to `cmd.exe` or `powershell.exe`, preventing command injection vulnerabilities.

## 3. IPC Access Controls
- Named pipes are established with explicit Security Descriptors granting read/write access solely to the current user token.
- Remote connections over the network are explicitly rejected with `PIPE_REJECT_REMOTE_CLIENTS`.
- Maximum message size is strictly capped at 64 KB to mitigate DoS memory exhaustion.
- Message parsing limits JSON recursion depth.

## 4. Input Safety (InputShield Utility Boundary)
- UTILIST core **never** implements system-wide input hooking or blocking.
- When controlling safety utilities like `InputShield`, the core assumes protection is **INACTIVE** unless explicitly confirmed via an active IPC heartbeat.
- If IPC communication with `InputShield` drops or is delayed, the status immediately transitions to `ERROR / UNKNOWN` rather than displaying a false sense of security.
- Utilities must provide an emergency recovery shortcut or timeout independent of UTILIST.
