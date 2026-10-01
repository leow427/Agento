import { test } from "node:test";
import assert from "node:assert/strict";
import { CLI_PATHS, findCli, stepFromStatus, TailscaleCheck } from "../src/tailscale.ts";
import type { Runner } from "../src/tailscale.ts";

const running = (capMap: Record<string, unknown> = { funnel: null, https: null }) =>
  ({ BackendState: "Running", Self: { DNSName: "leos-macbook-pro.tail1d3d28.ts.net.", CapMap: capMap } });

test("CLI is looked for in the app bundle, /usr/local/bin and /opt/homebrew/bin", () => {
  assert.deepEqual(CLI_PATHS, ["/Applications/Tailscale.app/Contents/MacOS/Tailscale", "/usr/local/bin/tailscale", "/opt/homebrew/bin/tailscale"]);
  assert.equal(findCli(() => false), null);
  assert.equal(findCli((p) => p === "/opt/homebrew/bin/tailscale"), "/opt/homebrew/bin/tailscale");
  assert.equal(findCli(() => true), CLI_PATHS[0]);
});

test("stepFromStatus maps each backend state", () => {
  assert.deepEqual(stepFromStatus(running()), { step: "ready", name: "leos-macbook-pro.tail1d3d28.ts.net" });
  assert.deepEqual(stepFromStatus(running({ https: null })), { step: "funnel_not_allowed", name: "leos-macbook-pro.tail1d3d28.ts.net" });
  assert.deepEqual(stepFromStatus({ BackendState: "Running", Self: {} }), { step: "funnel_not_allowed" });
  assert.deepEqual(stepFromStatus({ BackendState: "NeedsLogin" }), { step: "signed_out" });
  assert.deepEqual(stepFromStatus({ BackendState: "NoState" }), { step: "signed_out" });
  assert.deepEqual(stepFromStatus({ BackendState: "Stopped" }), { step: "off" });
  assert.deepEqual(stepFromStatus({ BackendState: "Starting" }), { step: "not_running" });
  assert.deepEqual(stepFromStatus(null), { step: "not_running" });
});

test("status: not installed, failing, garbage, and ready", async () => {
  const ok = (stdout: string): Runner => async () => ({ ok: true, stdout });
  assert.deepEqual(await new TailscaleCheck({ find: () => null }).status(), { step: "not_installed" });
  const failing: Runner = async () => ({ ok: false, stdout: "" });
  assert.deepEqual(await new TailscaleCheck({ find: () => "/x", run: failing }).status(), { step: "not_running" });
  assert.deepEqual(await new TailscaleCheck({ find: () => "/x", run: ok("not json") }).status(), { step: "not_running" });
  assert.equal((await new TailscaleCheck({ find: () => "/x", run: ok(JSON.stringify(running())) }).status()).step, "ready");
});

test("status uses `status --json` with a 3 s timeout and caches for 10 s", async () => {
  let t = 0;
  const calls: string[] = [];
  const run: Runner = async (cli, args, timeoutMs) => {
    calls.push(`${cli} ${args.join(" ")} ${timeoutMs}`);
    return { ok: true, stdout: JSON.stringify(running()) };
  };
  const c = new TailscaleCheck({ find: () => "/ts", run, now: () => t });
  await c.status();
  t = 9.9;
  await c.status();
  assert.deepEqual(calls, ["/ts status --json 3000"]);
  t = 10;
  await c.status();
  assert.equal(calls.length, 2);
});

test("the real runner times out instead of hanging", async () => {
  const c = new TailscaleCheck({ find: () => "/bin/sleep" });
  const t0 = Date.now();
  // `sleep status --json` fails at once on macOS; either way the answer is not_running, quickly.
  assert.deepEqual(await c.status(), { step: "not_running" });
  assert.ok(Date.now() - t0 < 3500);
});
