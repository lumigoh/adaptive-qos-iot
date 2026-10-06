# Adaptive QoS for a Wireless IoT Network

A three-person university project investigating whether gateway QoS can protect
critical IoT messages under congestion.

## Current milestone

**Stage 1: review the shared project foundation.** The architecture is agreed at
concept level; hardware operation and experiment results have not been verified.

```text
ESP32 critical ─┐
ESP32 normal ───┼─ Wi-Fi → Raspberry Pi AP/router → Ethernet → wired receiver
Bulk laptop ────┘
```

Later, the Pi will create a controlled Ethernet output bottleneck and apply fixed
QoS. Adaptive QoS follows only after fixed QoS works.

The intended claim concerns gateway QoS for Wi-Fi-connected devices. It does not
prove Wi-Fi airtime prioritisation; WMM is a separate later investigation.

## Shared technical knowledge

- [Architecture](docs/architecture.md): device roles, packet path, and scope.
- [Experiment plan](docs/experiment-plan.md): comparisons, measurements, and open choices.
- [Decision log](docs/decisions.md): agreed choices and their reasons.

Create source directories when implementation begins. There is no runnable system
or setup procedure yet.

## Collaboration

Use Markdown for durable technical knowledge, Issues for work, one GitHub Project
board for status, and pull requests for integration. Do not copy task lists or board
status into the documentation.

### Issues and board

Create one repository-linked Project board with **Todo → In progress → Review → Done**.
Use the board's Status field rather than duplicate status labels.

Each issue needs a short purpose, one owner, a reviewer, dependencies, and an
observable completion check. Link evidence in the issue or pull request. Mark work
Done after its completion check is met and any associated pull request is merged.

### Branches and review

- Start a short branch from current `main` for an issue, e.g. `docs/shared-foundation`.
- Keep each pull request focused; link its issue and explain the change and evidence.
- Ask one teammate to review technical decisions, interfaces, code, or configuration.
- Keep typo reviews lightweight. Discuss architecture or interface changes together.
- Let the group review and merge this foundation before starting implementation.

### Responsibility without silos

| Member | Primary focus | Cross-review focus |
|---|---|---|
| A | ESP32 / wireless traffic | Receiver message handling |
| B | Pi / networking / QoS | Firmware markings and packet path |
| C | Receiver / logging / analysis | Routing evidence and measurement validity |

All three own the architecture, experiment interpretation, report, and presentation.
Everyone should explain the complete packet path and participate in integration.

Before coding, agree on the firmware toolchain, addresses/ports, message and
acknowledgement format, traffic configuration, logging schema, and scenario names.
Record decisions; avoid independently inventing incompatible interfaces.

The final report may use Google Docs or Word. Choose one shared document later;
use repository documentation and recorded results as its technical source.

## Review checkpoint

Read the three documents together, resolve or explicitly defer open choices, and
set up the Issues/Project board. The next milestone is one ESP32 UDP message reaching
the wired receiver through the Pi. Stop at this checkpoint before implementation.
