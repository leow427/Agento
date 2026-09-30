import { test, before, after } from "node:test";
import assert from "node:assert/strict";
import { request } from "node:http";
import { Store } from "../src/store.ts";
import { startServer, MAX_BODY } from "../src/server.ts";
import type { Running } from "../src/server.ts";

let srv: Running;
let base: string;

before(async () => {
  srv = await startServer({ store: new Store(), port: 0, host: "127.0.0.1", bonjour: false });
  base = `http://127.0.0.1:${srv.port}`;
});
after(() => srv.close());

function post(body: string | Buffer) {
  return fetch(`${base}/event`, { method: "POST", body, headers: { "content-type": "application/json" } });
}
async function state(since?: string | number) {
  const r = await fetch(`${base}/state${since === undefined ? "" : `?since=${since}`}`);
  assert.equal(r.status, 200);
  assert.match(r.headers.get("content-type") ?? "", /application\/json/);
  return r.json();
}

test("POST /event returns 204 and GET /state shows the session and messages", async () => {
  let r = await post(JSON.stringify({ session_id: "s1", cwd: "/x/Agento", hook_event_name: "SessionStart", sent_at: 10 }));
  assert.equal(r.status, 204);
  r = await post(JSON.stringify({ session_id: "s1", cwd: "/x/Agento", hook_event_name: "UserPromptSubmit", prompt: "go", sent_at: 11 }));
  r = await post(JSON.stringify({ session_id: "s1", hook_event_name: "PreToolUse", tool_name: "Bash", tool_input: { command: "npm test" }, sent_at: 12 }));
  const s = await state(0);
  assert.equal(s.sessions.length, 1);
  assert.equal(s.sessions[0].project, "Agento");
  assert.equal(s.sessions[0].state, "working");
  assert.deepEqual(s.messages.map((m: { text: string }) => m.text), ["> go", "Bash: npm test"]);
  assert.equal(s.seq, 2);
});

test("GET /state with various since values", async () => {
  const all = await state(0);
  assert.equal(all.messages.length, 2);
  assert.deepEqual((await state(1)).messages.map((m: { seq: number }) => m.seq), [2]);
  assert.deepEqual((await state(2)).messages, []);
  assert.equal((await state(2)).sessions.length, 1, "sessions are always listed");
  assert.equal((await state(9999)).messages.length, 2, "since > seq is treated as 0");
  assert.equal((await state()).messages.length, 2, "missing since is 0");
  assert.equal((await state("abc")).messages.length, 2, "garbage since is 0");
});

test("bad JSON returns 400", async () => {
  assert.equal((await post("{nope")).status, 400);
  assert.equal((await post("[1,2]")).status, 400);
  assert.equal((await post("null")).status, 400);
});

test("events without a session_id are ignored with 204", async () => {
  const before = await state(0);
  assert.equal((await post(JSON.stringify({ hook_event_name: "SessionStart" }))).status, 204);
  assert.deepEqual(await state(0), before);
});

test("GET /health returns 200 ok", async () => {
  const r = await fetch(`${base}/health`);
  assert.equal(r.status, 200);
  assert.equal(await r.text(), "ok");
});

test("other paths and methods return 404", async () => {
  assert.equal((await fetch(`${base}/nope`)).status, 404);
  assert.equal((await fetch(`${base}/event`)).status, 404);
  assert.equal((await fetch(`${base}/state`, { method: "POST", body: "{}" })).status, 404);
});

test("request bodies are capped at 16 MB", async () => {
  assert.equal(MAX_BODY, 16 * 1024 * 1024);
  const r = await post(Buffer.alloc(MAX_BODY + 1, 0x20));
  assert.equal(r.status, 413);
});

test("a body of exactly 16 MB is accepted", async () => {
  const head = '{"session_id":"big","hook_event_name":"Stop","pad":"';
  const tail = '"}';
  const body = head + "x".repeat(MAX_BODY - head.length - tail.length) + tail;
  assert.equal(Buffer.byteLength(body), MAX_BODY);
  assert.equal((await post(body)).status, 204);
});

test("an oversized chunked body (no content-length) is also refused", async () => {
  const status = await new Promise<number>((resolve, reject) => {
    const req = request(`${base}/event`, { method: "POST" }, (res) => {
      res.resume();
      resolve(res.statusCode ?? 0);
    });
    req.on("error", reject);
    const chunk = Buffer.alloc(1024 * 1024, 0x20);
    for (let i = 0; i < 17; i++) req.write(chunk);
    req.end();
  });
  assert.equal(status, 413);
});
