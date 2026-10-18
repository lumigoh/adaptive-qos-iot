import "./style.css";
import { Simulation, CAPACITY, LIMIT, STEP, CLASSES } from "./simulation.js";
const $ = (s) => document.querySelector(s);
const symbols = { critical: "▲", normal: "●", bulk: "■" };
const descriptions = {
  baseline: [
    "Room to move.",
    "Light load leaves little queueing. Critical and Normal messages stay responsive.",
  ],
  fifo: [
    "First in. First out. Even when it matters.",
    "Critical messages wait behind Bulk traffic in the same queue. Congestion can also cause drops.",
  ],
  qos: [
    "Same capacity. Different priorities.",
    "Critical gets preferential service. Bulk absorbs most queueing and loss, with regular service to prevent starvation.",
  ],
};
let scenario = "baseline",
  load = 0,
  running = false,
  speed = 1,
  sim;
// Wall-time display cadence is independent of the engine's simulated clock.
const METRIC_INTERVAL_MS = 500;
const RECENT_HOLD_MS = 600;
let last = performance.now(),
  accumulator = 0,
  activeWallMs = 0,
  nextMetricsAt = 0;
const recent = new Map();
$("#app").innerHTML = `
<main>
<header><div><div class="eyebrow">WIRELESS IoT NETWORK / UNIVERSITY PROJECT</div><h1>Adaptive QoS<span>Fixed QoS demonstrator</span></h1></div><button id="fullscreen" class="quiet">⛶ Full screen</button></header>
<section class="intro"><h2>Same bandwidth. <span>Different priorities.</span></h2><p>Explore scheduling at the Raspberry Pi’s<br>controlled Ethernet output bottleneck.</p></section>
<nav aria-label="Simulation scenario preset"><button data-scenario="baseline">01 <b>Baseline</b><small>Default: Bulk off · FIFO</small></button><button data-scenario="fifo">02 <b>Congested — FIFO</b><small>Shared queue · load adjustable</small></button><button data-scenario="qos">03 <b>Congested — QoS</b><small>Class queues · load adjustable</small></button></nav>
<section class="lab" aria-label="Network simulation">
<div class="lab-top"><span class="eyebrow">PACKET PATH</span><div class="clock-group"><small>Simulation time · <span id="playback-label">1×</span></small><span id="clock">READY · 0.0 s</span></div><span class="capacity">Fixed capacity <b>100 kB/s</b></span></div>
<div class="network">
<div class="sources"><div class="source critical"><i>▲</i><div><b>Critical ESP32</b><small>Emergency / fault alerts</small></div></div><div class="source normal"><i>●</i><div><b>Normal ESP32</b><small>Routine telemetry</small></div></div><div class="source bulk"><i>■</i><div><b>Bulk laptop</b><small>Backup / large upload</small></div></div></div>
<div class="wireless"><svg class="wifi" width="30" height="24" viewBox="0 0 30 24" aria-hidden="true"><path d="M2 7Q15 -3 28 7M6 12Q15 5 24 12M10 17Q15 13 20 17" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round"/><circle cx="15" cy="22" r="2" fill="currentColor"/></svg><b>Wi-Fi</b><span class="arrow">→</span></div>
<div class="gateway"><div class="gateway-heading"><div><small>AP / ROUTER</small><h3>Raspberry Pi <span>QoS Gateway</span></h3></div><span id="policy" class="pill">FIFO</span></div><div id="queues"></div><div class="queue-foot"><span>← Next to serve</span><span id="backlog"></span></div></div>
<div class="output"><small>ETHERNET</small><div class="pipe"><span id="service"></span></div><b>Bottleneck →</b><span id="service-label">Idle</span></div>
<div class="receiver"><div class="receiver-icon">▤</div><b>Receiver</b><small>Separate wired computer</small><span id="received">0 delivered</span><div class="recent-service"><small>Recently served</small><div id="recent" aria-label="Recently completed packets by class"></div></div></div>
</div>
<div class="insight"><span id="insight-icon">↳</span><div><h3 id="insight-title"></h3><p id="insight-text"></p></div></div>
</section>
<section class="metrics" aria-label="Simulated metrics"><div class="metric featured"><label>▲ Critical latency</label><strong id="critical-latency">—</strong><small>Mean, delivered · last 3 sim s</small></div><div class="metric"><label>▲ Critical delivery</label><strong id="critical-delivery">—</strong><small>Delivered / (delivered + dropped)</small></div><div class="metric"><label>■ Bulk throughput</label><strong id="bulk-throughput">—</strong><small>Received · last 3 sim s</small></div><div class="metric"><label>● Normal latency</label><strong id="normal-latency">—</strong><small>Mean, delivered · last 3 sim s</small></div><div class="metric"><label>Queue occupancy</label><strong id="occupancy">0 <em>/ 100 kB</em></strong><small>Waiting data · excludes service</small></div></section>
<section class="controls"><div class="buttons"><button id="start" class="primary">Start</button><button id="pause" disabled>Ⅱ Pause</button><button id="reset">↺ Reset</button></div><div class="playback"><label for="speed">Playback</label><select id="speed"><option value="1">1× Normal</option><option value="0.1">0.1× Slow motion</option></select></div><div class="load"><label for="load">Bulk load <b id="load-value">Off</b></label><input id="load" type="range" min="0" max="100" value="0"><div><span>Off</span><span>High</span></div></div><div class="offered"><span id="offered"></span><small>Offered load / 100 kB/s capacity</small></div></section>
<footer><strong>Interactive simulation — illustrative behaviour, not measured experimental results.</strong><span>Gateway QoS only · No Wi-Fi airtime / WMM claim · No adaptive control</span></footer>
</main>`;
function packet(p) {
  return `<span class="packet ${p.class}" title="${p.class} packet #${p.id}" aria-label="${p.class} packet">${symbols[p.class]}</span>`;
}
function renderNetwork() {
  const qos = sim.policy === "qos";
  $("#clock").textContent =
    `${running ? "RUNNING" : sim.time ? "PAUSED" : "READY"} · ${sim.time.toFixed(1)} s`;
  $("#start").disabled = running;
  $("#pause").disabled = !running;
  $("#start").textContent = sim.time ? "Resume" : "Start";
  $("#policy").textContent = qos ? "FIXED QoS" : "FIFO";
  $("#queues").innerHTML = (qos ? CLASSES : ["shared"])
    .map((key) => {
      const q = sim.queues[key];
      return `<div class="queue-row"><div class="queue-label ${key}">${qos ? symbols[key] + " " + key : "Shared FIFO"}<small>${q.length} waiting</small></div><div class="queue-track">${q.slice(0, 18).map(packet).join("")}${q.length > 18 ? `<span class="overflow">+${q.length - 18}</span>` : ""}</div></div>`;
    })
    .join("");
  $("#queues").classList.toggle("single", !qos);
  $("#backlog").textContent = `${sim.occupancy} / ${LIMIT} kB`;
  $("#service").innerHTML = sim.active ? packet(sim.active) : "";
  $("#service").style.width = sim.active
    ? `${(1 - sim.active.remaining / sim.active.size) * 80 + 15}%`
    : "0";
  $("#service-label").textContent = sim.active
    ? `${symbols[sim.active.class]} ${sim.active.class} in service`
    : "Idle";
  renderRecent();
  const key = sim.offered < CAPACITY ? "baseline" : sim.policy;
  $("#insight-title").textContent = descriptions[key][0];
  $("#insight-text").textContent = descriptions[key][1];
  $("#load-value").textContent = load ? `${load}%` : "Off";
  $("#offered").textContent =
    `${Math.round(sim.offered)} kB/s ${sim.offered > CAPACITY ? "↑ congested" : "· below capacity"}`;
}
function renderMetrics() {
  const m = sim.metrics();
  $("#received").textContent =
    `${CLASSES.reduce((s, c) => s + sim.counts[c].delivered, 0)} delivered`;

  const latency = (v) =>
    v === null ? "—" : `~${Math.round(v / 10) * 10 || 10} <em>ms</em>`;
  $("#critical-latency").innerHTML = latency(m.criticalLatency);
  $("#normal-latency").innerHTML = latency(m.normalLatency);
  $("#critical-delivery").innerHTML =
    m.criticalDelivery === null
      ? "—"
      : `${Math.round(m.criticalDelivery)}<em>%</em>`;
  $("#bulk-throughput").innerHTML =
    `${Math.round(m.bulkThroughput)} <em>kB/s</em>`;
  $("#occupancy").innerHTML = `${m.occupancy} <em>/ 100 kB</em>`;
  nextMetricsAt = activeWallMs + METRIC_INTERVAL_MS;
}
function renderRecent() {
  // One bounded slot per class: never queue visual events or prolong service.
  $("#recent").innerHTML = CLASSES.map((c) => {
    const entry = recent.get(c);
    return entry && activeWallMs - entry.seenAt < RECENT_HOLD_MS
      ? packet(entry.packet)
      : `<span class="recent-empty" aria-hidden="true">·</span>`;
  }).join("");
}
function render() {
  renderNetwork();
  renderMetrics();
}
function reset() {
  accumulator = 0;
  activeWallMs = 0;
  recent.clear();
  last = performance.now();
  sim = new Simulation(scenario === "qos" ? "qos" : "fifo", load);
  render();
}
function select(next) {
  const wasBaseline = scenario === "baseline";
  scenario = next;
  if (next === "baseline") load = 0;
  else if (wasBaseline) load = 100;
  $("#load").value = load;
  document.querySelectorAll("[data-scenario]").forEach((b) => {
    b.setAttribute("aria-pressed", String(b.dataset.scenario === next));
  });
  reset();
}
$("#start").onclick = () => {
  last = performance.now();
  running = true;
  render();
};
$("#pause").onclick = () => {
  advance(performance.now());
  running = false;
  render();
};
$("#reset").onclick = () => {
  running = false;
  reset();
};
$("#speed").onchange = (e) => {
  // Account for the current frame under the old rate before changing speed.
  advance(performance.now());
  speed = Number(e.target.value);
  $("#playback-label").textContent = speed === 1 ? "1×" : "0.1× slow motion";
  renderNetwork();
};
$("#load").oninput = (e) => {
  load = Number(e.target.value);
  reset();
};
document
  .querySelectorAll("[data-scenario]")
  .forEach((b) => (b.onclick = () => select(b.dataset.scenario)));
$("#fullscreen").onclick = async () => {
  try {
    if (document.fullscreenElement) await document.exitFullscreen();
    else await document.documentElement.requestFullscreen();
  } catch {
    $("#fullscreen").textContent = "Use browser full screen";
  }
};
// Fixed steps: all traffic and link service share the same scaled clock.
// Clamp stalls and discard hidden-tab wall time instead of catching up.
function advance(now) {
  if (running && !document.hidden) {
    const wallSeconds = Math.min(Math.max((now - last) / 1000, 0), 0.1);
    activeWallMs += wallSeconds * 1000;
    accumulator += wallSeconds * speed;
    while (accumulator + 1e-9 >= STEP) {
      sim.step();
      accumulator = Math.max(0, accumulator - STEP);
      // Capture each tick, even when a packet starts and finishes between frames.
      const completed = sim.history.at(-1);
      if (completed?.at === sim.time) {
        recent.set(completed.class, {
          packet: completed,
          seenAt: activeWallMs,
        });
      }
    }
  }
  last = now;
}
function frame(now) {
  advance(now);
  if (running && !document.hidden) {
    renderNetwork();
    if (activeWallMs >= nextMetricsAt) renderMetrics();
  }
  requestAnimationFrame(frame);
}
document.addEventListener("visibilitychange", () => {
  last = performance.now();
});
select("baseline");
requestAnimationFrame(frame);
