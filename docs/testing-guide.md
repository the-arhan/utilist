# UTILIST Testing Guide

UTILIST includes both unit and end-to-end integration tests registered with CTest.

## 1. Test Architecture

### 1.1 Unit Tests (`tests/unit/test_shared.cpp`)
- **Framing**: Verifies length-prefixed stream framing, chunked buffer reconstitution, and oversized payload rejection.
- **Protocol JSON**: Verifies serialization, deserialization, request-response matching, and error payload propagation.
- **Manifest Validation**: Verifies schema validation, executable name safety (disallows path traversal `..`), and capability matching.

### 1.2 Integration Tests (`tests/integration/test_ipc_lifecycle.cpp`)
- Verifies real child process spawning (`CreateProcessW`) of `HeartbeatUtility.exe`.
- Establishes local Windows Named Pipe connection (`\\.\pipe\utilist.test.lifecycle.<pid>`).
- Performs handshake negotiation and queries status.
- Issues activation and deactivation commands and verifies internal state change.
- Issues graceful shutdown command and verifies process termination within 3000ms with exit code 0.

## 2. Running Tests

Run all tests via CTest:
```powershell
ctest --preset windows-msvc-debug --output-on-failure
```
Or via script:
```powershell
.\scripts\test.ps1 -Preset windows-msvc-debug
```
