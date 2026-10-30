// Prototype-only model. Abstract kB units are not real project settings.
export const CLASSES = ["critical", "normal", "bulk"];
export const CAPACITY = 100; // kB/s, identical in every scenario
export const LIMIT = 100; // waiting kB; excludes the packet in service
export const STEP = 0.01;
const SIZE = { critical: 1, normal: 2, bulk: 5 };
const RESERVATION = { critical: 6, normal: 24, bulk: 70 };
export const rates = (load) => ({
  critical: 10,
  normal: 20,
  bulk: (32 * load) / 100,
});
export class Simulation {
  constructor(policy = "fifo", load = 85) {
    this.policy = policy;
    this.load = load;
    this.time = 0;
    this.tick = 0;
    this.id = 0;
    this.queues = { shared: [], critical: [], normal: [], bulk: [] };
    this.credit = { critical: 0, normal: 0, bulk: 0 };
    this.counts = Object.fromEntries(
      CLASSES.map((c) => [c, { generated: 0, delivered: 0, dropped: 0 }]),
    );
    this.active = null;
    this.lastBulk = 0;
    this.history = [];
    this.servedBytes = 0;
  }
  waiting(key) {
    return this.queues[key].reduce((sum, p) => sum + p.size, 0);
  }
  get occupancy() {
    return Object.keys(this.queues).reduce(
      (s, key) => s + this.waiting(key),
      0,
    );
  }
  get offered() {
    const r = rates(this.load);
    return CLASSES.reduce((s, c) => s + r[c] * SIZE[c], 0);
  }
  enqueue(c) {
    const p = {
      id: ++this.id,
      class: c,
      size: SIZE[c],
      remaining: SIZE[c],
      born: this.time,
    };
    this.counts[c].generated++;
    const key = this.policy === "qos" ? c : "shared";
    const limit = this.policy === "qos" ? RESERVATION[c] : LIMIT;
    if (this.waiting(key) + p.size > limit) this.counts[c].dropped++;
    else this.queues[key].push(p);
  }
  next() {
    if (this.policy !== "qos") return this.queues.shared.shift();
    // Non-preemptive priority, with a Bulk opportunity every 100 ms.
    // Empty classes are skipped; regular Bulk service prevents starvation.
    const order =
      this.time - this.lastBulk >= 0.1 - 1e-9
        ? ["bulk", "critical", "normal"]
        : CLASSES;
    for (const c of order) {
      if (this.queues[c].length) {
        if (c === "bulk") this.lastBulk = this.time;
        return this.queues[c].shift();
      }
    }
  }
  step() {
    this.time = ++this.tick * STEP;
    const r = rates(this.load);
    // Identical deterministic arrivals under both policies.
    for (const c of CLASSES) {
      this.credit[c] += r[c] * STEP;
      while (this.credit[c] >= 1 - 1e-9) {
        this.credit[c] -= 1;
        this.enqueue(c);
      }
    }
    let budget = CAPACITY * STEP;
    while (budget > 1e-9) {
      if (!this.active) this.active = this.next() || null;
      if (!this.active) break;
      const amount = Math.min(budget, this.active.remaining);
      this.active.remaining -= amount;
      this.servedBytes += amount;
      budget -= amount;
      if (this.active.remaining < 1e-9) {
        const p = this.active;
        this.counts[p.class].delivered++;
        this.history.push({
          ...p,
          at: this.time,
          latency: (this.time - p.born + STEP) * 1000,
        });
        this.active = null;
      }
    }
    this.history = this.history.filter((p) => p.at > this.time - 3);
  }
  run(seconds) {
    for (let i = 0; i < Math.round(seconds / STEP); i++) this.step();
    return this;
  }
  metrics() {
    const latency = (c) => {
      const packets = this.history.filter((p) => p.class === c);
      return packets.length
        ? packets.reduce((s, p) => s + p.latency, 0) / packets.length
        : null;
    };
    const c = this.counts.critical;
    const resolved = c.delivered + c.dropped;
    return {
      criticalLatency: latency("critical"),
      normalLatency: latency("normal"),
      criticalDelivery: resolved ? (c.delivered / resolved) * 100 : null,
      bulkThroughput:
        this.history
          .filter((p) => p.class === "bulk")
          .reduce((s, p) => s + p.size, 0) / Math.min(3, this.time || 3),
      occupancy: this.occupancy,
    };
  }
}
