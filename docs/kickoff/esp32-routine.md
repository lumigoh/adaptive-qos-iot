# Kickoff: ESP32 Routine Traffic Generator

**Repository:** https://github.com/lumigoh/adaptive-qos-iot  
**Suggested branch:** `feat/esp32-routine`

## Objective

Implement the Routine traffic profile using the **same firmware foundation as the Critical ESP32**.

Avoid building a second, independently maintained implementation.

## Project specifications

Read `README.md`, `AGENTS.md`, `docs/architecture.md`, and `docs/testing.md`.

The Routine ESP32 must eventually:
- Connect to the Raspberry Pi 5 Wi-Fi AP.
- Send 256-byte UDP messages at 20 messages/second.
- Mark packets DSCP `CS0` (TOS `0x00`).
- Use the same ASCII `Q1`/`A1` protocol as the Critical device.
- Identify its traffic class as `routine`.
- Track acknowledgements, RTT, timeouts, and experiment phases.
- Produce compatible sender logs.

## Shared firmware dependency

Inspect the Critical component's progress first.

- If the shared firmware is available, build directly on it.
- If it is being developed on another branch, coordinate the shared interface before changing overlapping code.
- If it is not ready, identify the configuration requirements and avoid creating competing firmware.

The two ESP32s should differ primarily in their traffic configuration.

## Development approach

Work incrementally, preserving existing functionality.

**First coding milestone:** demonstrate that the shared firmware produces a correctly formatted 256-byte Routine message.

Then stop and report what changed, what was verified, and what remains.

Add periodic transmission and ACK/RTT handling in later increments.

Do not invent hardware test results or modify shared protocol specifications independently. Keep changes focused and do not automatically commit, push, or merge without my request.

**Start by inspecting the shared firmware foundation and identifying the smallest compatible Routine implementation.**
