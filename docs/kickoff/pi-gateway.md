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

1. Inspect the repository and record which Pi OS, interface names, and networking services are **not yet confirmed**. Do not select an AP/network-manager configuration based on guesses.
2. **First coding milestone:** prepare a minimal, OS-independent `tc` scenario **inspection and configuration script** for the documented 5 Mbit/s FIFO treatment, with a configurable Ethernet interface, safe inspection commands, and a clear rollback procedure. **Do not apply it to a live network.**
3. Check shell syntax and `tc` command construction; optionally test in a disposable Linux VM/network namespace, not on the host's active interface. Stop and explain what was actually verified.
4. Later add the documented HTB/DSCP treatment and scenario switching using small, reviewable changes.
5. **Only after inspecting the real Pi OS and networking services**, prepare the appropriate AP/routing configuration, then apply and verify connectivity on hardware before enabling QoS.

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

**Start by inspecting the repository and implementing only the first safe, OS-independent `tc` scripting milestone.**
