# UTILIST Product Roadmap

## Milestone 1: Core Foundation & Protocol (Completed)
- [x] Length-prefixed framing and JSON IPC protocol over local Windows Named Pipes.
- [x] Manifest schema and validation.
- [x] Reference `HeartbeatUtility` development example.
- [x] Automated unit and integration test harness.

## Milestone 2: Qt 6 Control Panel & Lifecycle Management (In Progress)
- [ ] Qt 6 Widgets MainWindow with navigation sidebar (Home, Utilities, Shortcuts, Settings, About).
- [ ] Process Manager with Windows Job Objects (`AssignProcessToJobObject`).
- [ ] Asynchronous IPC client for UTILIST host.
- [ ] System tray integration (`QSystemTrayIcon`) with minimize-to-tray on close.
- [ ] Global shortcut manager (`RegisterHotKey`) with conflict detection.
- [ ] Configuration manager (`%LOCALAPPDATA%\Utilist\config.json`) with safe atomic writes.

## Milestone 3: Initial Utilities Integration
- [ ] **MTrail**: Smooth customizable cursor trail utility.
- [ ] **InputShield**: Sensitive input lock utility with emergency recovery workflows.
- [ ] **Desktop Drift**: Animated desktop toy car with obstacle recognition.

## Milestone 4: Packaging and Distribution
- [ ] Standalone installer packaging (MSIX / Inno Setup).
- [ ] Automated release pipeline via GitHub Actions.
