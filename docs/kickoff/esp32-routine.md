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

## Development approach — software first

Physical hardware access is limited. Work incrementally and preserve Critical behaviour.

**First coding milestone:** add the Routine profile to the existing shared firmware and extend its **host-side packet tests** to verify `Q1` fields, ASCII space padding, and exactly 256 bytes. Do not build a second firmware application.

If the shared firmware is not available yet, stop at a short list of required profile settings instead of creating competing code. If the board/toolchain is available, compile both profiles; otherwise note the outstanding compile check.

Stop after the first milestone, explaining the tests performed and what still needs hardware. Later milestones cover the 50 ms transmission schedule, ACK/RTT handling, logging, and physical Wi-Fi/DSCP verification. Keep local tests distinct from hardware results.

Do not invent hardware test results or modify shared protocol specifications independently. Keep changes focused and do not automatically commit, push, or merge without my request.

**Start by inspecting the shared firmware foundation and identifying the smallest compatible Routine implementation.**
