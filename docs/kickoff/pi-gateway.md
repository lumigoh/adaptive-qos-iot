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

## Development approach — software first

Physical Pi access is limited. **Prepare scripts and checks now; apply and verify networking on the Pi later.** On hardware, validate basic connectivity **before** enabling traffic shaping.

1. Inspect the repository and note which Pi OS, interface names, and networking services are **not yet confirmed**.
2. **First coding milestone:** draft a minimal, reviewable AP/routing configuration or setup script with explicit assumptions, configurable interfaces, safe inspection steps, and a rollback procedure. Do not apply it to any live network.
3. Check shell syntax and configuration logic where possible. If an isolated Linux VM/network namespace is available, use it for optional checks without touching the host's main networking.
4. Stop and report verified checks versus untested Pi-specific behaviour.
5. Later prepare separate FIFO/HTB QoS scenario scripts and counter-inspection commands, using the exact settings in `docs/testing.md`.

When hardware access becomes available, verify the Pi OS/interface names, AP, IPv4 forwarding, NUC return route, and end-to-end connectivity first. Only then apply the 5 Mbit/s egress shaper and test DSCP class counters.

## Safety and verification

- Avoid replacing existing network configuration blindly.
- Before any disruptive change, explain the expected effect, access risks, and recovery procedure.
- Do not run connectivity-breaking commands without approval.
- Preserve SSH access where possible.
- Distinguish static checks or disposable Linux tests from verified routing, bandwidth limits, or classification on the Pi.
- Confirm DSCP classification through observed packet headers and `tc` counters.

Keep changes understandable and reproducible. Avoid unnecessary dependencies, frameworks, or orchestration systems.

Work on the feature branch; do not automatically commit, push, or merge without my request.

**Start by inspecting the repository and identifying the first safe AP setup step.**
