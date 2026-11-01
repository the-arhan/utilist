# UTILIST Architecture Specification

## 1. System Overview

UTILIST is an extensible Windows desktop utility platform and central control panel built with C++20 and Qt 6 Widgets. It manages native Windows desktop utilities running as isolated child processes communicating via local named pipes.

```
+-----------------------------------------------------------------------+
|                            UTILIST Core                               |
|                                                                       |
|  +--------------------+  +--------------------+  +-----------------+  |
|  |  MainWindow / UI   |  |   TrayController   |  | ShortcutManager |  |
|  |  (Qt 6 Widgets)    |  |  (QSystemTrayIcon) |  | (RegisterHotKey)|  |
|  +---------+----------+  +---------+----------+  +--------+--------+  |
|            |                       |                      |           |
|            v                       v                      v           |
|  +-----------------------------------------------------------------+  |
|  |                     ApplicationController                       |  |
|  +---------------------------------+-------------------------------+  |
|                                    |                                  |
|         +--------------------------+--------------------------+       |
|         |                          |                          |       |
|         v                          v                          v       |
|  +--------------+          +---------------+          +---------------+
|  |UtilityManager|          |ProcessManager |          | Configuration |
|  |  (Registry)  |          | (Job Objects) |          |    Manager    |
|  +------+-------+          +-------+-------+          +---------------+
|         |                          |                                  |
|         |                          | (CreateProcessW / AssignToJob)   |
|         |                          v                                  |
|         |              [Child Utility Process]                        |
|         |                          |                                  |
|         | (Named Pipe Client)      | (Named Pipe Server)              |
|         +------------------------->|                                  |
|               IPC Commands / State | \\.\pipe\utilist.<id>.<nonce>    |
+------------------------------------+----------------------------------+
```

## 2. Core Subsystems

### 2.1 Utility Registry & Discovery
- Scans registered directories (e.g., `%LOCALAPPDATA%\Utilist\Utilities` and local development folders).
- Reads and strictly validates `manifest.json` against `manifest-schema.json`.
- Maintains descriptors containing utility metadata, capability flags, and path constraints.

### 2.2 Process Manager
- Launches utilities using Win32 `CreateProcessW` without shell redirection.
- Encapsulates child processes in a Windows **Job Object** configured with `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE` to prevent orphaned processes if the host abruptly terminates.
- Tracks process health via retained `HANDLE` and Windows wait objects, avoiding name-based identification.
- Implements bounded backoff and crash tracking; disables utilities that crash repeatedly.

### 2.3 IPC Layer (Inter-Process Communication)
- Utilizes local Windows Named Pipes: `\\.\pipe\utilist.<id>.<nonce>`.
- Client/Server architecture: UTILIST host acts as client connecting to utility or server hosting bidirectional endpoint with explicit local DACL security.
- Clear framing: 4-byte little-endian message length prefix followed by UTF-8 JSON payload.
- Asynchronous non-blocking I/O using overlapped named pipe calls or dedicated worker threads to guarantee the GUI thread never stalls.

### 2.4 Lifecycle & State Machine
Separate representation of:
- **Enabled State**: Configuration preference allowing utility to run and respond to shortcuts.
- **Process State**: `STOPPED`, `STARTING`, `RUNNING`, `STOPPING`, `ERROR`.
- **Feature State**: Utility-specific activation (e.g., trail active, input shielded, car driving) reported via IPC.

### 2.5 Shortcut Manager
- Global keyboard hotkeys registered via Win32 `RegisterHotKey` / `UnregisterHotKey`.
- Centralized conflict detection and remapping.
- Hotkey events dispatched to `ApplicationController` which sends capability-checked IPC commands to the appropriate target utility.

### 2.6 Persistence & Diagnostics
- Configuration stored in `%LOCALAPPDATA%\Utilist\config.json`.
- Rotating, structured log files written to `%LOCALAPPDATA%\Utilist\logs\utilist.log`.
- Safe atomic file writes using temporary files and replace operations to prevent corruption.
