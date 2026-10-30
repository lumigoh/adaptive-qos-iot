#!/usr/bin/env python3
"""Local test double for the NUC receiver.

Listens for Q1 data messages, validates the documented format and replies with
an unpadded A1 acknowledgement. It can drop or delay ACKs so the sender's
timeout/late-ACK handling can be exercised without the NUC.

This is a test tool. It writes nothing to results/ and its output is not
experiment data.
"""

import argparse
import socket
import sys
import time

MAX_PAYLOAD = 256


def parse_q1(payload: bytes) -> tuple[str, str, int]:
    """Returns (run_id, class_name, sequence); raises ValueError if invalid."""
    if len(payload) not in (128, 256):
        raise ValueError(f"payload is {len(payload)} bytes, expected 128 or 256")

    core = payload.rstrip(b" ")
    if not core:
        raise ValueError("payload is all padding")
    if any(byte not in b" " for byte in payload[len(core):]):
        raise ValueError("padding contains a non-space byte")
    if b"\x00" in payload or b"\n" in payload or b"\r" in payload:
        raise ValueError("payload contains NUL/CR/LF")

    try:
        text = core.decode("ascii")
    except UnicodeDecodeError as exc:
        raise ValueError("payload is not ASCII") from exc

    parts = text.split("|")
    if len(parts) != 4 or parts[0] != "Q1":
        raise ValueError(f"unexpected fields: {text!r}")

    _, run_id, class_name, sequence_text = parts
    if not (1 <= len(run_id) <= 24):
        raise ValueError("bad run_id length")
    if any(c not in "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-"
           for c in run_id):
        raise ValueError("bad run_id characters")
    if class_name not in ("critical", "routine"):
        raise ValueError(f"unknown traffic class: {class_name!r}")
    if not sequence_text.isdigit() or int(sequence_text) <= 0:
        raise ValueError(f"bad sequence: {sequence_text!r}")

    expected = 128 if class_name == "critical" else 256
    if len(payload) != expected:
        raise ValueError(f"{class_name} payload is {len(payload)} bytes, expected {expected}")

    return run_id, class_name, int(sequence_text)


def main() -> int:
    parser = argparse.ArgumentParser(description="Mock Q1/A1 receiver")
    parser.add_argument("--bind", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=5001)
    parser.add_argument("--drop-every", type=int, default=0,
                        help="drop every Nth ACK (0 = never drop)")
    parser.add_argument("--delay-every", type=int, default=0,
                        help="delay every Nth ACK past the 2000 ms timeout")
    parser.add_argument("--delay-ms", type=int, default=2500)
    parser.add_argument("--idle-stop-s", type=float, default=0.0,
                        help="exit after this many idle seconds (0 = never)")
    args = parser.parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind((args.bind, args.port))
    if sys.platform == "win32":
        # A sender closing its socket makes Windows report ICMP unreachable
        # as WSAECONNRESET on the next recvfrom; disable that behaviour.
        try:
            sock.ioctl(0x9800000C, False)  # SIO_UDP_CONNRESET
        except (OSError, ValueError):
            pass
    if args.idle_stop_s > 0:
        sock.settimeout(args.idle_stop_s)

    received = 0
    valid = 0
    invalid = 0
    acked = 0
    dropped = 0
    delayed = 0
    max_sequence = 0
    sizes: dict[int, int] = {}

    print(f"mock receiver on {args.bind}:{args.port}", flush=True)

    while True:
        try:
            payload, source = sock.recvfrom(MAX_PAYLOAD * 2)
        except socket.timeout:
            break
        except ConnectionResetError:
            continue
        except KeyboardInterrupt:
            break

        received += 1
        sizes[len(payload)] = sizes.get(len(payload), 0) + 1
        try:
            run_id, class_name, sequence = parse_q1(payload)
        except ValueError as exc:
            invalid += 1
            print(f"invalid datagram from {source}: {exc}", flush=True)
            continue

        valid += 1
        max_sequence = max(max_sequence, sequence)
        if sequence % 100 == 1:
            print(f"seq {sequence} from {source} ({len(payload)} bytes)", flush=True)

        if args.drop_every and sequence % args.drop_every == 0:
            dropped += 1
            continue

        ack = f"A1|{run_id}|{class_name}|{sequence}".encode("ascii")
        delayed_now = args.delay_every and sequence % args.delay_every == 0
        if delayed_now:
            delayed += 1
            time.sleep(args.delay_ms / 1000.0)
        sock.sendto(ack, source)
        acked += 1

    print("---- summary ----", flush=True)
    print(f"received={received} valid={valid} invalid={invalid} acked={acked} "
          f"dropped={dropped} delayed={delayed}", flush=True)
    print(f"max_sequence={max_sequence}", flush=True)
    print("payload sizes: " + ", ".join(f"{k}B={v}" for k, v in sorted(sizes.items())),
          flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
