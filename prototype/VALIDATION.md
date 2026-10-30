# Prototype validation

Validated 2026-10-18 in the managed Linux workspace with Node 24.19.0,
Vite 7.3.7, Playwright 1.64.1 and Chromium 156. The lockfile records exact versions.

- Dependency installation and production build succeeded. The production bundle
  is approximately 24 kB of JS/CSS before compression, with no runtime libraries
  or external assets/API calls.
- `npm test`: **5 passing engine tests**. They check low-load similarity, bounded
  queues, packet conservation and fixed capacity at every step over five load
  settings and both policies, FIFO departure order, Critical behind Bulk,
  repeatable reset, metric event calculations, and high-load QoS trade-offs.
- `npm run test:ui`: **6 passing Chromium browser tests against the production
  build**. All three presets, Start/Pause/Resume/Reset, slider changes, policy
  switching with preserved load, queue display, Critical protection, disclosure,
  and absence of runtime errors were checked. Mobile operation and continued
  operation after the loaded browser goes offline were checked.
- All presets fit without horizontal scrolling and with the footer visible at
  1280×720, 1366×768, 1440×900, 1024×768, and 1920×1080. At 390×844 the page
  uses a scrolling mobile layout without horizontal overflow.
- Screenshots of FIFO, QoS and mobile were inspected. Packet classes use distinct
  symbols and text as well as colour; keyboard controls retain visible focus.
- The real architecture, experiment plan and decision log were left unchanged.
  App wording explicitly distinguishes illustrative simulation from measurements,
  fixed QoS from adaptive QoS, and gateway priority from Wi-Fi airtime/WMM.

## Playback and display checks

The fixed-step engine and experiment assumptions are unchanged. Controlled-browser
clock tests confirm that a 4-second run at 1× and a 40-second run at 0.1× produce
identical displayed simulation time, metric values, receiver totals, queue packet
identities/order, and backlog. Playback changes preserve the run, and speed is
retained through Reset and scenario changes. Pause freezes both queues and markers.

Metric-card tests confirm that values remain unchanged between 500 ms wall-time
updates, refresh during slow playback, flush to current state on Pause, and clear
immediately on Reset. A one-tick Critical completion appears in its bounded
“Recently served” slot even when there is no Critical packet in service; the slot
expires after its display hold and is not a queued animation. The receiver count
uses the same update cadence as the metric cards.

The FIFO/QoS presentation was recorded in Chromium at 1× and 0.1×, and extracted
frames plus slow-motion/mobile screenshots were visually inspected. The playback
control, simulation-time label, recently-served markers, and disclosure fit the
existing tested screen sizes. The display markers show completed transmissions;
they do not alter service time or imply additional capacity.

## Deterministic model check — not experimental data

For a 20-second simulated run at 100% Bulk, generated counts are identical under
both policies. The engine completes the same total service work in each policy
and remains below the 100 kB/s capacity bound. Rounded illustrative outputs:

| Simulated metric | FIFO | Fixed QoS |
| --- | ---: | ---: |
| Critical latency, delivered mean over final 3 s | ~970 ms | ~20 ms |
| Critical resolved-packet delivery, run to date | ~70% | 100% |
| Bulk completed throughput over final 3 s | ~65 kB/s | ~50 kB/s |

Bulk receives more drops under QoS while Critical and Normal receive none in
this model run. Capacity is unchanged. At 0%/10% Bulk, both policies have no
drops and similar short Critical delays. These values are reproducible teaching
outputs only; they predict neither the real project settings nor hardware results.

## Limits

This is a deterministic teaching model with synthetic traffic, reserved buffers,
fixed scheduling, and a single bottleneck. It does not model radios, Linux, TCP,
acknowledgements or hardware timing. Latency omits undelivered packets, and the
delivery ratio excludes pending packets. Early throughput uses a shorter window.
Other browsers and a physical projector were not tested. Full-screen mode uses
the standard browser API; browser/OS full screen remains a fallback. No real
experiment, deployment, adaptive policy, or hardware implementation was performed.
