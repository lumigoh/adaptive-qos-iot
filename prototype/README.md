# Adaptive QoS — presentation simulation

An explanatory, client-side prototype of **fixed gateway QoS**. It is not the
ESP32/Pi implementation, an adaptive controller, or evidence about hardware.
Interactive simulation — illustrative behaviour, not measured experimental results.

## Run locally

Use Node **20.19+ or 22.12+** (Node 24 is suitable).

```sh
cd prototype
npm ci
npm run dev
```

Open the local URL printed by Vite (normally `http://localhost:5173`).
For the presentation, build and serve the production version:

```sh
npm run build
npm run preview
```

Open `http://localhost:4173`. Keep that terminal running. After dependencies are
installed, the build and app need no internet, account, API, database, or backend.
Do not open `dist/index.html` directly with `file://`; use the local server.
The static `dist/` directory can also be hosted later. No deployment is configured.

## A short live demo

1. Choose **Baseline**, then **Start**. Bulk is off; Critical and Normal remain responsive.
2. Choose **Congested — FIFO**. Running continues with a fresh run at high load.
   Within about 3–6 seconds the shared queue builds; triangles wait behind squares.
3. Choose **Congested — QoS**. This resets the run with the **same offered load and
   link capacity**. Critical latency falls; Bulk throughput falls and its queue stays busy.
4. Set Bulk load to **10%**. Below capacity, both policies behave similarly.
5. Use **Pause** to explain a queue, **Resume** to continue, and **Reset** to clear
   packets, counters, time, and metrics while retaining the current policy and load.

Scenario buttons are presets. Baseline selects Bulk off; leaving Baseline selects
100% Bulk. Switching between the congested policies preserves the slider value.
The slider can override a preset's load; the offered-load label and explanation
show whether the current settings actually congest the link. Any load or policy
change starts a fresh run, retaining running/paused status. Reset always stops.
The screen remains stable when a browser tab is hidden; it does not catch up later.

## Architecture and scope

```text
Critical ESP32 ─┐
Normal ESP32 ───┼─ Wi-Fi → Raspberry Pi AP/router → Ethernet bottleneck → wired receiver
Bulk laptop ────┘
```

The simulated queues act at the Pi's Ethernet output, **after Wi-Fi**. The receiver
is a separate wired computer. Wi-Fi is a conceptual link, not a radio model.
Gateway QoS cannot establish Wi-Fi airtime/WMM priority. QoS redistributes fixed
capacity; it never adds bandwidth. No real experiment design or agreed project
settings are changed. See the repository's architecture and experiment documents
for the real project; the following choices apply only to this prototype.

## Simulation assumptions

| Choice        | Prototype value / meaning                                                                                     |
| ------------- | ------------------------------------------------------------------------------------------------------------- |
| Clock         | Deterministic 10 ms fixed steps; approximately real-time display                                              |
| Bottleneck    | 100 abstract kB/s in **every** scenario                                                                       |
| Critical      | 10 messages/s, 1 kB each; triangle ▲                                                                          |
| Normal        | 20 messages/s, 2 kB each; circle ●                                                                            |
| Bulk          | 0–32 packets/s, 5 kB each; square ■; linear slider mapping                                                    |
| Offered load  | Baseline 50 kB/s; high-load preset 210 kB/s                                                                   |
| FIFO buffer   | One 100 kB waiting buffer; arrival order; tail drop when full                                                 |
| QoS buffers   | Reserved waiting budgets: Critical 6, Normal 24, Bulk 70 kB; total 100 kB                                     |
| QoS scheduler | Prefer Critical, then Normal, then Bulk; every 100 ms offer Bulk service first if waiting; skip empty classes |
| Service       | Non-preemptive: finish a packet before selecting another; no idle capacity credit carried forward             |
| Receiver      | A packet arrives when serialization at the bottleneck completes                                               |

The Bulk opportunity is a fixed fairness rule, not adaptive QoS. At the specified
rates it prevents starvation while leaving Critical strongly preferred. A packet
already being served can delay Critical by up to one Bulk serialization (50 ms).
QoS changes **buffer admission as well as scheduling**, so protected class buffers
cannot be occupied by Bulk. This demonstrates classified gateway treatment; it
is not an isolated scheduler benchmark. Waiting buffers exclude the one packet
in service; the same exclusion applies in both policies.

Rates, sizes, buffers, time scaling, and service rules are teaching assumptions,
not proposed `tc` commands or physical experiment parameters. Regular arrivals
and a stable class order make the demonstration repeatable without randomness.
The 1 kB critical message is an abstract model unit, not a claim about real alert
payloads. No RF/PHY, interference, Wi-Fi contention, DSCP, WMM, Linux `tc`, ACKs,
propagation, retransmissions, TCP feedback, or application processing are simulated.

## What the metrics mean

- **Critical / Normal latency:** mean generation-to-completion time of delivered
  packets in the last 3 simulated seconds, quantized by the 10 ms clock. These are
  simulated forward service delays, not measured RTT or real one-way latency.
  Dropped or still-waiting packets are not latency samples.
- **Critical delivery:** delivered / (delivered + dropped), since reset. Pending
  packets are excluded; this is a resolved-packet ratio, not a deadline guarantee.
- **Bulk throughput:** completed Bulk bytes in the last 3 seconds, divided by that
  window's duration (elapsed time during startup). Serialization of one packet can
  span the edge of the window; the full packet is counted on completion.
- **Queue occupancy:** actual waiting bytes, excluding service. Shapes are packets,
  not equal-size bytes. The queue displays its leading packets; `+N` counts the rest.

A dash means there are no delivered samples yet. The initial 3 seconds are a
startup window. All metrics are computed from engine state; there are no preset
scenario results. Display values are rounded deliberately. Determinism means the
same settings and number of simulation steps produce identical state; wall-clock
pause timing can change the last displayed frame slightly.

## Understand and validate the implementation

- `src/simulation.js`: traffic credits → admission → queues → selection → fixed
  service budget → completion events → metrics. It has no DOM or framework dependency.
- `src/main.js`: scenario/control state, frame clock, queue rendering, metric display.
- `src/style.css`: presentation layout, class symbols, focus styles and responsive rules.
- `tests/simulation.test.js`: conservation, buffer/capacity bounds, FIFO order,
  determinism, low-load similarity, Critical protection and corresponding Bulk cost.
- `browser-tests/demo.spec.js`: production-build controls, all scenarios, screen
  sizes, disclosure, mobile and offline behaviour; captures screenshots in `/tmp`.

```sh
npm test
# Browser verification, optional for presenters:
npx playwright install chromium
npm run test:ui
```

The browser test command builds first and starts a local production preview.
Chromium may need system libraries on a minimal Linux installation. Both Vite and
Playwright are development dependencies; the shipped app has no runtime dependencies.
Use `npm ci` to reproduce the committed lockfile. See `VALIDATION.md` for the checks
performed during implementation. This bounded model is suitable for explaining
trade-offs; hardware outcomes still require the real controlled experiment.
