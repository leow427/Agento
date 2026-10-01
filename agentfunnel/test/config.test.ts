import { test } from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, readFileSync, statSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { Config, validateBrightness, validateWifi } from "../src/config.ts";

const tmpPath = () => join(mkdtempSync(join(tmpdir(), "af-cfg-")), "config.json");

test("defaults: running, colour 0, nothing queued", () => {
  const c = new Config();
  assert.equal(c.running, true);
  assert.equal(c.colour, 0);
  assert.deepEqual(c.pendingAfter(null), []);
});

test("brightness defaults to 100% awake, 20% asleep", () => {
  assert.deepEqual(new Config().brightness, { max: 100, sleep: 20 });
});

test("setBrightness accepts 5-100 whole percents with sleep <= max", () => {
  const c = new Config();
  c.setBrightness(60, 10);
  assert.deepEqual(c.brightness, { max: 60, sleep: 10 });
  c.setBrightness(5, 5);
  for (const [m, s] of [[4, 4], [101, 20], [50, 51], [50.5, 20], [50, NaN], ["50", 20]] as const)
    assert.throws(() => c.setBrightness(m as number, s as number), RangeError);
  assert.deepEqual(c.brightness, { max: 5, sleep: 5 });
  assert.equal(validateBrightness(100, 100), null);
});

test("brightness is saved, and a file from before it existed loads with the defaults", () => {
  const path = tmpPath();
  const c = new Config({ path });
  c.setBrightness(70, 30);
  const d = new Config({ path });
  d.load();
  assert.deepEqual(d.brightness, { max: 70, sleep: 30 });
  writeFileSync(path, '{"running":false,"colour":1,"nextOpId":5,"wifiOps":[]}');
  const e = new Config({ path });
  e.load();
  assert.equal(e.running, false);
  assert.equal(e.colour, 1);
  assert.deepEqual(e.brightness, { max: 100, sleep: 20 });
  writeFileSync(path, '{"running":true,"colour":1,"brightness":{"max":10,"sleep":50},"nextOpId":5,"wifiOps":[]}');
  const f = new Config({ path });
  f.load();
  assert.deepEqual(f.brightness, { max: 100, sleep: 20 });
});

test("setColour accepts 0-5 only", () => {
  const c = new Config();
  c.setColour(5);
  assert.equal(c.colour, 5);
  for (const bad of [-1, 6, 1.5, NaN]) assert.throws(() => c.setColour(bad), RangeError);
  assert.equal(c.colour, 5);
});

test("op ids only go up, and cancel drops one", () => {
  const c = new Config();
  const a = c.addWifi("add", "home", "password1");
  const b = c.addWifi("delete", "old");
  assert.deepEqual([a.id, b.id], [1, 2]);
  assert.equal(c.cancel(1), true);
  assert.equal(c.cancel(1), false);
  assert.equal(c.addWifi("add", "x", "").id, 3);
  assert.deepEqual(c.pendingPublic(), [{ id: 2, op: "delete", ssid: "old" }, { id: 3, op: "add", ssid: "x" }]);
});

test("pendingAfter returns ops above the lanyard's applied id, oldest first, with passwords", () => {
  const c = new Config();
  c.addWifi("add", "a", "password1");
  c.addWifi("add", "b", "password2");
  assert.deepEqual(c.pendingAfter(1), [{ id: 2, op: "add", ssid: "b", pass: "password2" }]);
  assert.equal(c.pendingAfter(null).length, 2);
});

test("pendingPublic never includes passwords", () => {
  const c = new Config();
  c.addWifi("add", "a", "password1");
  assert.ok(!JSON.stringify(c.pendingPublic()).includes("password1"));
});

test("lanyardApplied drops ops it has applied", () => {
  const c = new Config();
  c.addWifi("add", "a", "password1");
  c.addWifi("add", "b", "password2");
  c.lanyardApplied(1);
  assert.deepEqual(c.pendingPublic().map((o) => o.id), [2]);
  c.lanyardApplied(2);
  assert.deepEqual(c.pendingPublic(), []);
  assert.equal(c.addWifi("add", "c", "").id, 3);
});

test("renumbers queued ops when the lanyard is ahead (config.json was lost)", () => {
  const c = new Config();
  c.addWifi("add", "a", "password1"); // id 1, but the lanyard already applied up to 40
  c.lanyardApplied(40);
  assert.deepEqual(c.pendingPublic(), [{ id: 41, op: "add", ssid: "a" }]);
  assert.equal(c.addWifi("delete", "z").id, 42);
  c.lanyardApplied(41);
  assert.deepEqual(c.pendingPublic().map((o) => o.id), [42]);
});

test("lanyardApplied ignores garbage", () => {
  const c = new Config();
  c.addWifi("add", "a", "");
  for (const bad of [-1, 1.5, NaN]) c.lanyardApplied(bad);
  assert.equal(c.pendingPublic().length, 1);
});

test("save/load round trip, file mode 0600", () => {
  const path = tmpPath();
  const c = new Config({ path });
  c.setRunning(false);
  c.setColour(2);
  c.addWifi("add", "home", "password1");
  assert.equal(statSync(path).mode & 0o777, 0o600);
  const d = new Config({ path });
  d.load();
  assert.equal(d.running, false);
  assert.equal(d.colour, 2);
  assert.deepEqual(d.pendingAfter(null), [{ id: 1, op: "add", ssid: "home", pass: "password1" }]);
  assert.equal(d.addWifi("add", "x", "").id, 2);
});

test("a corrupt or missing file starts from defaults and logs a warning for corrupt", () => {
  const path = tmpPath();
  const warnings: string[] = [];
  new Config({ path, log: (m) => warnings.push(m) }).load();
  assert.deepEqual(warnings, []);
  for (const bad of ["{nope", "[]", '{"running":"yes"}', '{"running":true,"colour":9,"nextOpId":1,"wifiOps":[]}']) {
    writeFileSync(path, bad);
    const c = new Config({ path, log: (m) => warnings.push(m) });
    c.load();
    assert.equal(c.running, true);
    assert.equal(c.colour, 0);
  }
  assert.equal(warnings.length, 4);
  assert.match(readFileSync(path, "utf8"), /colour/);
});

test("validateWifi matches the lanyard's rules, counting bytes", () => {
  assert.equal(validateWifi("add", "home", "password1"), null);
  assert.equal(validateWifi("add", "home", ""), null);
  assert.equal(validateWifi("add", "home", undefined), null);
  assert.equal(validateWifi("delete", "home", undefined), null);
  assert.equal(validateWifi("add", "", "password1"), "pick a network");
  assert.equal(validateWifi("add", "x".repeat(32), ""), null);
  assert.equal(validateWifi("add", "x".repeat(33), ""), "network name is too long");
  assert.equal(validateWifi("add", "😀".repeat(8), ""), null); // 32 bytes
  assert.equal(validateWifi("add", "😀".repeat(8) + "x", ""), "network name is too long");
  assert.equal(validateWifi("add", "home", "short"), "Wi-Fi passwords are at least 8 characters");
  assert.equal(validateWifi("add", "home", "p".repeat(64)), "password is too long");
  assert.equal(validateWifi("rename", "home", ""), "op must be add or delete");
  assert.equal(validateWifi("add", 5, ""), "pick a network");
  assert.equal(validateWifi("add", "home", 5), "bad password");
});

test("after config.json is lost, new op ids start from the clock so they stay above what the lanyard applied", () => {
  const path = tmpPath(); // no file yet: as if it was deleted
  const c = new Config({ path, now: () => 1790000000.7 });
  c.load();
  const a = c.addWifi("add", "A", "");
  const b = c.addWifi("add", "B", "");
  assert.deepEqual([a.id, b.id], [1790000000, 1790000001]);
  c.lanyardApplied(2); // the lanyard's applied id from before the loss
  assert.deepEqual(c.pendingAfter(2).map((o) => o.ssid), ["A", "B"], "nothing dropped, all still delivered");
  writeFileSync(path, "{corrupt");
  const d = new Config({ path, now: () => 1790000500, log: () => {} });
  d.load();
  assert.equal(d.addWifi("delete", "C").id, 1790000500);
});
