import { test } from "node:test";
import assert from "node:assert/strict";
import { createServer } from "node:http";
import type { Server } from "node:http";
import { createServer as createTcpServer } from "node:net";
import { run, freePort } from "./helpers.ts";

function listen(s: Server | ReturnType<typeof createTcpServer>): Promise<number> {
  return new Promise((r) => s.listen(0, "127.0.0.1", () => r((s.address() as { port: number }).port)));
}

const payload = { session_id: "abc", hook_event_name: "PreToolUse", tool_name: "Bash", tool_input: { command: "ls" } };

test("hook POSTs stdin plus sent_at to /event and exits 0", async () => {
  const got: { url?: string; method?: string; body?: Record<string, unknown> } = {};
  const srv = createServer((req, res) => {
    let b = "";
    req.on("data", (d) => (b += d));
    req.on("end", () => {
      Object.assign(got, { url: req.url, method: req.method, body: JSON.parse(b) });
      res.writeHead(204).end();
    });
  });
  const port = await listen(srv);
  const before = Date.now() / 1000;
  const r = await run("hook.ts", [], JSON.stringify(payload), { AGENTFUNNEL_URL: `http://127.0.0.1:${port}/event` });
  srv.close();
  assert.equal(r.code, 0);
  assert.equal(got.method, "POST");
  assert.equal(got.url, "/event");
  const { sent_at, ...rest } = got.body!;
  assert.deepEqual(rest, payload);
  assert.equal(typeof sent_at, "number");
  assert.ok((sent_at as number) >= before && (sent_at as number) <= Date.now() / 1000);
  assert.equal(r.stdout, "", "hook prints nothing");
});

test("hook exits 0 quickly when no server is listening", async () => {
  const port = await freePort();
  const r = await run("hook.ts", [], JSON.stringify(payload), { AGENTFUNNEL_URL: `http://127.0.0.1:${port}/event` });
  assert.equal(r.code, 0);
  assert.ok(r.ms < 1500, `took ${r.ms} ms`);
  assert.equal(r.stderr, "");
});

test("hook exits 0 within about 1 s when the server hangs", async () => {
  const sockets: import("node:net").Socket[] = [];
  const hung = createTcpServer((s) => sockets.push(s)); // accepts, never answers
  const port = await listen(hung);
  const r = await run("hook.ts", [], JSON.stringify(payload), { AGENTFUNNEL_URL: `http://127.0.0.1:${port}/event` });
  sockets.forEach((s) => s.destroy());
  hung.close();
  assert.equal(r.code, 0);
  assert.ok(r.ms >= 900 && r.ms < 1500, `took ${r.ms} ms`);
});

test("hook exits 0 on input that is not JSON", async () => {
  const port = await freePort();
  const r = await run("hook.ts", [], "not json", { AGENTFUNNEL_URL: `http://127.0.0.1:${port}/event` });
  assert.equal(r.code, 0);
  assert.equal(r.stderr, "");
});
