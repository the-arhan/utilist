# ADR 0003: JSON for Configuration and Manifests

## Context
A data serialization format was needed for:
1. Application and utility configuration files (`config.json`).
2. Utility metadata and registration manifests (`manifest.json`).
3. Inter-process communication message payloads over named pipes.

Options considered:
- JSON
- YAML
- Windows Registry
- Protocol Buffers (protobuf) / FlatBuffers

## Decision
We chose **JSON** formatted with standard UTF-8 encoding.

### Rationale:
1. **Universality**: JSON is supported natively in Qt (`QJsonDocument`, `QJsonObject`), and easily parsed in standard C++ with lightweight, widely-adopted header libraries or native parsers without heavy external dependencies.
2. **Human-readability**: Allows users and developers to inspect and edit configuration and manifests directly using any text editor.
3. **Low Complexity**: Avoids protobuf compilation steps and binary schema evolution overhead while maintaining strict schema validation at runtime.
4. **Tooling**: Standard JSON Schema (`manifest-schema.json`) can be used to validate utility manifests in CI and IDEs.
