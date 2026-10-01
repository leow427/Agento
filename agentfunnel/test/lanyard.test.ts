import { test, before, after } from "node:test";
import assert from "node:assert/strict";
import { Store } from "../src/store.ts";
import { Config } from "../src/config.ts";
import { Lanyard, parseReport } from "../src/lanyard.ts";
import { startServer } from "../src/server.ts";
import type { Running } from "../src/server.ts";

const report = { wifi: "HomeNet", nets: ["HomeNet", "iPhone"], colour: 2,
  battery: { present: true, charging: false, percent: 81 }, applied: 0 };

test("parseReport accepts a full report and rejects bad shapes", () => {
  assert.deepEqual(parseReport(report), report);
  for (const bad of [null, [], { ...report, nets: "x" }, { ...report, nets: [1] }, { ...report, colour: 7 },
    { ...report, applied: -1 }, { ...report, battery: { present: "yes" } }, { ...report, wifi: 3 }])
    assert.equal(parseReport(bad), null, JSON.stringify(bad));
});

test("Lanyard: view is null until seen; seen and report update lastSeen", () => {
  let t = 100;
  const l = new Lanyard(() => t);
  assert.equal(l.view(), null);
  assert.equal(l.applied(), null);
  l.seen();
  assert.deepEqual(l.view(), { lastSeen: 100 });
  t = 105;
  l.report({ ...report, applied: 3 });
  assert.equal(l.applied(), 3);
  assert.deepEqual(l.view(), { lastSeen: 105, ...report, applied: 3 });
});

let srv: Running;
let base: string;
const config = new Config();
const lanyard = new Lanyard();

before(async () => {
  srv = await startServer({ store: new Store(), config, lanyard, port: 0, host: "127.0.0.1", bonjour: false });
  base = `http://127.0.0.1:${srv.port}`;
});
after(() => srv.close());

const state = async (q = "") => (await fetch(`${base}/state${q}`)).json();
const postLanyard = (body: unknown) =>
  fetch(`${base}/lanyard`, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify(body) });
const event = (body: unknown) => fetch(`${base}/event`, { method: "POST", body: JSON.stringify(body) });

test("/state carries running, config and pending wifi", async () => {
  config.setColour(3);
  config.addWifi("add", "iPhone", "password1");
  const s = await state("?since=0");
  assert.equal(s.running, true);
  assert.deepEqual(s.config, { colour: 3 });
  assert.deepEqual(s.wifi, [{ id: 1, op: "add", ssid: "iPhone", pass: "password1" }]);
});

test("lanyard=1 marks it seen; a plain /state does not", async () => {
  await state("?since=0");
  assert.equal(lanyard.view(), null);
  await state("?since=0&lanyard=1");
  assert.equal(typeof lanyard.view()?.lastSeen, "number");
});

test("POST /lanyard stores the report and drops applied ops from /state", async () => {
  assert.equal((await postLanyard({ ...report, applied: 1 })).status, 204);
  assert.equal(lanyard.view()?.wifi, "HomeNet");
  assert.deepEqual((await state("?lanyard=1")).wifi, []);
  assert.deepEqual(config.pendingPublic(), []);
});

test("POST /lanyard with a bad body is 400", async () => {
  assert.equal((await postLanyard({ nope: 1 })).status, 400);
  const r = await fetch(`${base}/lanyard`, { method: "POST", body: "{bad" });
  assert.equal(r.status, 400);
});

test("off: no sessions, but config and wifi still flow", async () => {
  await event({ session_id: "s1", cwd: "/x/Proj", hook_event_name: "SessionStart" });
  config.setRunning(false);
  config.addWifi("delete", "old");
  assert.equal((await event({ session_id: "s2", cwd: "/x/Two", hook_event_name: "SessionStart" })).status, 204);
  const off = await state("?since=0&lanyard=1");
  assert.equal(off.running, false);
  assert.deepEqual(off.sessions, []);
  assert.deepEqual(off.messages, []);
  assert.deepEqual(off.config, { colour: 3 });
  assert.deepEqual(off.wifi.map((o: { ssid: string }) => o.ssid), ["old"]);
  assert.equal((await postLanyard({ ...report, applied: 2 })).status, 204);
  config.setRunning(true);
  const on = await state("?since=0");
  assert.deepEqual(on.sessions.map((s: { project: string }) => s.project), ["Proj"], "the event sent while off was ignored");
  assert.deepEqual(on.wifi, []);
});
