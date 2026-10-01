import { test } from "node:test";
import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { mkdtempSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { LABEL, nodeOk, paths, plistText } from "../src/install.ts";

test("nodeOk needs 22.18 or newer", () => {
  assert.equal(nodeOk("22.18.0"), true);
  assert.equal(nodeOk("22.23.2"), true);
  assert.equal(nodeOk("24.1.0"), true);
  assert.equal(nodeOk("22.17.9"), false);
  assert.equal(nodeOk("20.11.0"), false);
});

test("paths live in ~/.agentfunnel and ~/Library/LaunchAgents", () => {
  assert.deepEqual(paths("/Users/x"), {
    dir: "/Users/x/.agentfunnel",
    link: "/Users/x/.agentfunnel/bin/Agento",
    plist: "/Users/x/Library/LaunchAgents/com.agento.service.plist",
    log: "/Users/x/.agentfunnel/agentfunnel.log",
    err: "/Users/x/.agentfunnel/agentfunnel.err.log",
  });
});

test("plistText is a valid launchd plist that runs main.ts through the link", () => {
  const text = plistText({ link: "/U/AgentFunnel", mainTs: "/repo & co/agentfunnel/src/main.ts", log: "/U/a.log", err: "/U/a.err.log" });
  assert.match(text, new RegExp(`<string>${LABEL}</string>`));
  assert.match(text, /<string>\/U\/AgentFunnel<\/string>\s*<string>--no-warnings<\/string>\s*<string>\/repo &amp; co\/agentfunnel\/src\/main.ts<\/string>\s*<string>--log<\/string>\s*<string>\/U\/a.log<\/string>/);
  assert.match(text, /<key>RunAtLoad<\/key>\s*<true\/>/);
  assert.match(text, /<key>KeepAlive<\/key>\s*<true\/>/);
  assert.match(text, /<key>ThrottleInterval<\/key>\s*<integer>10<\/integer>/);
  assert.match(text, /<key>StandardErrorPath<\/key>\s*<string>\/U\/a.err.log<\/string>/);
  const f = join(mkdtempSync(join(tmpdir(), "af-pl-")), "x.plist");
  writeFileSync(f, text);
  assert.match(execFileSync("plutil", ["-lint", f]).toString(), /OK/);
});

test("the service is called Agento", () => assert.equal(LABEL, "com.agento.service"));
