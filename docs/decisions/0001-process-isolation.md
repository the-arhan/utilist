# ADR 0001: Separate Process Isolation Over In-Process DLLs

## Context
UTILIST serves as a central manager for independently developed desktop utilities (e.g. cursor trails, input blockers, desktop mini-games). We needed to decide whether to load utilities as in-process dynamic libraries (`.dll`) or execute them as independent processes (`.exe`).

## Decision
We chose **out-of-process execution** with process isolation over in-process DLL loading for the following reasons:
1. **Crash Isolation**: If an experimental utility crashes (e.g., in a graphics driver or game loop), UTILIST core remains intact and can report the failure to the user.
2. **Framework Freedom**: In-process DLLs would tie every utility to the same C++ runtime and Qt version. Process isolation allows utilities to use pure Win32, DirectX, or different toolchains.
3. **Resource Management**: Terminating or updating an independent process is clean. Windows handles releasing all GDI/DirectX handles, hooks, and allocated memory upon process exit.
4. **Security Boundaries**: Prevents an unstable utility from corrupting the core memory space or shortcut hooks.

## Consequences
- Requires an IPC protocol (Windows Named Pipes) to synchronize state and commands.
- Slightly higher memory footprint per utility compared to in-proc DLLs, which is well within acceptable limits for modern desktop environments.
