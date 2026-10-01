import { test } from "node:test";
import assert from "node:assert/strict";
import { interpret, clip } from "../src/events.ts";

const base = { session_id: "s1", cwd: "/x/Agento", transcript_path: "/x.jsonl" };

function pre(tool_name: string, tool_input: Record<string, unknown>) {
  return interpret({ ...base, hook_event_name: "PreToolUse", tool_name, tool_input });
}

test("SessionStart and Stop set your_turn", () => {
  assert.equal(interpret({ ...base, hook_event_name: "SessionStart", source: "startup" }).state, "your_turn");
  assert.equal(interpret({ ...base, hook_event_name: "Stop", last_assistant_message: "Done." }).state, "your_turn");
});

test("prompt and tool events set working", () => {
  assert.equal(interpret({ ...base, hook_event_name: "UserPromptSubmit", prompt: "hi" }).state, "working");
  assert.equal(pre("Bash", { command: "ls" }).state, "working");
  assert.equal(
    interpret({ ...base, hook_event_name: "PostToolUse", tool_name: "Bash", tool_input: {}, tool_response: {} }).state,
    "working",
  );
});

test("Notification sets needs_you, SessionEnd sets ended", () => {
  assert.equal(interpret({ ...base, hook_event_name: "Notification", message: "Claude needs your permission to use Bash" }).state, "needs_you");
  assert.equal(interpret({ ...base, hook_event_name: "SessionEnd", reason: "prompt_input_exit" }).state, "ended");
});

test("unknown events change nothing and say nothing", () => {
  assert.deepEqual(interpret({ ...base, hook_event_name: "PreCompact" }), { state: undefined, text: undefined });
});

test("only PreToolUse, Notification, Stop and UserPromptSubmit produce text", () => {
  assert.equal(interpret({ ...base, hook_event_name: "SessionStart" }).text, undefined);
  assert.equal(interpret({ ...base, hook_event_name: "SessionEnd" }).text, undefined);
  assert.equal(interpret({ ...base, hook_event_name: "PostToolUse", tool_name: "Bash", tool_input: { command: "ls" } }).text, undefined);
});

test("PreToolUse detail per tool", () => {
  assert.equal(pre("Bash", { command: "npm test", description: "Run tests" }).text, "Bash: npm test");
  assert.equal(pre("Edit", { file_path: "/a/b/store.ts", old_string: "x", new_string: "y" }).text, "Edit: store.ts");
  assert.equal(pre("Write", { file_path: "/a/b/README.md", content: "..." }).text, "Write: README.md");
  assert.equal(pre("Read", { file_path: "/a/b/main.cpp" }).text, "Read: main.cpp");
  assert.equal(pre("NotebookEdit", { notebook_path: "/a/nb.ipynb", new_source: "" }).text, "NotebookEdit: nb.ipynb");
  assert.equal(pre("Grep", { pattern: "TODO", path: "src" }).text, "Grep: TODO");
  assert.equal(pre("Glob", { pattern: "**/*.ts" }).text, "Glob: **/*.ts");
  assert.equal(pre("WebFetch", { url: "https://docs.waveshare.com/page?x=1", prompt: "p" }).text, "WebFetch: docs.waveshare.com");
  assert.equal(pre("WebSearch", { query: "co5300 datasheet" }).text, "WebSearch: co5300 datasheet");
  assert.equal(pre("Task", { description: "Find pins", prompt: "..." }).text, "Task: Find pins");
  assert.equal(pre("Agent", { description: "Review code", prompt: "..." }).text, "Agent: Review code");
  assert.equal(pre("TodoWrite", { todos: [] }).text, "TodoWrite");
  assert.equal(pre("mcp__github__get_me", {}).text, "mcp__github__get_me");
});

test("PreToolUse with a missing or bad detail falls back to the tool name", () => {
  assert.equal(pre("Bash", {}).text, "Bash");
  assert.equal(pre("WebFetch", { url: "not a url" }).text, "WebFetch");
  assert.equal(interpret({ ...base, hook_event_name: "PreToolUse", tool_name: "Read" }).text, "Read");
});

test("Notification, Stop and UserPromptSubmit text", () => {
  assert.equal(interpret({ ...base, hook_event_name: "Notification", message: "Claude is waiting for your input" }).text, "Claude is waiting for your input");
  assert.equal(interpret({ ...base, hook_event_name: "Stop", last_assistant_message: "All 12 tests pass." }).text, "All 12 tests pass.");
  assert.equal(interpret({ ...base, hook_event_name: "UserPromptSubmit", prompt: "fix the build" }).text, "> fix the build");
});

test("Stop without last_assistant_message has no text", () => {
  assert.equal(interpret({ ...base, hook_event_name: "Stop" }).text, undefined);
});

test("text is collapsed to one line", () => {
  assert.equal(interpret({ ...base, hook_event_name: "UserPromptSubmit", prompt: "line one\n\n  line\ttwo  " }).text, "> line one line two");
  assert.equal(pre("Bash", { command: "cd x &&\n  make" }).text, "Bash: cd x && make");
});

test("tool lines are cut to 80 characters with an ellipsis", () => {
  const t = pre("Bash", { command: "x".repeat(200) }).text!;
  assert.equal([...t].length, 80);
  assert.ok(t.endsWith("…"));
  assert.ok(t.startsWith("Bash: xxx"));
});

test("other text is cut to 200 characters with an ellipsis", () => {
  const t = interpret({ ...base, hook_event_name: "Stop", last_assistant_message: "y".repeat(500) }).text!;
  assert.equal([...t].length, 200);
  assert.ok(t.endsWith("…"));
});

test("text at exactly the limit is not cut", () => {
  assert.equal(clip("a".repeat(80), 80), "a".repeat(80));
  assert.equal(clip("a".repeat(81), 80), "a".repeat(79) + "…");
});

test("cutting never splits an emoji", () => {
  const t = clip("😀".repeat(100), 10);
  assert.equal(t, "😀".repeat(9) + "…");
});

test("event names that collide with Object.prototype are unknown", () => {
  assert.equal(interpret({ ...base, hook_event_name: "constructor" }).state, undefined);
  assert.equal(interpret({ ...base, hook_event_name: "__proto__" }).state, undefined);
});
