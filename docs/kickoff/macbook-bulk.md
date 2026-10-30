# Kickoff: MacBook Bulk UDP Traffic Generator

**Repository:** https://github.com/lumigoh/adaptive-qos-iot  
**Suggested branch, only if code is necessary:** `feat/bulk-traffic`

## Objective

Configure a macOS MacBook to generate controlled UDP Bulk traffic for our gateway QoS experiment.

**Prefer existing `iperf3` commands over writing custom software.**

## Project specifications

Read `README.md`, `AGENTS.md`, `docs/architecture.md`, and `docs/testing.md`.

Target configuration:
- MacBook connects to the Raspberry Pi 5 Wi-Fi AP.
- UDP destination: Kubuntu NUC at `192.168.60.2:5201`.
- Offered load: 8 Mbit/s.
- UDP payload: 1200 bytes.
- DSCP: `CS1` (TOS `0x20`).
- Experiment: 10-second warm-up and 60-second measurement.
- Save run-specific JSON results.
- Verify actual DSCP marking and received throughput.

The existing testing document contains starting commands. Validate their behaviour against the installed `iperf3` version, including warm-up duration and JSON output, before relying on them for formal testing.

## Development approach

Keep this component as simple as possible.

1. Determine whether `iperf3` is installed and which version is available. If you cannot access the MacBook, provide commands for me to run.
2. **First milestone:** prepare and verify a minimal UDP connectivity test to the NUC.
3. Stop and review the result before introducing full experiment settings.
4. Add a small shell script only if repeated commands justify it.

Do not introduce Python wrappers, dashboards, services, or elaborate automation without a demonstrated need.

Do not assume setting TOS guarantees the packet retains its intended DSCP value; verify it using actual packet evidence.

Do not automatically commit or merge changes without my request.

**Start with the minimum macOS setup and a simple connectivity test.**
