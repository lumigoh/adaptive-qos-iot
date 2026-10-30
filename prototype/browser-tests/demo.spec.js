import { test, expect } from "@playwright/test";
test("presentation controls, policies, load and reset work without runtime errors", async ({
  page,
}) => {
  const errors = [];
  page.on("pageerror", (e) => errors.push(e.message));
  await page.goto("/");
  await expect(page.locator("footer")).toContainText(
    "not measured experimental results",
  );
  await expect(page.locator("#load")).toHaveValue("0");
  await expect(page.locator("#policy")).toHaveText("FIFO");
  await page.locator("#start").click();
  await expect(page.locator("#critical-latency")).not.toHaveText("—");
  await page.locator("#pause").click();
  const paused = await page.locator("#clock").textContent();
  await page.waitForTimeout(200);
  expect(await page.locator("#clock").textContent()).toBe(paused);
  await page.locator("#reset").click();
  await expect(page.locator("#clock")).toHaveText("READY · 0.0 s");
  await expect(page.locator("#critical-latency")).toHaveText("—");
  await page.locator("[data-scenario=fifo]").click();
  await page.locator("#start").click();
  await page.waitForTimeout(6500);
  await page.locator("#pause").click();
  const fifoLatency = Number(
    (await page.locator("#critical-latency").innerText()).replace(/\D/g, ""),
  );
  expect(fifoLatency).toBeGreaterThan(700);
  expect(await page.locator(".queue-track .critical").count()).toBeGreaterThan(
    0,
  );
  await page.screenshot({ path: "/tmp/qos-fifo.png", fullPage: true });
  const fifoLoad = await page.locator("#load").inputValue();
  await page.locator("[data-scenario=qos]").click();
  await expect(page.locator("#load")).toHaveValue(fifoLoad);
  await expect(page.locator(".queue-row")).toHaveCount(3);
  await page.locator("#start").click();
  await page.waitForTimeout(6500);
  await page.locator("#pause").click();
  const qosLatency = Number(
    (await page.locator("#critical-latency").innerText()).replace(/\D/g, ""),
  );
  expect(qosLatency).toBeLessThan(80);
  await expect(page.locator("#critical-delivery")).toHaveText("100%");
  await page.screenshot({ path: "/tmp/qos-priority.png", fullPage: true });
  await page.locator("#load").fill("10");
  await expect(page.locator("#clock")).toHaveText("READY · 0.0 s");
  await expect(page.locator("#load-value")).toHaveText("10%");
  await expect(page.locator("#offered")).toContainText("below capacity");
  await page.locator("[data-scenario=baseline]").click();
  await expect(page.locator("#load")).toHaveValue("0");
  expect(errors).toEqual([]);
});
test("all presets fit laptop and presentation screens", async ({ page }) => {
  for (const size of [
    { width: 1366, height: 768 },
    { width: 1280, height: 720 },
    { width: 1440, height: 900 },
    { width: 1024, height: 768 },
    { width: 1920, height: 1080 },
  ]) {
    await page.setViewportSize(size);
    await page.goto("/");
    for (const scenario of ["baseline", "fifo", "qos"]) {
      await page.locator(`[data-scenario=${scenario}]`).click();
      const bounds = await page.evaluate(() => ({
        width: document.documentElement.scrollWidth,
        height: document.documentElement.scrollHeight,
        footer: document.querySelector("footer").getBoundingClientRect().bottom,
      }));
      expect(bounds.width).toBeLessThanOrEqual(size.width);
      expect(bounds.footer).toBeLessThanOrEqual(size.height);
    }
  }
});
test("mobile stays usable and the loaded app continues offline", async ({
  page,
  context,
}) => {
  await page.setViewportSize({ width: 390, height: 844 });
  await page.goto("/");
  expect(
    await page.evaluate(() => document.documentElement.scrollWidth),
  ).toBeLessThanOrEqual(390);
  await page.locator("[data-scenario=qos]").click();
  await expect(page.locator("#reset")).toBeVisible();
  await context.setOffline(true);
  await page.locator("#start").click();
  await expect(page.locator("#critical-latency")).not.toHaveText("—");
  await page.locator("#pause").click();
  await page.screenshot({ path: "/tmp/qos-mobile.png", fullPage: true });
  await page.locator("#reset").click();
  await expect(page.locator("#clock")).toHaveText("READY · 0.0 s");
});

async function frozenPage(page) {
  const time = new Date("2026-10-18T00:00:00Z");
  await page.clock.install({ time });
  await page.clock.pauseAt(time);
  await page.goto("/");
}
const clockSeconds = async (page) =>
  Number((await page.locator("#clock").textContent()).match(/([\d.]+) s/)[1]);
const metricSnapshot = (page) =>
  page.locator(".metrics strong").allTextContents();
const stateSnapshot = async (page) => ({
  time: await clockSeconds(page),
  metrics: await metricSnapshot(page),
  received: await page.locator("#received").textContent(),
  queue: await page
    .locator("#queues .packet")
    .evaluateAll((es) => es.map((e) => e.title)),
  backlog: await page.locator("#backlog").textContent(),
});

test("slow playback preserves state at equal simulated time and survives controls", async ({
  page,
}) => {
  await frozenPage(page);
  await page.locator("[data-scenario=fifo]").click();
  await page.locator("#start").click();
  await page.clock.runFor(4000);
  await page.locator("#pause").click();
  const normal = await stateSnapshot(page);
  expect(normal.time).toBeCloseTo(4, 1);
  await page.locator("#reset").click();
  await page.locator("#speed").selectOption("0.1");
  await page.locator("#start").click();
  await page.clock.runFor(40000);
  await page.locator("#pause").click();
  expect(await stateSnapshot(page)).toEqual(normal);
  await page.clock.runFor(1000);
  expect(await stateSnapshot(page)).toEqual(normal);
  await page.locator("#reset").click();
  await expect(page.locator("#speed")).toHaveValue("0.1");
  await expect(page.locator("#recent .packet")).toHaveCount(0);
  await page.locator("[data-scenario=qos]").click();
  await expect(page.locator("#speed")).toHaveValue("0.1");
  await page.locator("#start").click();
  await page.clock.runFor(1000);
  expect(await clockSeconds(page)).toBeCloseTo(0.1, 1);
  await page.locator("#speed").selectOption("1");
  await page.clock.runFor(1000);
  await page.locator("#pause").click();
  expect(await clockSeconds(page)).toBeCloseTo(1.1, 1);
});

test("metric cards refresh at wall-time cadence and Pause flushes the latest state", async ({
  page,
}) => {
  await frozenPage(page);
  await page.locator("[data-scenario=fifo]").click();
  await page.locator("#start").click();
  const initial = await metricSnapshot(page);
  await page.clock.runFor(400);
  expect(await metricSnapshot(page)).toEqual(initial);
  await page.clock.runFor(150);
  const first = await metricSnapshot(page);
  expect(first).not.toEqual(initial);
  await page.clock.runFor(300);
  expect(await metricSnapshot(page)).toEqual(first);
  await page.locator("#pause").click();
  expect(await metricSnapshot(page)).not.toEqual(first);
  const paused = await metricSnapshot(page);
  await page.clock.runFor(1000);
  expect(await metricSnapshot(page)).toEqual(paused);
  await page.locator("#reset").click();
  await expect(page.locator("#critical-latency")).toHaveText("—");
  await page.locator("#speed").selectOption("0.1");
  await page.locator("#start").click();
  await page.clock.runFor(2500);
  await expect(page.locator("#critical-latency")).not.toHaveText("—");
});

test("one-tick Critical completions remain visible separately from current service", async ({
  page,
}) => {
  await frozenPage(page);
  await page.locator("#speed").selectOption("0.1");
  await page.locator("#start").click();
  await page.clock.runFor(1100);
  await expect(page.locator("#recent .critical")).toHaveCount(1);
  await expect(page.locator("#service .critical")).toHaveCount(0);
  const title = await page.locator("#recent .critical").getAttribute("title");
  await page.clock.runFor(300);
  await expect(page.locator("#recent .critical")).toHaveAttribute(
    "title",
    title,
  );
  await page.locator("#pause").click();
  const recent = await page.locator("#recent").innerHTML();
  await page.clock.runFor(2000);
  expect(await page.locator("#recent").innerHTML()).toBe(recent);
  await page.screenshot({ path: "/tmp/qos-slow-motion.png", fullPage: true });
  await page.locator("#start").click();
  await page.clock.runFor(400);
  await expect(page.locator("#recent .critical")).toHaveCount(0);
  await expect(page.locator("#recent > *")).toHaveCount(3);
});
