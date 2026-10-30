import { test } from "node:test";
import assert from "node:assert/strict";
import { Simulation, CAPACITY, CLASSES } from "../src/simulation.js";

test("baseline stays responsive without drops; low load is similar under either policy", () => {
  for (const load of [0, 10]) {
    const a = new Simulation("fifo", load).run(15),
      b = new Simulation("qos", load).run(15);
    for (const s of [a, b]) {
      assert.ok(s.metrics().criticalLatency < 60);
      assert.equal(s.metrics().criticalDelivery, 100);
      assert.ok(s.occupancy < 15);
      assert.equal(
        CLASSES.reduce((n, c) => n + s.counts[c].dropped, 0),
        0,
      );
    }
    assert.ok(
      Math.abs(a.metrics().criticalLatency - b.metrics().criticalLatency) < 15,
    );
    assert.equal(a.metrics().bulkThroughput, b.metrics().bulkThroughput);
  }
});

test("congested policies share arrivals/capacity, protect Critical at a Bulk cost", () => {
  const fifo = new Simulation("fifo", 100).run(20),
    qos = new Simulation("qos", 100).run(20);
  const f = fifo.metrics(),
    q = qos.metrics();
  assert.equal(fifo.offered, qos.offered);
  for (const c of CLASSES)
    assert.equal(fifo.counts[c].generated, qos.counts[c].generated);
  assert.equal(fifo.servedBytes, qos.servedBytes);
  assert.ok(fifo.servedBytes <= CAPACITY * fifo.time);
  assert.ok(f.criticalLatency > 800);
  assert.ok(q.criticalLatency < 70);
  assert.ok(q.criticalLatency < f.criticalLatency / 10);
  assert.ok(q.criticalDelivery > f.criticalDelivery);
  assert.ok(q.bulkThroughput < f.bulkThroughput - 3);
  assert.ok(qos.counts.bulk.dropped > fifo.counts.bulk.dropped);
  assert.ok(q.bulkThroughput > 30); // no permanent starvation
  assert.ok(f.occupancy > 85);
});

test("packet conservation, bounded queues and capacity at every step over slider range", () => {
  for (const policy of ["fifo", "qos"])
    for (const load of [0, 10, 40, 85, 100]) {
      const s = new Simulation(policy, load);
      for (let i = 0; i < 1500; i++) {
        s.step();
        assert.ok(s.occupancy <= 100);
        assert.ok(s.servedBytes <= CAPACITY * s.time + 1e-7);
        for (const c of CLASSES) {
          const waiting = Object.values(s.queues)
            .flat()
            .filter((p) => p.class === c).length;
          const count = s.counts[c];
          assert.equal(
            count.generated,
            count.delivered +
              count.dropped +
              waiting +
              Number(s.active?.class === c),
          );
        }
      }
    }
});

test("FIFO departs in arrival order; Critical is visibly behind Bulk", () => {
  const s = new Simulation("fifo", 100);
  let blocked = false,
    last = 0;
  for (let i = 0; i < 800; i++) {
    s.step();
    for (const p of s.history)
      assert.ok(p.id >= last || p.at <= s.time - 0.01 + 1e-8);
    if (s.history.length) last = s.history.at(-1).id;
    const firstBulk = s.queues.shared.findIndex((p) => p.class === "bulk");
    if (
      firstBulk >= 0 &&
      s.queues.shared.slice(firstBulk + 1).some((p) => p.class === "critical")
    )
      blocked = true;
  }
  assert.ok(blocked);
  const ids = s.history.map((p) => p.id);
  assert.deepEqual(
    ids,
    [...ids].sort((a, b) => a - b),
  );
});

test("reset with identical inputs replays exactly; metrics derive from events", () => {
  for (const policy of ["fifo", "qos"]) {
    const a = new Simulation(policy, 100).run(12),
      b = new Simulation(policy, 100).run(12);
    assert.deepEqual(a, b);
    const critical = a.history.filter((p) => p.class === "critical");
    assert.equal(
      a.metrics().criticalLatency,
      critical.reduce((n, p) => n + p.latency, 0) / critical.length,
    );
    assert.equal(
      a.metrics().bulkThroughput,
      a.history
        .filter((p) => p.class === "bulk")
        .reduce((n, p) => n + p.size, 0) / 3,
    );
    const reset = new Simulation(policy, 100);
    assert.equal(reset.time, 0);
    assert.equal(reset.id, 0);
    assert.equal(reset.occupancy, 0);
    assert.equal(reset.metrics().criticalLatency, null);
  }
});
