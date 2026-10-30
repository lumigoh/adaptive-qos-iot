# Kickoff: Raspberry Pi 5 Access Point and QoS Gateway

**Repository:** https://github.com/lumigoh/adaptive-qos-iot  
**Branch:** `feat/pi-gateway`

## Objective

Implement the Raspberry Pi 5 as the Wi-Fi access point, IPv4 router, and controlled QoS gateway.

The primary experiment measures prioritisation at the **Pi's Ethernet egress bottleneck**.

## Project specifications

Read `README.md`, `AGENTS.md`, `docs/architecture.md`, and `docs/testing.md`.

Required capabilities:
- Wi-Fi AP for both ESP32s and the MacBook.
- Proposed Wi-Fi subnet: `192.168.50.0/24`.
- Proposed Ethernet subnet: `192.168.60.0/24`.
- Forward traffic to the separate Kubuntu NUC without unnecessary NAT.
- Apply a consistent 5 Mbit/s Ethernet egress bottleneck in every scenario.
- Support Baseline, Congested FIFO, and Congested classified QoS.
- Implement Linux `tc` TBF/HTB configurations and DSCP classification exactly as specified in `docs/testing.md`.
- Provide a simple, repeatable way to apply scenarios, inspect counters, and restore configuration.

Do not implement adaptive QoS or WMM extensions.

## Development approach

**Prioritise working networking before traffic shaping.**

1. Inspect the repository and available information about the Pi's OS, interfaces, and existing network configuration.
2. Identify anything requiring physical inspection. Do not pretend the agent has access to hardware unless it actually does.
3. **First milestone:** establish or prepare the minimum safe Wi-Fi AP configuration.
4. Explain and verify that milestone before progressing to Ethernet routing.
5. Introduce FIFO shaping, classified QoS, and measurement scripts only after basic networking works.

## Safety and verification

- Avoid replacing existing network configuration blindly.
- Before any disruptive change, explain the expected effect, access risks, and recovery procedure.
- Do not run connectivity-breaking commands without approval.
- Preserve SSH access where possible.
- Never claim routing, bandwidth limits, or classification work without actual evidence.
- Confirm DSCP classification through observed packet headers and `tc` counters.

Keep changes understandable and reproducible. Avoid unnecessary dependencies, frameworks, or orchestration systems.

Work on the feature branch; do not automatically commit, push, or merge without my request.

**Start by inspecting the repository and identifying the first safe AP setup step.**
