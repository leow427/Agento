import { test, before, after } from "node:test";
import assert from "node:assert/strict";
import { request } from "node:http";
import { mkdtempSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { Store } from "../src/store.ts";
import { Config } from "../src/config.ts";
import { Lanyard } from "../src/lanyard.ts";
import { checkLocal } from "../src/panel.ts";
import { startServer } from "../src/server.ts";
import type { Running } from "../src/server.ts";

test("checkLocal: loopback only, our Host, matching Origin, JSON posts", () => {
  const ok = { remote: "127.0.0.1", host: "localhost:41917", port: 41917, method: "GET", contentType: undefined, origin: undefined };
  assert.equal(checkLocal(ok), null);
  assert.equal(checkLocal({ ...ok, remote: "::1", host: "[::1]:41917" }), null);
  assert.equal(checkLocal({ ...ok, remote: "::ffff:127.0.0.1", host: "127.0.0.1:41917" }), null);
  assert.equal(checkLocal({ ...ok, host: "LOCALHOST:41917" }), null);
  assert.match(checkLocal({ ...ok, remote: "192.168.1.20" }) ?? "", /only/);
  assert.match(checkLocal({ ...ok, host: "evil.example:41917" }) ?? "", /Host/);
  assert.match(checkLocal({ ...ok, host: "localhost:80" }) ?? "", /Host/);
  assert.match(checkLocal({ ...ok, host: undefined }) ?? "", /Host/);
  assert.match(checkLocal({ ...ok, origin: "http://evil.example" }) ?? "", /Origin/);
  assert.equal(checkLocal({ ...ok, origin: "http://localhost:41917" }), null);
  assert.match(checkLocal({ ...ok, method: "POST", contentType: "text/plain" }) ?? "", /JSON/);
  assert.equal(checkLocal({ ...ok, method: "POST", contentType: "application/json; charset=utf-8" }), null);
});

let srv: Running;
const config = new Config();
const lanyard = new Lanyard();
const store = new Store();

before(async () => {
  const dir = mkdtempSync(join(tmpdir(), "af-ui-"));
  for (const f of ["index.html", "app.js", "logic.js", "style.css"]) writeFileSync(join(dir, f), `/* ${f} */`);
  writeFileSync(join(dir, "secret"), "nope");
  const framesPath = join(dir, "data.js");
  writeFileSync(framesPath, "window.DATA = {};");
  srv = await startServer({
    store, config, lanyard, port: 0, host: "127.0.0.1", bonjour: false, uiDir: dir, framesPath,
    tailscale: { status: async () => ({ step: "ready", name: "mac.tail.ts.net" }) },
  });
});
after(() => srv.close());

function raw(method: string, path: string, headers: Record<string, string> = {}, body?: string) {
  return new Promise<{ status: number; type: string; text: string }>((resolve, reject) => {
    const req = request({ host: "127.0.0.1", port: srv.port, method, path, headers }, (res) => {
      let text = "";
      res.on("data", (d) => (text += d));
      res.on("end", () => resolve({ status: res.statusCode ?? 0, type: String(res.headers["content-type"] ?? ""), text }));
    });
    req.on("error", reject);
    req.end(body);
  });
}
const api = (path: string, body: unknown) => raw("POST", path, { "content-type": "application/json" }, JSON.stringify(body));

test("serves only the listed files", async () => {
  const page = await raw("GET", "/");
  assert.equal(page.status, 200);
  assert.match(page.type, /text\/html/);
  assert.equal(page.text, "/* index.html */");
  assert.match((await raw("GET", "/ui/app.js")).type, /javascript/);
  assert.match((await raw("GET", "/ui/logic.js")).type, /javascript/);
  assert.match((await raw("GET", "/ui/style.css")).type, /text\/css/);
  const frames = await raw("GET", "/frames.js");
  assert.equal(frames.text, "window.DATA = {};");
  for (const p of ["/ui/secret", "/ui/", "/ui/../src/main.ts", "/ui/%2e%2e/src/main.ts", "/index.html", "/ui/constructor"])
    assert.equal((await raw("GET", p)).status, 404, p);
});

test("a foreign Host or Origin, or a non-JSON post, is refused with 403", async () => {
  assert.equal((await raw("GET", "/", { host: `evil.example:${srv.port}` })).status, 403);
  assert.equal((await raw("GET", "/api/status", { host: `evil.example:${srv.port}` })).status, 403);
  assert.equal((await raw("POST", "/api/running", { "content-type": "application/json", origin: "http://evil.example" }, '{"running":false}')).status, 403);
  assert.equal((await raw("POST", "/api/running", { "content-type": "text/plain" }, '{"running":false}')).status, 403);
  assert.equal(config.running, true);
});

test("/api/status: running, colour, lanyard, sessions, pending wifi without passwords, tailscale", async () => {
  await fetch(`http://127.0.0.1:${srv.port}/event`, { method: "POST", body: JSON.stringify({ session_id: "s1", cwd: "/x/Agento", hook_event_name: "UserPromptSubmit", prompt: "hi" }) });
  config.addWifi("add", "iPhone", "hunter2hunter2");
  let s = JSON.parse((await raw("GET", "/api/status")).text);
  assert.equal(s.running, true);
  assert.equal(s.colour, 0);
  assert.equal(s.lanyard, null);
  assert.deepEqual(s.sessions, [{ project: "Agento", state: "working", slot: 0 }]);
  assert.deepEqual(s.wifiPending, [{ id: 1, op: "add", ssid: "iPhone" }]);
  assert.deepEqual(s.tailscale, { step: "ready", name: "mac.tail.ts.net" });
  assert.ok(!JSON.stringify(s).includes("hunter2"));
  lanyard.seen();
  s = JSON.parse((await raw("GET", "/api/status")).text);
  assert.equal(typeof s.lanyard.lastSeen, "number");
});

test("POST /api/running, /api/colour, /api/wifi, /api/wifi/cancel", async () => {
  assert.equal((await api("/api/running", { running: false })).status, 204);
  assert.equal(config.running, false);
  assert.equal((await api("/api/running", { running: "no" })).status, 400);
  assert.equal((await api("/api/running", { running: true })).status, 204);

  assert.equal((await api("/api/colour", { colour: 4 })).status, 204);
  assert.equal(config.colour, 4);
  for (const bad of [6, -1, "2", 1.5]) assert.equal((await api("/api/colour", { colour: bad })).status, 400);

  const short = await api("/api/wifi", { op: "add", ssid: "cafe", pass: "short" });
  assert.equal(short.status, 400);
  assert.equal(short.text, "Wi-Fi passwords are at least 8 characters");
  assert.equal((await api("/api/wifi", { op: "add", ssid: "cafe", pass: "password9" })).status, 204);
  assert.equal((await api("/api/wifi", { op: "delete", ssid: "old" })).status, 204);
  const ids = config.pendingPublic().map((o) => o.id);
  assert.equal((await api("/api/wifi/cancel", { id: ids[ids.length - 1] })).status, 204);
  assert.deepEqual(config.pendingPublic().map((o) => o.ssid), ["iPhone", "cafe"]);
  assert.equal((await api("/api/wifi/cancel", { id: "x" })).status, 400);
  assert.equal((await raw("POST", "/api/running", { "content-type": "application/json" }, "{bad")).status, 400);
  assert.equal((await api("/api/nope", {})).status, 404);
});
