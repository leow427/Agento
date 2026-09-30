import { test } from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, writeFileSync, readFileSync, existsSync, readdirSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { Store } from "../src/store.ts";

class Clock {
  t = 1_790_000_000;
  now = () => this.t;
}

function ev(sid: string, hook_event_name: string, extra: Record<string, unknown> = {}) {
  return { session_id: sid, cwd: `/Users/x/code/${sid}-proj`, hook_event_name, ...extra };
}

function tmpState(): string {
  return join(mkdtempSync(join(tmpdir(), "af-")), "state.json");
}

test("a new session takes the lowest free slot and its project from cwd", () => {
  const s = new Store({ now: new Clock().now });
  s.apply(ev("a", "SessionStart"));
  s.apply(ev("b", "SessionStart"));
  const snap = s.snapshot(0);
  assert.deepEqual(snap.sessions.map((x) => [x.id, x.slot, x.project, x.state]), [
    ["a", 0, "a-proj", "your_turn"],
    ["b", 1, "b-proj", "your_turn"],
  ]);
});

test("any event for an unknown session creates it", () => {
  const s = new Store({ now: new Clock().now });
  s.apply(ev("a", "PreToolUse", { tool_name: "Bash", tool_input: { command: "ls" } }));
  assert.equal(s.snapshot(0).sessions[0].state, "working");
});

test("a session created by a stateless event starts as your_turn", () => {
  const s = new Store({ now: new Clock().now });
  s.apply(ev("a", "PreCompact"));
  assert.equal(s.snapshot(0).sessions[0].state, "your_turn");
});

test("project is set from the first event that has a cwd", () => {
  const s = new Store({ now: new Clock().now });
  s.apply({ session_id: "a", hook_event_name: "SessionStart" });
  assert.equal(s.snapshot(0).sessions[0].project, "");
  s.apply({ session_id: "a", hook_event_name: "Stop", cwd: "/x/First" });
  s.apply({ session_id: "a", hook_event_name: "Stop", cwd: "/x/Second" });
  assert.equal(s.snapshot(0).sessions[0].project, "First");
});

test("events without a session_id are ignored", () => {
  const s = new Store({ now: new Clock().now });
  assert.equal(s.apply({ hook_event_name: "SessionStart" }), false);
  assert.equal(s.apply({ session_id: "", hook_event_name: "SessionStart" }), false);
  assert.equal(s.snapshot(0).sessions.length, 0);
});

test("slots are reused after a session is removed", () => {
  const c = new Clock();
  const s = new Store({ now: c.now });
  for (const id of ["a", "b", "c"]) s.apply(ev(id, "SessionStart"));
  s.apply(ev("b", "SessionEnd"));
  c.t += 301;
  s.snapshot(0);
  s.apply(ev("d", "SessionStart"));
  assert.deepEqual(s.snapshot(0).sessions.map((x) => [x.id, x.slot]), [["a", 0], ["d", 1], ["c", 2]]);
});

test("at most 6 sessions; a 7th is ignored until a slot frees", () => {
  const c = new Clock();
  const s = new Store({ now: c.now });
  for (const id of ["a", "b", "c", "d", "e", "f"]) s.apply(ev(id, "SessionStart"));
  assert.equal(s.apply(ev("g", "UserPromptSubmit", { prompt: "hi" })), false);
  let snap = s.snapshot(0);
  assert.equal(snap.sessions.length, 6);
  assert.equal(snap.messages.length, 0);
  s.apply(ev("c", "SessionEnd"));
  c.t += 300;
  s.apply(ev("g", "PreToolUse", { tool_name: "Read", tool_input: { file_path: "/a/x.ts" } }));
  snap = s.snapshot(0);
  assert.equal(snap.sessions.length, 6);
  assert.deepEqual(snap.sessions.find((x) => x.id === "g")!.slot, 2);
});

test("each session keeps its last 20 messages", () => {
  const s = new Store({ now: new Clock().now });
  for (let i = 1; i <= 25; i++) s.apply(ev("a", "UserPromptSubmit", { prompt: `p${i}` }));
  s.apply(ev("b", "UserPromptSubmit", { prompt: "other" }));
  const msgs = s.snapshot(0).messages;
  const a = msgs.filter((m) => m.sid === "a");
  assert.equal(a.length, 20);
  assert.equal(a[0].text, "> p6");
  assert.equal(a[19].text, "> p25");
  assert.equal(msgs.filter((m) => m.sid === "b").length, 1);
});

test("messages carry seq, sid, ts (sent_at) and text, oldest first", () => {
  const s = new Store({ now: new Clock().now });
  s.apply(ev("a", "UserPromptSubmit", { prompt: "one", sent_at: 100.5 }));
  s.apply(ev("b", "Notification", { message: "two", sent_at: 101 }));
  assert.deepEqual(s.snapshot(0).messages, [
    { seq: 1, sid: "a", ts: 100.5, text: "> one" },
    { seq: 2, sid: "b", ts: 101, text: "two" },
  ]);
});

test("seq only goes up, even when messages and sessions are dropped", () => {
  const c = new Clock();
  const s = new Store({ now: c.now });
  for (let i = 0; i < 30; i++) s.apply(ev("a", "UserPromptSubmit", { prompt: "x" }));
  assert.equal(s.snapshot(0).seq, 30);
  s.apply(ev("a", "SessionEnd"));
  c.t += 400;
  const snap = s.snapshot(0);
  assert.equal(snap.sessions.length, 0);
  assert.equal(snap.seq, 30);
  s.apply(ev("b", "UserPromptSubmit", { prompt: "y" }));
  assert.equal(s.snapshot(0).messages[0].seq, 31);
});

test("snapshot(since) returns only newer messages; since > seq means 0", () => {
  const s = new Store({ now: new Clock().now });
  for (const p of ["1", "2", "3"]) s.apply(ev("a", "UserPromptSubmit", { prompt: p }));
  assert.deepEqual(s.snapshot(2).messages.map((m) => m.seq), [3]);
  assert.deepEqual(s.snapshot(3).messages, []);
  assert.deepEqual(s.snapshot(99).messages.map((m) => m.seq), [1, 2, 3]);
});

test("an older sent_at adds its message but does not change the state", () => {
  const s = new Store({ now: new Clock().now });
  s.apply(ev("a", "Stop", { last_assistant_message: "done", sent_at: 200 }));
  s.apply(ev("a", "PreToolUse", { tool_name: "Bash", tool_input: { command: "late" }, sent_at: 199 }));
  const snap = s.snapshot(0);
  assert.equal(snap.sessions[0].state, "your_turn");
  assert.deepEqual(snap.messages.map((m) => m.text), ["done", "Bash: late"]);
  s.apply(ev("a", "UserPromptSubmit", { prompt: "next", sent_at: 201 }));
  assert.equal(s.snapshot(0).sessions[0].state, "working");
});

test("a late event does not revive an ended session", () => {
  const s = new Store({ now: new Clock().now });
  s.apply(ev("a", "SessionEnd", { sent_at: 300 }));
  s.apply(ev("a", "Stop", { last_assistant_message: "bye", sent_at: 299 }));
  assert.equal(s.snapshot(0).sessions[0].state, "ended");
});

test("an ended session is removed 5 minutes after it ended, with its messages", () => {
  const c = new Clock();
  const s = new Store({ now: c.now });
  s.apply(ev("a", "UserPromptSubmit", { prompt: "hi" }));
  s.apply(ev("a", "SessionEnd"));
  c.t += 299;
  assert.equal(s.snapshot(0).sessions.length, 1);
  c.t += 1;
  const snap = s.snapshot(0);
  assert.equal(snap.sessions.length, 0);
  assert.equal(snap.messages.length, 0);
});

test("a session with no events for 30 minutes is removed", () => {
  const c = new Clock();
  const s = new Store({ now: c.now });
  s.apply(ev("a", "PreToolUse", { tool_name: "Bash", tool_input: { command: "sleep" } }));
  s.apply(ev("b", "SessionStart"));
  c.t += 1000;
  s.apply(ev("b", "Stop"));
  c.t += 799;
  assert.equal(s.snapshot(0).sessions.length, 2);
  c.t += 1;
  assert.deepEqual(s.snapshot(0).sessions.map((x) => x.id), ["b"]);
});

test("updated is the time of the session's last event", () => {
  const c = new Clock();
  const s = new Store({ now: c.now });
  s.apply(ev("a", "SessionStart"));
  c.t += 5;
  s.apply(ev("a", "PreCompact"));
  assert.equal(s.snapshot(0).sessions[0].updated, c.t);
});

test("save/load round trip keeps sessions, messages and seq", () => {
  const path = tmpState();
  const c = new Clock();
  const s1 = new Store({ now: c.now, path });
  s1.apply(ev("a", "UserPromptSubmit", { prompt: "hello" }));
  s1.apply(ev("b", "Notification", { message: "perm?" }));
  assert.ok(existsSync(path), "saved after every change");
  const s2 = new Store({ now: c.now, path });
  s2.load();
  assert.deepEqual(s2.snapshot(0), s1.snapshot(0));
  s2.apply(ev("a", "Stop", { last_assistant_message: "ok" }));
  assert.equal(s2.snapshot(0).seq, 3);
});

test("save writes a temp file and renames it, leaving no temp behind", () => {
  const path = tmpState();
  const s = new Store({ now: new Clock().now, path });
  s.apply(ev("a", "SessionStart"));
  const dir = join(path, "..");
  assert.deepEqual(readdirSync(dir), ["state.json"]);
  assert.equal(JSON.parse(readFileSync(path, "utf8")).sessions.length, 1);
});

test("save creates the state directory if missing", () => {
  const path = join(mkdtempSync(join(tmpdir(), "af-")), "nested", "state.json");
  const s = new Store({ now: new Clock().now, path });
  s.apply(ev("a", "SessionStart"));
  assert.ok(existsSync(path));
});

test("loading a corrupt state file warns and starts empty", () => {
  const path = tmpState();
  writeFileSync(path, "{not json");
  const warnings: string[] = [];
  const s = new Store({ now: new Clock().now, path, log: (m) => warnings.push(m) });
  s.load();
  assert.deepEqual(s.snapshot(0), { seq: 0, sessions: [], messages: [] });
  assert.equal(warnings.length, 1);
  assert.match(warnings[0], /state/i);
});

test("loading a state file with the wrong shape warns and starts empty", () => {
  const path = tmpState();
  writeFileSync(path, JSON.stringify({ seq: "x", sessions: 5 }));
  const warnings: string[] = [];
  const s = new Store({ now: new Clock().now, path, log: (m) => warnings.push(m) });
  s.load();
  assert.deepEqual(s.snapshot(0), { seq: 0, sessions: [], messages: [] });
  assert.equal(warnings.length, 1);
});

test("a missing state file starts empty without a warning", () => {
  const warnings: string[] = [];
  const s = new Store({ now: new Clock().now, path: tmpState(), log: (m) => warnings.push(m) });
  s.load();
  assert.equal(s.snapshot(0).seq, 0);
  assert.deepEqual(warnings, []);
});

test("load runs expiry", () => {
  const path = tmpState();
  const c = new Clock();
  const s1 = new Store({ now: c.now, path });
  s1.apply(ev("a", "SessionEnd"));
  c.t += 600;
  const s2 = new Store({ now: c.now, path });
  s2.load();
  assert.equal(s2.snapshot(0).sessions.length, 0);
  assert.equal(JSON.parse(readFileSync(path, "utf8")).sessions.length, 0);
});
