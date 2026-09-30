import { test } from "node:test";
import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { mkdtempSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { run, freePort, SRC } from "./helpers.ts";

function startMain(port: number, statePath: string) {
  const p = spawn(process.execPath, ["--no-warnings", SRC + "main.ts", "--port", String(port), "--state", statePath, "--no-bonjour"]);
  const ready = new Promise<void>((resolve, reject) => {
    let out = "";
    p.stdout.on("data", (d) => {
      out += d;
      if (out.includes("listening")) resolve();
    });
    p.on("exit", (code) => reject(new Error(`exited ${code}`)));
  });
  return { p, ready };
}

test("main serves on --port with state in --state, and a second copy says already in use", async () => {
  const port = await freePort();
  const statePath = join(mkdtempSync(join(tmpdir(), "af-")), "state.json");
  const first = startMain(port, statePath);
  try {
    await first.ready;
    const r = await fetch(`http://127.0.0.1:${port}/health`);
    assert.equal(await r.text(), "ok");

    const second = await run("main.ts", ["--port", String(port), "--state", statePath, "--no-bonjour"]);
    assert.equal(second.code, 1);
    assert.match(second.stderr, /already in use/);

    const again = await fetch(`http://127.0.0.1:${port}/health`);
    assert.equal(again.status, 200, "first copy still serving");
  } finally {
    first.p.kill("SIGTERM");
  }
});

test("main restores state from --state on start", async () => {
  const statePath = join(mkdtempSync(join(tmpdir(), "af-")), "state.json");
  let port = await freePort();
  let m = startMain(port, statePath);
  await m.ready;
  await fetch(`http://127.0.0.1:${port}/event`, {
    method: "POST",
    body: JSON.stringify({ session_id: "s", cwd: "/p/Proj", hook_event_name: "UserPromptSubmit", prompt: "hi" }),
  });
  m.p.kill("SIGTERM");
  await new Promise((r) => m.p.on("exit", r));

  port = await freePort();
  m = startMain(port, statePath);
  try {
    await m.ready;
    const s = await (await fetch(`http://127.0.0.1:${port}/state?since=0`)).json();
    assert.equal(s.seq, 1);
    assert.equal(s.sessions[0].project, "Proj");
    assert.equal(s.messages[0].text, "> hi");
  } finally {
    m.p.kill("SIGTERM");
  }
});

test("main exits 0 on SIGTERM", async () => {
  const port = await freePort();
  const m = startMain(port, join(mkdtempSync(join(tmpdir(), "af-")), "state.json"));
  await m.ready;
  m.p.kill("SIGTERM");
  const code = await new Promise((r) => m.p.on("exit", (c) => r(c)));
  assert.equal(code, 0);
});
