import { test } from "node:test";
import assert from "node:assert/strict";
import { existsSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { makeLogger, MAX_LOG_BYTES } from "../src/log.ts";

const fixed = () => new Date("2026-09-30T12:00:00.000Z");

test("logger appends timestamped lines to the file", () => {
  const path = join(mkdtempSync(join(tmpdir(), "af-log-")), "sub", "a.log");
  const log = makeLogger(path, MAX_LOG_BYTES, fixed);
  log("one");
  log("two");
  assert.equal(readFileSync(path, "utf8"), "2026-09-30T12:00:00.000Z one\n2026-09-30T12:00:00.000Z two\n");
});

test("logger rolls over to .1 above the limit, replacing an older .1", () => {
  const dir = mkdtempSync(join(tmpdir(), "af-log-"));
  const path = join(dir, "a.log");
  writeFileSync(path + ".1", "ancient\n");
  writeFileSync(path, "x".repeat(90) + "\n");
  const log = makeLogger(path, 100, fixed);
  log("next");
  assert.equal(readFileSync(path + ".1", "utf8"), "x".repeat(90) + "\n");
  assert.equal(readFileSync(path, "utf8"), "2026-09-30T12:00:00.000Z next\n");
  assert.ok(existsSync(path));
});

test("MAX_LOG_BYTES is 5 MB", () => assert.equal(MAX_LOG_BYTES, 5 * 1024 * 1024));
