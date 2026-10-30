# Experiment plan

## Question and scope

Can gateway QoS protect critical-message delivery for Wi-Fi-connected devices
under a controlled gateway bottleneck?

Fixed QoS comes first. Adaptive QoS and WMM investigation are later extensions.
There are no experiment results yet.

## Agreed comparison

| Scenario | Bulk traffic | Pi Ethernet output treatment |
|---|---|---|
| Baseline | Off | Shared FIFO |
| Congested FIFO | On | Shared FIFO |
| Congested classified QoS | On | Separate traffic classes |

Use the same aggregate bottleneck rate across scenarios. Keep traffic markings
unchanged when comparing FIFO and classified gateway treatment, so the comparison
does not also change wireless treatment.

## Proposed measurement approach — review before implementation

- **RTT:** sender records send and acknowledgement times using its own monotonic clock.
- **Delivery:** compare unique received sequence numbers with actual sent counts.
- **Acknowledgements:** record timeouts separately from forward-message loss.
- **Bulk throughput:** use receiver-side `iperf3` results.
- **Queue evidence:** record classification counters, backlog, and drops.

Do not subtract timestamps from unrelated device clocks or present RTT/2 as measured
one-way latency. Report unsuccessful probes alongside successful RTT measurements.

## Proposed repeat procedure

Start with five 60-second measured runs per scenario, after a separate warm-up.
Shuffle scenario order within repetition blocks. Finalise the procedure after pilots.

Keep positions, channel, traffic rates/sizes, bottleneck rate, queue limits, and
power-saving settings constant. Record software versions and the Git commit.

Preserve each run's raw logs and configuration, including invalid runs and reasons.
Summarise results per run before comparing repetitions.

## Decisions still needed

The ASCII UDP message and acknowledgement format (including ports, sequence numbers, and packet lengths) is specified in [architecture.md](architecture.md#udp-message-and-acknowledgement-protocol-v1). The 2000 ms ACK timeout is provisional until hardware pilots.

Still to finalise:

- CSV column names, export procedures, and cross-device run setup.
- Stable scenario names and the full run-ID naming convention (the protocol already defines allowed run-ID characters).
- Traffic rates, DSCP verification, shaping rate, and queue policy/limits.
- Warm-up, drain period, and invalid-run criteria.
- Whether to use a separately predeclared deadline for timely acknowledgements.

Check remaining settings using small hardware pilots before formal measurements.
