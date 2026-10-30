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

1. Identify the `iperf3` version and installation method on the available development machine. If you cannot access it, give me concise commands to run.
2. **First milestone:** start a local `iperf3` server and UDP client on `127.0.0.1`; check that a JSON result is produced. Do not require the Pi or NUC yet.
3. Stop and review this local result before adding 8 Mbit/s, 1200-byte payloads, `-O 10`, run-specific output names, and CS1 marking.
4. Add a small shell script only if repeated commands justify it. Before writing timed-run automation, coordinate its start/stop behaviour with the shared run lifecycle in `docs/testing.md`. Later verify the real MacBook → Pi → NUC path and inspect DSCP packet markings on hardware.

Do not introduce Python wrappers, dashboards, services, or elaborate automation without a demonstrated need.

Do not assume setting TOS guarantees the packet retains its intended DSCP value; verify it using actual packet evidence **later**. Save local-loopback `iperf3` output outside `results/<run_id>/`; label it as a software check, never an experiment result.

Do not automatically commit or merge changes without my request.

**Start with the minimum macOS setup and a simple connectivity test.**
