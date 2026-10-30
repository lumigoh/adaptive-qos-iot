# Agent Instructions

- Read [`README.md`](README.md) for the project overview, components, and getting started.
- Consult [`docs/`](docs/) for project documentation, including new files added later. Key references:
  - [`docs/architecture.md`](docs/architecture.md) — topology, addressing, device roles, traffic markings, and the shared UDP/ACK protocol.
  - [`docs/testing.md`](docs/testing.md) — traffic rates, QoS configurations, scenarios, metrics, logging, and experimental procedure.
  - [`docs/kickoff/`](docs/kickoff/) — component-specific starting prompts: [Critical ESP32](docs/kickoff/esp32-critical.md), [Routine ESP32](docs/kickoff/esp32-routine.md), [MacBook Bulk](docs/kickoff/macbook-bulk.md), [Pi gateway](docs/kickoff/pi-gateway.md), and [NUC receiver](docs/kickoff/nuc-receiver.md). Read the relevant prompt when starting that component.
- Use the architecture and testing documents as the technical specifications; kickoff prompts guide the initial implementation steps.
- **Hardware access is limited:** implement and test software locally first. Use lightweight host tests, loopback clients, compilation, and isolated Linux checks as appropriate; do not wait for devices to make software progress.
- Keep mocks/test fixtures separate from production traffic and results. Never present local or simulated checks as on-device validation; record what still needs hardware.
- Avoid hard-coded device interfaces, credentials, or addresses when local testing requires configuration. Never commit secrets.
- Work in small, reviewable steps. Avoid unnecessary abstractions.
- Keep changes compatible with other project components.
- Explain changes and how to verify them.
- Never claim hardware tests passed without actual evidence.
