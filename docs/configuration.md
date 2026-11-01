# UTILIST Configuration Specification

## 1. Storage Location

Configuration is stored per-user in:
```
%LOCALAPPDATA%\Utilist\config.json
```
For testing or portable execution, a configuration file in the working directory may be prioritized if present.

## 2. Configuration Schema (v1.0)

```json
{
  "schema_version": 1,
  "app": {
    "minimize_to_tray_on_close": true,
    "start_with_windows": false,
    "check_updates": false,
    "theme": "system",
    "log_level": "INFO"
  },
  "shortcuts": {
    "vendor.myutility": {
      "key_sequence": "Ctrl+Shift+U",
      "action": "toggle",
      "enabled": true
    }
  },
  "utilities": {
    "vendor.myutility": {
      "enabled": true,
      "auto_start": true,
      "custom_settings": {}
    }
  }
}
```

## 3. Resilience and Migration Strategy
- **Atomic Writes**: Writes are made to `config.json.tmp` and atomically replaced via `MoveFileExW` with `MOVEFILE_REPLACE_EXISTING`.
- **Corruption Recovery**: If `config.json` fails JSON validation or schema checks, it is preserved as `config.json.corrupt.<timestamp>` and a fresh default configuration is instantiated. Existing user data is never destroyed without a backup.
- **Transient State Isolation**: Runtime states (`STARTING`, `RUNNING`, `STOPPING`) are maintained in memory and never written to durable configuration.
