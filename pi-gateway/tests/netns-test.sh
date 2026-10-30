#!/usr/bin/env bash
# netns-test.sh - exercise tc-scenario.sh inside disposable network namespaces.
#
# NEVER run this on the Raspberry Pi during an experiment, and never point it
# at a real interface: it creates its own namespaces and veth pairs and deletes
# them on exit. It needs root, iproute2, and (optionally) iperf3 + python3 for
# the throughput check.
#
# Model (isolated from the host's own interfaces and routes):
#
#   qt-cli (Wi-Fi client stand-in)   qt-gw (gateway stand-in)        qt-rx (NUC stand-in)
#   veth-cli 192.168.50.10/24 <----> veth-wl 192.168.50.1/24
#                                    veth-eth 192.168.60.1/24 <----> veth-rx 192.168.60.2/24
#                                    ip_forward=1, no NAT            route 192.168.50.0/24 via .60.1
#
# tc-scenario.sh is applied to veth-eth egress in qt-gw. This checks script
# logic and kernel tbf behaviour on this host only - it is NOT evidence about
# the Pi, its Ethernet NIC, Wi-Fi, or real DSCP markings.

set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
SCRIPT="$HERE/../tc-scenario.sh"
SUFFIX="$$"
NS_CLI="qt-cli-$SUFFIX"
NS_GW="qt-gw-$SUFFIX"
NS_RX="qt-rx-$SUFFIX"
IFACE="veth-eth"
TMP="$(mktemp -d)"
PASS=0
FAIL=0
PIDS=()
NAMESPACES=()

stop_processes() {
    local pid
    for pid in "${PIDS[@]}"; do
        kill -TERM "$pid" 2>/dev/null || true
    done
    for pid in "${PIDS[@]}"; do
        # timeout forwards TERM and escalates after its kill grace. Foreground
        # mode lets it kill and reap iperf without killing itself in the group.
        wait "$pid" 2>/dev/null || true
    done
    PIDS=()
}

cleanup() {
    trap '' HUP INT TERM
    stop_processes
    for ns in "${NAMESPACES[@]}"; do
        ip netns del "$ns" 2>/dev/null || true
    done
    rm -rf "$TMP"
}
trap cleanup EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM

ok()   { echo "PASS: $*"; PASS=$((PASS + 1)); }
bad()  { echo "FAIL: $*"; FAIL=$((FAIL + 1)); }
check() { # check "description" command...
    local desc="$1"; shift
    if "$@" >"$TMP/out" 2>&1; then ok "$desc"; else bad "$desc"; sed 's/^/    /' "$TMP/out"; fi
}
check_fails() { # check_fails "description" command...
    local desc="$1"; shift
    if "$@" >"$TMP/out" 2>&1; then bad "$desc (unexpectedly succeeded)"; sed 's/^/    /' "$TMP/out"; else ok "$desc"; fi
}
in_gw() { ip netns exec "$NS_GW" "$@"; }
gw_script() { in_gw "$SCRIPT" -i "$IFACE" "$@"; }
root_of() { in_gw tc qdisc show dev "$IFACE" | awk '$4 == "root" { print $2, $3; exit }'; }

[[ "$(id -u)" == 0 ]] || { echo "netns-test.sh must run as root (it creates namespaces)" >&2; exit 1; }
command -v tc >/dev/null || { echo "tc not found (install iproute2)" >&2; exit 1; }
command -v timeout >/dev/null || { echo "timeout not found (install coreutils)" >&2; exit 1; }

# --- 1. plan mode: pure command construction, no namespace needed ----------

expected="tc qdisc replace dev eth0 root handle 1: tbf rate 5mbit burst 32768 limit 102400"
check "plan fifo prints the documented tbf command" \
    test "$("$SCRIPT" -i eth0 plan fifo)" == "$expected"
check "plan baseline uses the same FIFO treatment" \
    test "$("$SCRIPT" -i eth0 plan baseline)" == "$expected"
check "QOS_IFACE env var selects the interface" \
    test "$(QOS_IFACE=end0 "$SCRIPT" plan fifo)" == "${expected/eth0/end0}"
check_fails "plan qos is rejected until HTB is implemented" "$SCRIPT" -i eth0 plan qos
check_fails "unknown scenario is rejected" "$SCRIPT" -i eth0 plan bogus
check_fails "missing scenario is rejected" "$SCRIPT" -i eth0 plan
check_fails "missing interface is rejected (no default)" env -u QOS_IFACE "$SCRIPT" plan fifo
check_fails "unsafe interface name is rejected" "$SCRIPT" -i 'eth0;reboot' plan fifo

# --- 2. build the disposable three-namespace topology ----------------------

for ns in "$NS_CLI" "$NS_GW" "$NS_RX"; do
    ip netns add "$ns"
    NAMESPACES+=("$ns")
done
ip -n "$NS_CLI" link add veth-cli type veth peer name veth-wl netns "$NS_GW"
ip -n "$NS_GW" link add veth-eth type veth peer name veth-rx netns "$NS_RX"
ip -n "$NS_CLI" addr add 192.168.50.10/24 dev veth-cli
ip -n "$NS_GW" addr add 192.168.50.1/24 dev veth-wl
ip -n "$NS_GW" addr add 192.168.60.1/24 dev veth-eth
ip -n "$NS_RX" addr add 192.168.60.2/24 dev veth-rx
for pair in "$NS_CLI veth-cli" "$NS_GW veth-wl" "$NS_GW veth-eth" "$NS_RX veth-rx"; do
    read -r ns dev <<<"$pair"
    ip -n "$ns" link set lo up
    ip -n "$ns" link set "$dev" up
done
ip -n "$NS_CLI" route add default via 192.168.50.1
ip -n "$NS_RX" route add 192.168.50.0/24 via 192.168.60.1
in_gw sysctl -qw net.ipv4.ip_forward=1

check "routed path client -> gw -> rx works without NAT (before shaping)" \
    ip netns exec "$NS_CLI" ping -c 2 -W 1 -q 192.168.60.2

# --- 3. show / restore on the kernel default --------------------------------

check "show works on the default qdisc" gw_script show
check "restore on the kernel default is a no-op" gw_script restore
check "default root qdisc has handle 0:" test "$(root_of | cut -d' ' -f2)" == "0:"

# --- 4. apply fifo and inspect the result -----------------------------------

check "apply fifo succeeds" gw_script apply fifo
check "root qdisc is now tbf 1:" test "$(root_of)" == "tbf 1:"
raw="$(in_gw tc -raw qdisc show dev "$IFACE")"
check "tbf rate is 5Mbit"        grep -q 'rate 5Mbit' <<<"$raw"
check "tbf burst is 32Kb"        grep -q 'burst 32Kb' <<<"$raw"
check "tbf limit is 100Kb"       grep -q 'limit 100Kb' <<<"$raw"
check_fails "re-applying fifo requires explicit force" gw_script apply fifo
check "re-applying fifo with force succeeds" gw_script -f apply fifo
check "switching to baseline with force succeeds" gw_script -f apply baseline
check "after apply baseline, exactly one qdisc line on $IFACE" \
    test "$(in_gw tc qdisc show dev "$IFACE" | grep -c .)" == 1
check "routed path still works through the shaper" \
    ip netns exec "$NS_CLI" ping -c 2 -W 1 -q 192.168.60.2

# --- 5. optional: forwarded UDP is limited to ~5 Mbit/s ----------------------

measure_udp() { # sets MEASURED in the parent shell so cleanup owns every PID
    timeout --foreground -k 2s 15s ip netns exec "$NS_RX" iperf3 -s -1 -p 5201 >"$TMP/server.log" 2>&1 &
    local srv=$!
    PIDS+=("$srv")
    sleep 0.5
    timeout --foreground -k 2s 12s ip netns exec "$NS_CLI" iperf3 -4 -c 192.168.60.2 -p 5201 -u -b 8M -l 1200 -t 5 -J \
        >"$TMP/cli.json" 2>"$TMP/client.log" &
    local client=$!
    PIDS+=("$client")
    if ! wait "$client"; then
        PIDS=("$srv")
        echo "FAIL: iperf3 client failed or timed out" >&2
        cat "$TMP/client.log" "$TMP/cli.json" >&2
        stop_processes
        return 1
    fi
    # Keep only the unreaped server registered: never signal a reaped PID.
    PIDS=("$srv")
    if ! wait "$srv"; then
        PIDS=()
        echo "FAIL: iperf3 server failed or timed out" >&2
        cat "$TMP/server.log" >&2
        return 1
    fi
    PIDS=()
    # sum_received = receiver-side (rx namespace) UDP goodput reported back to the client.
    MEASURED="$(python3 -c 'import json,sys; print("%.2f" % (json.load(open(sys.argv[1]))["end"]["sum_received"]["bits_per_second"]/1e6))' "$TMP/cli.json")"
}
in_range() { python3 -c 'import sys; v,lo,hi=map(float,sys.argv[1:]); sys.exit(0 if lo<=v<=hi else 1)' "$@"; }

if command -v iperf3 >/dev/null && command -v python3 >/dev/null; then
    measure_udp
    shaped="$MEASURED"
    echo "INFO: 8 Mbit/s offered UDP (1200 B payload), shaped receiver rate: $shaped Mbit/s"
    # 1200 B payload in a 1242 B Ethernet frame -> ~4.83 Mbit/s payload at a 5 Mbit/s cap.
    check "shaped UDP goodput is between 4.3 and 5.0 Mbit/s" in_range "$shaped" 4.3 5.0
    drops="$(in_gw tc -s qdisc show dev "$IFACE" | sed -n 's/.*dropped \([0-9]*\).*/\1/p' | head -1)"
    echo "INFO: tbf dropped packets after the overload: ${drops:-?}"
    check "tbf recorded drops under 8 Mbit/s offered load" test "${drops:-0}" -gt 0
else
    echo "SKIP: throughput check (iperf3 and python3 required)"
fi

# --- 6. ownership safety and restore ----------------------------------------

check "restore removes the shaper with explicit force" gw_script -f restore
check "root qdisc is back to handle 0:" test "$(root_of | cut -d' ' -f2)" == "0:"

in_gw tc qdisc add dev "$IFACE" root handle 5: pfifo limit 50
check_fails "apply refuses to replace a foreign root qdisc" gw_script apply fifo
check "foreign root qdisc left untouched by refused apply" test "$(root_of)" == "pfifo 5:"
check_fails "restore refuses to delete a foreign root qdisc" gw_script restore
check "apply -f replaces a foreign root qdisc" in_gw "$SCRIPT" -i "$IFACE" -f apply fifo
check "restore after -f returns to the default" gw_script -f restore
check "root qdisc is back to handle 0: at the end" test "$(root_of | cut -d' ' -f2)" == "0:"

for rate in 2mbit 5mbit; do
    in_gw tc qdisc add dev "$IFACE" root handle 1: tbf rate "$rate" burst 32768 limit 102400
    before="$(in_gw tc -raw qdisc show dev "$IFACE")"
    check_fails "apply refuses pre-existing tbf 1: at $rate" gw_script apply fifo
    check_fails "restore refuses pre-existing tbf 1: at $rate" gw_script restore
    check "refused operations preserve the entire TBF configuration" \
        test "$(in_gw tc -raw qdisc show dev "$IFACE")" == "$before"
    check "forced restore removes the conflicting TBF" gw_script -f restore
done

if command -v iperf3 >/dev/null && command -v python3 >/dev/null; then
    measure_udp
    unshaped="$MEASURED"
    echo "INFO: same flow after restore (no shaper): $unshaped Mbit/s"
    check "after restore the flow is no longer capped near 5 Mbit/s" in_range "$unshaped" 6.5 9.0
fi

# --- 7. non-root guard -------------------------------------------------------

if id nobody >/dev/null 2>&1 && command -v setpriv >/dev/null; then
    check_fails "apply refuses to run without root" \
        in_gw setpriv --reuid=nobody --regid=nogroup --clear-groups "$SCRIPT" -i "$IFACE" apply fifo
fi

echo
echo "netns-test: $PASS passed, $FAIL failed"
[[ "$FAIL" == 0 ]]
