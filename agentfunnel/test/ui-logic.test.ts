import { test } from "node:test";
import assert from "node:assert/strict";
// @ts-ignore: plain JS module shared with the browser
import * as L from "../ui/logic.js";
import { Store } from "../src/store.ts";
import { startServer } from "../src/server.ts";

test("COLOURS match the lanyard's slot colours", () => {
  assert.deepEqual(L.COLOURS.map((c: { body: string }) => c.body), ["#D97757", "#5B8DEF", "#6BBF59", "#A67CE0", "#E8C547", "#E0709B"]);
  assert.deepEqual(L.COLOURS.map((c: { shadow: string }) => c.shadow), ["#B05A3F", "#3F66B0", "#4A8F3C", "#7A55B0", "#B89A2E", "#B04F76"]);
});

test("colourOrder puts the chosen colour first, the rest in their usual order", () => {
  assert.deepEqual(L.colourOrder(0), [0, 1, 2, 3, 4, 5]);
  assert.deepEqual(L.colourOrder(2), [2, 0, 1, 3, 4, 5]);
  assert.deepEqual(L.colourOrder(5), [5, 0, 1, 2, 3, 4]);
});

test("sessionColours ranks the sessions that aren't ended", () => {
  const s = [{ state: "working" }, { state: "ended" }, { state: "your_turn" }];
  assert.deepEqual(L.sessionColours(s, 2), [2, null, 0]);
});

test("ago", () => {
  assert.equal(L.ago(100, 103), "just now");
  assert.equal(L.ago(100, 130), "30 s ago");
  assert.equal(L.ago(100, 100 + 125), "2 min ago");
  assert.equal(L.ago(100, 100 + 3 * 3600 + 5), "3 h ago");
  assert.equal(L.ago(100, 100 + 2 * 86400), "2 d ago");
  assert.equal(L.ago(100, 90), "just now");
});

test("stateWords", () => {
  assert.equal(L.stateWords("working"), "working");
  assert.equal(L.stateWords("your_turn"), "your turn");
  assert.equal(L.stateWords("needs_you"), "needs you");
  assert.equal(L.stateWords("ended"), "ended");
});

test("effectiveNets applies pending changes like the lanyard will", () => {
  assert.deepEqual(L.effectiveNets(["a", "b"], [{ op: "add", ssid: "c" }]), ["c", "a", "b"]);
  assert.deepEqual(L.effectiveNets(["a", "b", "c"], [{ op: "add", ssid: "d" }]), ["d", "a", "b"]);
  assert.deepEqual(L.effectiveNets(["a", "b"], [{ op: "add", ssid: "b" }]), ["b", "a"]);
  assert.deepEqual(L.effectiveNets(["a", "b"], [{ op: "delete", ssid: "a" }]), ["b"]);
});

test("addWarning only when a 4th network would drop the oldest", () => {
  assert.equal(L.addWarning(["a", "b"], [], "c"), null);
  assert.equal(L.addWarning(["a", "b", "c"], [], "a"), null, "same name just replaces the password");
  assert.match(L.addWarning(["a", "b", "c"], [], "d"), /replaces "c"/);
  assert.match(L.addWarning(["a", "b"], [{ op: "add", ssid: "x" }], "d"), /replaces "b"/);
});

test("deleteWarning for the current network and the last network", () => {
  assert.equal(L.deleteWarning(["a", "b"], [], "a", "b"), null);
  assert.match(L.deleteWarning(["a", "b"], [], "a", "a"), /disconnect/);
  assert.match(L.deleteWarning(["a"], [], "a", "a"), /first-time setup/);
  assert.match(L.deleteWarning(["a", "b"], [{ op: "delete", ssid: "b" }], "x", "a"), /first-time setup/);
});

test("tailscaleSteps marks done, current and to-do", () => {
  const states = (step: string) => L.tailscaleSteps(step).map((s: { state: string }) => s.state);
  assert.deepEqual(states("not_installed"), ["current", "todo", "todo", "todo", "todo", "todo"]);
  assert.deepEqual(states("signed_out"), ["done", "done", "current", "todo", "todo", "todo"]);
  assert.deepEqual(states("ready"), ["done", "done", "done", "done", "done", "done"]);
  const last = L.tailscaleSteps("ready", "mac.ts.net").at(-1);
  assert.match(last.title, /mac\.ts\.net/);
  assert.equal(L.tailscaleSteps("not_installed")[0].help.link.href, "https://tailscale.com/download/mac");
  assert.equal(L.tailscaleSteps("funnel_not_allowed")[4].help.link.href, "https://tailscale.com/kb/1223/funnel");
});

test("the real page files are served", async () => {
  const srv = await startServer({ store: new Store(), port: 0, host: "127.0.0.1", bonjour: false,
    tailscale: { status: async () => ({ step: "not_installed" }) } });
  try {
    const get = (p: string) => fetch(`http://127.0.0.1:${srv.port}${p}`);
    const page = await (await get("/")).text();
    for (const id of ["toggle", "seen", "session-list", "swatches", "nets", "add-net", "steps"]) assert.match(page, new RegExp(`id="${id}"`));
    for (const p of ["/ui/app.js", "/ui/logic.js", "/ui/style.css"]) assert.equal((await get(p)).status, 200, p);
    assert.match(await (await get("/frames.js")).text(), /^window\.DATA = /);
  } finally {
    await srv.close();
  }
});

test("the page is called Agento", async () => {
  const { readFileSync } = await import("node:fs");
  const html = readFileSync(new URL("../ui/index.html", import.meta.url), "utf8");
  assert.match(html, /<title>Agento<\/title>/);
  assert.match(html, /<h1>Agento<\/h1>/);
  assert.doesNotMatch(html, /AgentFunnel/);
});
