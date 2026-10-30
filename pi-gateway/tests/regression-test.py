#!/usr/bin/env python3
"""Unprivileged safety tests: fake networking, real subprocesses and signals.

No commands reach the kernel's networking configuration. Throughput and actual
tc behaviour are covered separately by netns-test.sh in a disposable environment.
"""

import os
from pathlib import Path
import signal
import subprocess
import tempfile
import time
import unittest


HERE = Path(__file__).resolve().parent
SCRIPT = HERE.parent / "tc-scenario.sh"
NETNS = HERE / "netns-test.sh"

# One executable, symlinked under the command names the scripts invoke.
MOCK = r'''#!/usr/bin/env python3
import json, os, pathlib, signal, sys, time
p = pathlib.Path(os.environ["FAKE_DIR"])
name, args = pathlib.Path(sys.argv[0]).name, sys.argv[1:]
mode = os.environ.get("FAKE_MODE", "success")
def log(s):
    with (p / "log").open("a") as f: f.write(s + "\n")
def root():
    return (p / "root").read_text()
log(name + " " + " ".join(args))
if name == "id":
    if args == ["-u"]: print(0)
    else: sys.exit(1)  # optional nobody test is outside this mock's scope
elif name == "timeout":
    assert args[:3] == ["--foreground", "-k", "2s"] and args[3] in ("12s", "15s")
    # Keep the production bounds asserted, but run timeout regressions quickly.
    os.execv("/usr/bin/timeout", ["timeout", "--foreground", "-k", "0.2s",
             "1s" if args[3] == "12s" else "2s", *args[4:]])
elif name == "ip":
    if args[:2] == ["netns", "exec"]:
        os.execvp(args[3], args[3:])
    elif args[:2] == ["netns", "del"]:
        # Processes (including zombies) must be gone before namespace deletion.
        for record in (p / "log").read_text().splitlines():
            if record.startswith("process "):
                if pathlib.Path("/proc/" + record.split()[1]).exists():
                    log("live-on-delete " + record)
elif name == "tc":
    if "show" in args:
        if mode == "inspect-fail":
            print("synthetic inspection error", file=sys.stderr); sys.exit(2)
        if mode == "inspect-partial":
            print(root()); sys.exit(2)
        if mode == "inspect-empty": sys.exit(0)
        if mode == "inspect-multiple": print(root())
        if "qdisc" in args:
            print(root())
            if "-s" in args: print(" Sent 123 bytes 1 pkt (dropped 7, overlimits 1 requeues 0)")
    elif "replace" in args or "add" in args:
        log("mutation")
        if "pfifo" in args: value = "qdisc pfifo 5: root limit 50p"
        else:
            rate = "5Mbit" if args[args.index("rate") + 1] == "5mbit" else "2Mbit"
            value = "qdisc tbf 1: root rate " + rate + " burst 32Kb limit 100Kb"
        (p / "root").write_text(value)
    elif "del" in args:
        log("mutation"); (p / "root").write_text("qdisc noqueue 0: root")
elif name == "iperf3":
    log("process " + str(os.getpid()))
    # Record the timeout parent too, to assert it was reaped before deletion.
    log("process " + str(os.getppid()))
    server = "-s" in args
    if server and mode == "server-fail": sys.exit(3)
    if not server and mode == "client-fail": sys.exit(4)
    if mode == "interrupt" or (server and mode == "server-hang") or (not server and mode == "client-hang"):
        signal.signal(signal.SIGTERM, signal.SIG_IGN)
        while True: time.sleep(0.05)
    done = p / "done"
    if server:
        while not done.exists(): time.sleep(0.01)
        done.unlink()
    else:
        if mode == "bad-json": print("not json")
        else:
            rate = 4.85e6 if "tbf" in root() else 8e6
            print(json.dumps({"end": {"sum_received": {"bits_per_second": rate}}}))
        done.touch()
elif name not in ("ping", "sysctl"):
    raise RuntimeError("unexpected mock command: " + name)
'''


class SafetyTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="qos-regression-")
        self.addCleanup(self.tmp.cleanup)
        self.directory = Path(self.tmp.name)
        mock = self.directory / "mock"
        mock.write_text(MOCK)
        mock.chmod(0o755)
        for name in ("tc", "ip", "id", "timeout", "iperf3", "ping", "sysctl"):
            (self.directory / name).symlink_to(mock)
        self.env = dict(os.environ, PATH=f"{self.directory}:{os.environ['PATH']}",
                        FAKE_DIR=str(self.directory))
        self.env.pop("QOS_IFACE", None)
        self.set_root("qdisc noqueue 0: root")

    def set_root(self, value):
        (self.directory / "root").write_text(value)

    def log(self):
        path = self.directory / "log"
        return path.read_text() if path.exists() else ""

    def run_script(self, *args, mode="success"):
        return subprocess.run([str(SCRIPT), "-i", "fake0", *args],
                              env=dict(self.env, FAKE_MODE=mode),
                              capture_output=True, text=True, timeout=5)

    def test_plan_is_exact_and_never_inspects(self):
        for scenario in ("fifo", "baseline"):
            result = self.run_script("plan", scenario)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stdout.strip(), "tc qdisc replace dev fake0 root handle 1: tbf rate 5mbit burst 32768 limit 102400")
        self.assertEqual(self.log(), "")

    def test_conflicting_roots_are_preserved_even_with_matching_settings(self):
        for root in ("qdisc pfifo 5: root limit 50p",
                     "qdisc tbf 1: root rate 2Mbit burst 32Kb limit 100Kb",
                     "qdisc tbf 1: root rate 5Mbit burst 32Kb limit 100Kb",
                     "qdisc mq 0: root\nqdisc fq_codel 7: parent :1"):
            for args in (("apply", "fifo"), ("restore",)):
                with self.subTest(root=root, args=args):
                    self.set_root(root)
                    result = self.run_script(*args)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn("ownership", result.stderr)
                    self.assertEqual((self.directory / "root").read_text(), root)
        self.assertNotIn("mutation", self.log())

    def test_inspection_fails_closed_including_force(self):
        for mode in ("inspect-fail", "inspect-partial", "inspect-empty", "inspect-multiple"):
            for args in (("apply", "fifo"), ("restore",)):
                for force in ((), ("-f",)):
                    with self.subTest(mode=mode, args=args, force=force):
                        result = self.run_script(*force, *args, mode=mode)
                        self.assertNotEqual(result.returncode, 0)
                        self.assertIn("refusing changes", result.stderr)
        self.assertNotIn("mutation", self.log())

    def test_apply_and_explicit_rollback(self):
        self.assertEqual(self.run_script("restore").returncode, 0)
        self.assertNotIn("mutation", self.log())
        self.assertEqual(self.run_script("apply", "fifo").returncode, 0)
        self.assertNotEqual(self.run_script("restore").returncode, 0)
        self.assertEqual(self.run_script("-f", "apply", "baseline").returncode, 0)
        self.assertEqual(self.run_script("-f", "restore").returncode, 0)
        self.assertEqual((self.directory / "root").read_text(), "qdisc noqueue 0: root")

    def assert_clean(self):
        self.assertNotIn("live-on-delete", self.log(), self.log())
        self.assertEqual(self.log().count("ip netns del "), 3, self.log())

    def test_mocked_suite_success(self):
        result = subprocess.run([str(NETNS)], env=self.env, capture_output=True,
                                text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("0 failed", result.stdout)
        self.assert_clean()

    def test_failed_and_stalled_iperf_processes_are_bounded_and_cleaned(self):
        for mode in ("client-fail", "server-fail", "client-hang", "server-hang", "bad-json"):
            with self.subTest(mode=mode):
                (self.directory / "log").write_text("")
                self.set_root("qdisc noqueue 0: root")
                (self.directory / "done").unlink(missing_ok=True)
                result = subprocess.run([str(NETNS)], env=dict(self.env, FAKE_MODE=mode),
                                        capture_output=True, text=True, timeout=8)
                self.assertNotEqual(result.returncode, 0, result.stdout)
                if mode != "bad-json":
                    self.assertIn("failed or timed out", result.stderr)
                self.assert_clean()

    def test_interrupted_cleanup_terminates_and_reaps_before_namespace_deletion(self):
        for sig in (signal.SIGTERM, signal.SIGINT, signal.SIGHUP):
            with self.subTest(signal=sig):
                (self.directory / "log").write_text("")
                self.set_root("qdisc noqueue 0: root")
                proc = subprocess.Popen([str(NETNS)], env=dict(self.env, FAKE_MODE="interrupt"),
                                        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
                try:
                    deadline = time.monotonic() + 5
                    # Two iperf processes, each logging itself and its parent.
                    while self.log().count("process ") < 4:
                        if proc.poll() is not None or time.monotonic() > deadline:
                            self.fail("test did not reach the active client/server pair")
                        time.sleep(0.01)
                    proc.send_signal(sig)
                    stdout, stderr = proc.communicate(timeout=5)
                    self.assertEqual(proc.returncode, 128 + sig, stdout + stderr)
                    self.assert_clean()
                finally:
                    if proc.poll() is None:
                        proc.kill()
                        proc.communicate()


if __name__ == "__main__":
    unittest.main(verbosity=2)
