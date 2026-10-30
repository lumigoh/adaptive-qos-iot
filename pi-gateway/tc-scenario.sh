#!/usr/bin/env bash
# tc-scenario.sh - inspect, apply, and roll back the Pi gateway's Ethernet
# egress treatment specified in docs/testing.md ("Gateway shaping and queues").
#
# Milestone 1 implements the shared FIFO treatment only:
#   root tbf, 5 Mbit/s, burst 32768 bytes, limit 102400 bytes.
# The classified HTB/DSCP treatment ('qos') is not implemented yet.
#
# Usage: tc-scenario.sh -i IFACE [-f] <command> [scenario]
#
# Commands:
#   show              Read-only: print tc/kernel versions and the qdisc, class
#                     and filter counters for IFACE. Safe to redirect into
#                     tc-before.txt / tc-after.txt.
#   plan <scenario>   Print the exact tc commands 'apply' would run. Changes
#                     nothing, needs no root, and does not touch IFACE.
#   apply <scenario>  Replace IFACE's root qdisc with the scenario treatment.
#                     Needs root.
#   restore           Delete IFACE's root qdisc so the kernel's default qdisc
#                     returns. Needs root.
#
# Scenarios: baseline, fifo  - shared FIFO (identical gateway treatment;
#                              they differ only in whether Bulk load runs)
#            qos             - classified HTB + DSCP (not implemented yet)
#
# Options:
#   -i IFACE  Ethernet egress interface (or set QOS_IFACE). There is
#             deliberately no default: confirm the real name on the Pi first.
#   -f        Allow apply/restore to replace or delete a root qdisc that this
#             script did not create. Its settings are NOT saved or recreated.
#
# Nothing here is persistent: a reboot also returns IFACE to the kernel default.

set -euo pipefail

# Shared FIFO settings from docs/testing.md.
RATE="5mbit"       # tc rate units are SI: 5mbit = 5,000,000 bit/s
TBF_BURST=32768    # bytes (32 KiB starting target)
TBF_LIMIT=102400   # bytes (100 KiB nominal waiting budget)
ROOT_HANDLE="1:"   # handle used for every root qdisc this script creates

PROG="${0##*/}"
IFACE="${QOS_IFACE:-}"
FORCE=0
DRY_RUN=0

usage() {
    sed -n '/^# Usage:/,/^# Nothing here/s/^# \{0,1\}//p' "$0"
}

die() {
    echo "$PROG: error: $*" >&2
    exit 1
}

# Run a tc command, or only print it when planning.
run_tc() {
    if [[ "$DRY_RUN" == 1 ]]; then
        printf 'tc'
        printf ' %q' "$@"
        printf '\n'
    else
        echo "+ tc $*"
        tc "$@"
    fi
}

check_iface_name() {
    [[ -n "$IFACE" ]] || die "no interface given; use -i IFACE or set QOS_IFACE (no default on purpose)"
    [[ "$IFACE" =~ ^[A-Za-z0-9._-]{1,15}$ ]] || die "invalid interface name: '$IFACE'"
}

require_live_iface() {
    check_iface_name
    command -v tc >/dev/null 2>&1 || die "tc not found (install iproute2)"
    ip link show dev "$IFACE" >/dev/null 2>&1 || die "interface '$IFACE' does not exist here"
}

require_root() {
    [[ "$(id -u)" == 0 ]] || die "'$1' changes the qdisc on $IFACE and must run as root (sudo)"
}

# Print "<kind> <handle>" of IFACE's root qdisc, e.g. "fq_codel 0:" or "tbf 1:".
root_qdisc() {
    tc qdisc show dev "$IFACE" | awk '$4 == "root" { print $2, $3; exit }'
}

# Refuse to touch a root qdisc that is neither the kernel default (handle 0:)
# nor one this script created (handle 1:, a kind this script uses).
check_ownership() {
    local kind handle
    read -r kind handle <<<"$(root_qdisc)"
    echo "Current root qdisc on $IFACE: ${kind:-none} ${handle:-}"
    case "$kind $handle" in
        *" 0:" | "tbf 1:" | " ") return 0 ;;
    esac
    if [[ "$FORCE" == 1 ]]; then
        echo "WARNING: -f given; replacing '$kind $handle', which this script did not create." >&2
        return 0
    fi
    die "root qdisc '$kind $handle' on $IFACE was not created by this script, and restore cannot recreate it.
  Save it first (tc -s -d qdisc show dev $IFACE), then rerun with -f if you really want to replace it."
}

# --- treatments -------------------------------------------------------------

treatment_fifo() {
    run_tc qdisc replace dev "$IFACE" root handle "$ROOT_HANDLE" \
        tbf rate "$RATE" burst "$TBF_BURST" limit "$TBF_LIMIT"
}

treatment_for() {
    case "$1" in
        baseline | fifo) echo treatment_fifo ;;
        qos) die "scenario 'qos' (HTB + DSCP) is not implemented yet" ;;
        "") die "missing scenario (baseline, fifo, or qos)" ;;
        *) die "unknown scenario '$1' (expected baseline, fifo, or qos)" ;;
    esac
}

# --- commands ---------------------------------------------------------------

cmd_show() {
    require_live_iface
    echo "# $PROG show"
    echo "# time (UTC): $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "# host: $(hostname)"
    echo "# kernel: $(uname -r)"
    echo "# tc: $(tc -V 2>&1)"
    echo "# interface: $IFACE"
    echo
    echo "## qdisc (tc -s -d qdisc show; -raw adds the tbf byte limit)"
    tc -s -d qdisc show dev "$IFACE"
    tc -raw qdisc show dev "$IFACE"
    echo
    echo "## class (tc -s class show)"
    tc -s class show dev "$IFACE"
    echo
    echo "## filter (tc -s filter show)"
    tc -s filter show dev "$IFACE"
}

cmd_plan() {
    local fn
    check_iface_name
    fn="$(treatment_for "${1:-}")"
    DRY_RUN=1
    "$fn"
}

cmd_apply() {
    local scenario="${1:-}" fn
    fn="$(treatment_for "$scenario")"
    require_live_iface
    require_root apply
    check_ownership
    echo "Applying '$scenario' treatment to $IFACE egress:"
    "$fn"
    echo
    tc -s -d qdisc show dev "$IFACE"
    echo
    echo "Roll back with: sudo $0 -i $IFACE restore   (or: sudo tc qdisc del dev $IFACE root)"
}

cmd_restore() {
    local kind handle
    require_live_iface
    require_root restore
    read -r kind handle <<<"$(root_qdisc)"
    if [[ "$handle" == "0:" || -z "$handle" ]]; then
        echo "$IFACE already uses the kernel default root qdisc (${kind:-none}); nothing to restore."
        return 0
    fi
    check_ownership
    run_tc qdisc del dev "$IFACE" root
    echo
    tc qdisc show dev "$IFACE"
}

# --- main -------------------------------------------------------------------

while getopts ":i:fh" opt; do
    case "$opt" in
        i) IFACE="$OPTARG" ;;
        f) FORCE=1 ;;
        h) usage; exit 0 ;;
        :) echo "$PROG: -$OPTARG needs an argument" >&2; exit 2 ;;
        *) echo "$PROG: unknown option -$OPTARG" >&2; usage >&2; exit 2 ;;
    esac
done
shift $((OPTIND - 1))

command="${1:-}"
[[ $# -gt 0 ]] && shift
case "$command" in
    show) cmd_show ;;
    plan) cmd_plan "$@" ;;
    apply) cmd_apply "$@" ;;
    restore) cmd_restore ;;
    "") usage >&2; exit 2 ;;
    *) echo "$PROG: unknown command '$command'" >&2; usage >&2; exit 2 ;;
esac
