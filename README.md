# UTILIST

**UTILIST** is a lightweight, modern, and extensible Windows desktop utility platform built with modern C++ (C++20), Qt 6 Widgets, and native Win32 APIs.

Unlike simple application launchers, UTILIST provides a robust, isolated management environment for native Windows desktop utilities (such as cursor effects, input security, and desktop overlay games). Each utility runs as an independent process communicating over local Windows Named Pipes, ensuring maximum stability, crash isolation, and framework freedom.

---

## Architecture at a Glance

- **Process Isolation**: Utilities run as separate child executables encapsulated within Windows Job Objects (`JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`). If a utility encounters a fatal error, UTILIST remains intact and reports diagnostic information.
- **Native IPC**: High-speed, secure local Windows Named Pipes (`\\.\pipe\utilist.<id>.<nonce>`) using 4-byte length-prefixed framing and UTF-8 JSON payloads.
- **Independent Utilities**: Utilities do not depend on Qt. They can be built using standard C++20, Win32, DirectX, or GDI, linking only against the zero-dependency `utilist_shared` library or implementing the protocol independently.
- **Separation of Concerns**: Distinct representation of _Enabled_, _Process Lifecycle_ (`STOPPED`, `STARTING`, `RUNNING`, `STOPPING`, `ERROR`), and _Feature_ states.
- **Global Shortcut Routing**: Centralized Windows hotkey registration (`RegisterHotKey`) with conflict detection, persistence, and dispatch.

---

## Directory Structure

```
UTILIST/
|-- CMakeLists.txt              # Root build configuration
|-- CMakePresets.json           # Presets for MSVC Debug and Release
|-- docs/                       # Architecture, IPC protocol, security, and guides
|   |-- architecture.md
|   |-- ipc-protocol.md
|   |-- utility-integration-guide.md
|   |-- configuration.md
|   |-- security-model.md
|   |-- build-guide.md
|   |-- testing-guide.md
|   |-- roadmap.md
|   `-- decisions/              # Architectural Decision Records (ADRs)
|-- app/                        # Main UTILIST Qt 6 GUI application
|-- shared/                     # Reusable C++20 protocol, framing, and models
|-- utilities/                  # Manifest schema and utility ecosystem
|   `-- examples/
|       `-- heartbeat-utility/  # Reference development utility
|-- tests/                      # Unit and integration test suites
|-- scripts/                    # PowerShell build and test automation
`-- .github/workflows/          # Windows CI workflow
```

---

## Prerequisites

1. **Operating System**: Windows 10 (version 1809+) or Windows 11, x64.
2. **Compiler & Tools**: Visual Studio 2022 (Community, Professional, or Enterprise) with the **Desktop development with C++** workload (MSVC v143 or newer, Windows 10/11 SDK).
3. **CMake**: CMake 3.20 or newer (included with Visual Studio 2022).
4. **GUI Framework**: Qt 6 Widgets (Qt 6.5+ recommended for MSVC 2022 x64).

---

## Building UTILIST

UTILIST supports standardized CMake presets:

### Using CMake Presets directly

```powershell
# Configure Debug build
cmake --preset windows-msvc-debug

# Build targets
cmake --build --preset windows-msvc-debug

# Run automated tests
ctest --preset windows-msvc-debug --output-on-failure
```

### Using Automated Helper Scripts

```powershell
# Configure
.\scripts\configure.ps1 -Preset windows-msvc-debug

# Build
.\scripts\build.ps1 -Preset windows-msvc-debug

# Run unit and integration tests
.\scripts\test.ps1 -Preset windows-msvc-debug
```

---

## Documentation

- [Architecture Specification](docs/architecture.md)
- [IPC Protocol Specification](docs/ipc-protocol.md)
- [Utility Integration Guide](docs/utility-integration-guide.md)
- [Configuration Specification](docs/configuration.md)
- [Security Model](docs/security-model.md)
- [Build Guide](docs/build-guide.md)
- [Testing Guide](docs/testing-guide.md)
- [Roadmap](docs/roadmap.md)

---

## License

To be determined. Please consult [LICENSE](LICENSE) for details.
