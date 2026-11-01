# Utility Integration Guide

This guide explains how independent developers can build native C++ utilities that integrate with UTILIST.

## 1. Integration Model

Utilities are standalone Windows executables (`.exe`). UTILIST launches them as child processes, monitors their process lifecycle, and communicates with them over a local Windows Named Pipe.

Independent utilities:
- Do **not** need to link against Qt.
- Can use Win32, DirectX, GDI, Direct2D, or any UI/graphics stack.
- Can link against `utilist_shared` (pure C++20 Win32 named pipe transport) or implement the protocol independently.

## 2. Step-by-Step Integration

### Step 1: Create the Utility Manifest
Every utility must include a `manifest.json` in its directory:
```json
{
  "$schema": "manifest-schema.json",
  "schema_version": 1,
  "id": "vendor.myutility",
  "name": "My Utility",
  "description": "Short explanation of utility function.",
  "version": "1.0.0",
  "executable": "MyUtility.exe",
  "protocol_version": 1,
  "capabilities": [
    "status",
    "activate",
    "deactivate",
    "toggle"
  ],
  "default_shortcut": "Ctrl+Shift+U"
}
```

### Step 2: Receive Pipe Argument
When UTILIST launches your executable, it passes `--utilist-pipe=\\.\pipe\utilist.<id>.<nonce>`.

Your application should parse this argument and connect to the named pipe or initialize the named pipe server:
```cpp
// Example C++ command line check
std::wstring pipeName;
for (int i = 1; i < argc; ++i) {
    std::wstring arg = argv[i];
    if (arg.rfind(L"--utilist-pipe=", 0) == 0) {
        pipeName = arg.substr(15);
    }
}
```

### Step 3: Implement the IPC Loop
Using `utilist_shared` library:
```cpp
#include <utilist/protocol/pipe_channel.hpp>
#include <utilist/protocol/messages.hpp>

// Connect and handle messages
utilist::PipeChannel channel;
if (channel.connect(pipeName)) {
    channel.onMessage([](const utilist::Message& req) {
        if (req.command == "activate") {
            // Activate your visual feature
            channel.sendResponse(req.requestId, true, "Activated");
        }
    });
}
```

### Step 4: Handle Graceful Shutdown
When the utility receives a `"shutdown"` command or when the pipe closes, the utility should clean up resources (free hooks, close windows, stop threads) and exit cleanly within 3 seconds.
