# Pi gateway — `tc` scenario script

Scripts for the Raspberry Pi 5 QoS gateway. The specification lives in [`docs/testing.md`](../docs/testing.md#gateway-shaping-and-queues) and [`docs/architecture.md`](../docs/architecture.md); this folder only implements it.

**Status (milestone 1):** shared FIFO treatment only. Current validation and historical namespace results are distinguished below. **Nothing here has been run on the Pi.** The classified HTB/DSCP treatment (`qos`) and all AP/routing configuration are not implemented yet.

| File | Purpose |
|---|---|
| `tc-scenario.sh` | Inspect (`show`), dry-run (`plan`), apply (`apply`) and roll back (`restore`) the Ethernet egress treatment |
| `tests/netns-test.sh` | Disposable namespace test of the script — **never run on the Pi** |
| `tests/regression-test.py` | Unprivileged safety and subprocess cleanup regressions |

## What `fifo` / `baseline` apply

Both scenarios use the same gateway treatment (they differ only in whether the MacBook Bulk load runs):

```sh
tc qdisc replace dev <IFACE> root handle 1: tbf rate 5mbit burst 32768 limit 102400
```

| Setting | Value | Source |
|---|---|---|
| Rate | `5mbit` = 5,000,000 bit/s (tc rates are SI) | testing.md: 5 Mbit/s aggregate cap |
| Burst | 32768 bytes (32 KiB) | testing.md: `tbf` burst starting target |
| Limit | 102400 bytes (100 KiB) | testing.md: nominal total waiting budget |
| Handle | `1:` | identifies the root; does not establish ownership |

At a full queue, 100 KiB − 32 KiB burst at 5 Mbit/s adds roughly 111 ms of waiting (`tc` reports this as `lat 111ms`). `tc` prints the byte limit only with `-raw`, so `show` includes that line.

## Usage

The interface has **no default** — pass `-i` or set `QOS_IFACE`. Confirm the real Ethernet name on the Pi first (expected `eth0`, not yet verified).

```sh
./tc-scenario.sh -i eth0 plan fifo          # print commands only; changes nothing, no root needed
./tc-scenario.sh -i eth0 show               # read-only counters (redirect to tc-before.txt / tc-after.txt)
sudo ./tc-scenario.sh -i eth0 apply fifo    # apply the shared FIFO shaper
sudo ./tc-scenario.sh -i eth0 -f restore    # explicitly remove it; the kernel default returns
```

Safety behaviour:

- `plan` never runs `tc`; it can be used on any machine (including the MacBook) to review commands.
- `apply`/`restore` require root, refuse an unknown or missing interface, and print the root qdisc before and after.
- Every existing non-default qdisc requires `-f` for replacement or deletion, including `tbf 1:` installed by a previous invocation. Handles and matching settings do not prove ownership. No ownership state is stored. Re-applying FIFO or switching to Baseline therefore also requires `-f`. A default root with nonzero child handles is protected too.
- Failed, empty, or ambiguous root inspections abort before mutation, even with `-f`.
- `-f` authorizes discarding existing qdisc settings; save and review them first (`tc -s -d qdisc show dev <IFACE>`). The script cannot recreate them. `restore` removes the treatment; it does not restore an earlier custom configuration.
- `qos` currently exits with "not implemented yet" rather than applying anything.

## Rollback procedure

1. Inspect `tc -s -d qdisc show dev eth0`, confirm it is the treatment you intend to remove, then run `sudo ./tc-scenario.sh -i eth0 -f restore`.
2. If the script is unavailable: `sudo tc qdisc del dev eth0 root`
3. Confirm: `tc qdisc show dev eth0` shows a root qdisc with handle `0:` (the kernel default, e.g. `pfifo_fast`, `fq_codel` or `mq`).
4. Nothing is persistent: the script installs no boot-time configuration, so **a reboot also restores the default**.

**Access risk before the first hardware apply:** the shaper limits *all* egress on that interface to 5 Mbit/s with up to ~111 ms extra queueing. It does not block traffic, so SSH should continue to work, but SSH replies leaving through the same interface will be slowed under Bulk load. Applying it to the wrong interface (for example the Wi-Fi AP interface, or an uplink used for SSH) would shape the wrong traffic — inspect it and run `-f restore` on that interface. If SSH is lost, use a local keyboard/monitor or power-cycle the Pi (step 4).

## What has actually been verified

The original milestone reported the following checks in its development container (Ubuntu 24.04, kernel 6.18, iproute2 6.1), **not on the Pi**. These are historical results, not verification of the current fixes:

- ShellCheck 0.9.0 and `bash -n` pass for both scripts.
- `plan fifo`/`plan baseline` print exactly the command above; `plan qos`, unknown/missing scenarios, a missing interface and unsafe interface names are rejected.
- In a three-namespace model (client `192.168.50.10` → forwarding gateway → receiver `192.168.60.2`, no NAT, receiver return route via `192.168.60.1`), with the shaper on the gateway's "Ethernet" veth egress:
  - `apply fifo` creates `tbf 1:` with rate 5Mbit, burst 32Kb, limit 100Kb; re-apply and `apply baseline` succeeded under the original ownership policy. Current operations on an existing TBF require `-f`.
  - Forwarded traffic still routes through the shaper.
  - An 8 Mbit/s, 1200-byte UDP `iperf3` flow measured **4.87–4.88 Mbit/s** receiver goodput (3 runs) with `tbf` drops recorded; after `restore` the same flow reached **8.0 Mbit/s**.
  - `apply`/`restore` refuse a foreign root qdisc (and leave it untouched) unless `-f` is given; `restore` on the default is a no-op; `apply` refuses to run as a non-root user.
- The host's real interface was not modified (its qdisc was identical before and after).

The targeted unprivileged regressions use fake `ip`/`tc` commands and real subprocesses to check conflicting TBFs, inspection errors, client/server failures and timeouts, malformed JSON, and HUP/INT/TERM cleanup ordering. They do not verify kernel networking:

```sh
python3 pi-gateway/tests/regression-test.py
bash -n pi-gateway/tc-scenario.sh pi-gateway/tests/netns-test.sh
shellcheck pi-gateway/tc-scenario.sh pi-gateway/tests/netns-test.sh
```

During the targeted-fix review, all seven regression tests and `bash -n` passed. ShellCheck was unavailable, and the disposable namespace suite could not run: the environment lacked root/sudo and denied user-namespace creation. No new throughput or hardware validation is claimed.

Run the namespace test on a disposable Linux machine or VM (needs root, iproute2, coreutils `timeout`, ping; iperf3 + python3 optional). Each iperf client/server has a wall-clock bound (12/15 seconds plus a 2-second kill grace); failures abort the suite. Exit and HUP/INT/TERM cleanup terminates/reaps tracked processes before deleting namespaces. SIGKILL cannot be trapped:

```sh
sudo pi-gateway/tests/netns-test.sh
```

Veth pairs have no real NIC, driver, offload behaviour or Wi-Fi. These results do **not** establish the bottleneck, routing, DSCP markings or classification on the Pi.

## Not yet confirmed on the Pi

No AP or network-manager configuration has been chosen, because none of the following is known yet. Record the answers (e.g. in this README) during the first hardware session, using read-only commands:

| Unknown | Read-only check on the Pi |
|---|---|
| Pi OS release and version | `cat /etc/os-release; uname -a` |
| Ethernet and Wi-Fi interface names (expected `eth0`, `wlan0`) | `ip -br link; ip -br addr` |
| Which service manages networking (NetworkManager, systemd-networkd, dhcpcd, ifupdown, …) | `systemctl is-active NetworkManager systemd-networkd dhcpcd networking; nmcli dev status` |
| Whether hostapd/dnsmasq or another AP/DHCP stack is installed or running | `systemctl is-active hostapd dnsmasq; systemctl list-units --type=service | grep -Ei 'hostapd|dnsmasq|dhcp'` |
| Wi-Fi AP mode support on the chosen stack | `iw list | sed -n '/Supported interface modes/,/^\s*$/p'` |
| How SSH reaches the Pi (Wi-Fi, Ethernet, or another link) — decides what reconfiguration could cut access | `echo "$SSH_CONNECTION"; ip route get <your-laptop-IP>` |
| IPv4 forwarding state and how it is persisted | `sysctl net.ipv4.ip_forward; ls /etc/sysctl.d/` |
| Existing firewall/NAT rules | `sudo nft list ruleset; sudo iptables -S; sudo iptables -t nat -S` |
| iproute2 version and kernel support for `tbf`, `htb`, `bfifo`, `flower` | `tc -V; modinfo sch_tbf sch_htb cls_flower | grep -E '^(filename|name)'` |
| Default root qdisc on the Ethernet interface | `tc qdisc show dev <eth>; sysctl net.core.default_qdisc` |
| NIC offloads and link speed (can affect shaping accuracy and counters) | `ethtool -k <eth> | grep -E 'segmentation|generic-receive'; ethtool <eth> | grep -E 'Speed|Duplex'` |
| NUC address, return route to `192.168.50.0/24`, and firewall | on the NUC: `ip -br addr; ip route; sudo ufw status` |

Hardware order (from the kickoff): verify OS/interface names, AP, IPv4 forwarding, NUC return route and end-to-end connectivity **first**; only then apply the 5 Mbit/s shaper and, once implemented, check DSCP class counters against packet captures.

## Notes for the next milestone (HTB/DSCP)

- `tc qdisc replace` cannot change a root `tbf 1:` into `htb 1:` in place (`Error: Invalid qdisc name: must match existing qdisc`). Scenario switching between `fifo` and `qos` will need delete-then-add, which briefly leaves the kernel default qdisc in place.
- `show` will need `tc -s filter show dev <eth> parent 1:` to display HTB filter counters.
