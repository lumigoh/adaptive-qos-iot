# Repository Working Rules

## Project scope

This repository supports a university group project investigating whether
gateway QoS can protect critical IoT messages under controlled congestion.

Use `docs/architecture.md` as the source of truth for the current architecture.

Current high-level concept:

```text
ESP32 critical ─┐
ESP32 normal ───┼─ Wi-Fi → Raspberry Pi AP/router → Ethernet → wired receiver
Bulk laptop ────┘
```

The primary experiment concerns **gateway QoS at a controlled Raspberry Pi
Ethernet bottleneck**.

Do not claim that this alone proves Wi-Fi airtime prioritisation.
WMM / wireless airtime behaviour requires separate verification.

Fixed QoS should be established before adaptive QoS unless the group explicitly
changes that decision.

---

## Work incrementally

Prefer this progression:

1. agree on the design;
2. define interfaces;
3. prove small components;
4. integrate them;
5. establish controlled congestion;
6. implement fixed QoS;
7. run formal experiments;
8. consider adaptive QoS or WMM extensions.

Do not jump directly to a complete implementation.

Follow the **current milestone and active Issue** rather than automatically
starting later phases.

Every technical step should have an observable success condition.

---

## Shared source of truth

Use:

- `README.md` — project overview and current milestone
- `docs/architecture.md` — architecture, packet path, device roles, scope
- `docs/experiment-plan.md` — experiment design and measurement methodology
- `docs/decisions.md` — durable engineering decisions and reasoning
- GitHub Issues — bounded pieces of work
- GitHub Project — work status
- Pull Requests — review and integration

Use Markdown for durable technical knowledge.

Use Issues and the Project board for tasks and status.

Do not duplicate Kanban status or task lists into documentation.

Do not create placeholder directories or files before they contain useful work.

---

## Decisions and uncertainty

Clearly distinguish between:

- **agreed** — reviewed and accepted by the group
- **proposed** — awaiting group agreement
- **needs verification** — requires hardware/software evidence
- **unknown** — evidence is not yet available

Do not silently turn a proposal into an agreed decision.

If an agreed interface or architecture choice changes:

1. explain the reason;
2. update `docs/decisions.md`;
3. update affected documentation and implementation together;
4. obtain appropriate group review.

---

## GitHub workflow

Project status:

```text
Todo → In Progress → Review → Done
```

Issues should represent bounded, observable outcomes.

Meaningful Issues should normally include:

- goal;
- owner;
- reviewer;
- dependencies;
- completion criteria.

Use short-lived branches for meaningful changes to:

- code;
- networking configuration;
- interfaces;
- experiment logic;
- substantial documentation.

Keep Pull Requests focused.

Do not mix unrelated changes into one PR.

A task moves to **Done** only when its completion criteria are satisfied and any
required PR is merged.

Avoid substantial direct changes to `main` unless explicitly authorised.

---

## Team collaboration

Primary areas may include:

- ESP32 / wireless traffic
- Raspberry Pi / networking / QoS
- receiver / logging / analysis

These are ownership leads, not silos.

All group members should understand:

- the complete packet path;
- where congestion is created;
- where QoS acts;
- how measurements are collected;
- what the experiment can and cannot demonstrate.

Important interfaces and technical decisions should receive review from another
group member.

Avoid designs where only one person understands a critical component.

---

## Technical integrity

Never invent experiment results.

Do not present expected behaviour as measured behaviour.

Do not use timestamps from unsynchronised devices to claim measured one-way
latency.

Prefer measurements that can be independently checked.

Record enough information to reproduce experiment runs, including relevant:

- configuration;
- software versions;
- Git commit;
- traffic settings;
- experiment identifiers.

Keep invalid or failed experiment runs identifiable and record why they were
invalid rather than silently discarding them.

---

## Implementation discipline

Before adding a component:

1. state what it is meant to prove or enable;
2. identify its inputs and outputs;
3. define how success will be checked;
4. implement only what the current stage requires;
5. test it;
6. record useful evidence;
7. stop before expanding into unrelated work.

Prefer small, understandable implementations over large generated systems.

Avoid unnecessary:

- frameworks;
- dependencies;
- abstractions;
- automation;
- rewrites of already-working components.

Where practical, use one configurable ESP32 firmware codebase rather than
duplicating nearly identical firmware.

---

## Agent behaviour

Agents working in this repository are collaborators, not autonomous project owners.

Before substantial changes:

- inspect the relevant repository documentation;
- check the related Issue or requested task;
- preserve agreed architecture and terminology;
- identify whether affected choices are agreed, proposed, or unverified.

Agents should:

- explain consequential technical choices;
- make small, reviewable changes;
- preserve human understanding of the system;
- report what changed;
- report how the change was checked;
- stay within the requested milestone or Issue.

Agents should not:

- one-shot the entire project;
- silently redesign the architecture;
- implement later phases without being asked;
- create unnecessary files or directories;
- treat assumptions as verified facts;
- merge substantial changes without review unless explicitly instructed.

---

## When to surface a decision

Do not silently choose when a decision would materially affect:

- an agreed interface;
- the system architecture;
- experimental validity;
- measurement methodology;
- interpretation of results.

If the active Issue or an explicit instruction already authorises the choice,
proceed and document it.

Otherwise, present the decision and its consequences for group review before
making a consequential change.
