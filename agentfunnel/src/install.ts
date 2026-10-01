// Installs AgentFunnel as a macOS background process (a launchd agent) that starts at login and
// shows up as "AgentFunnel" in Activity Monitor. node src/install.ts [--uninstall]
import { execFileSync } from "node:child_process";
import { mkdirSync, rmSync, symlinkSync, writeFileSync } from "node:fs";
import { homedir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { DEFAULT_PORT } from "./server.ts";

export const LABEL = "com.terminalbuddy.agentfunnel";
const MAIN_TS = fileURLToPath(new URL("./main.ts", import.meta.url));

export function nodeOk(version: string): boolean {
  const [major, minor] = version.split(".").map(Number);
  return major > 22 || (major === 22 && minor >= 18);
}

export function paths(home: string) {
  const dir = join(home, ".agentfunnel");
  return {
    dir,
    link: join(dir, "bin", "AgentFunnel"),
    plist: join(home, "Library", "LaunchAgents", `${LABEL}.plist`),
    log: join(dir, "agentfunnel.log"),
    err: join(dir, "agentfunnel.err.log"),
  };
}

const xml = (s: string) => s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");

export function plistText(o: { link: string; mainTs: string; log: string; err: string }): string {
  const args = [o.link, "--no-warnings", o.mainTs, "--log", o.log].map((a) => `    <string>${xml(a)}</string>`).join("\n");
  return `<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>Label</key>
  <string>${LABEL}</string>
  <key>ProgramArguments</key>
  <array>
${args}
  </array>
  <key>RunAtLoad</key>
  <true/>
  <key>KeepAlive</key>
  <true/>
  <key>ThrottleInterval</key>
  <integer>10</integer>
  <key>StandardErrorPath</key>
  <string>${xml(o.err)}</string>
</dict>
</plist>
`;
}

const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));

async function healthy(url: string, ms: number): Promise<boolean> {
  for (const t0 = Date.now(); Date.now() - t0 < ms; await sleep(250)) {
    try {
      if ((await fetch(url)).ok) return true;
    } catch {}
  }
  return false;
}

async function main(argv: string[]) {
  const p = paths(homedir());
  const domain = `gui/${process.getuid!()}`;
  const bootout = () => {
    try {
      execFileSync("launchctl", ["bootout", `${domain}/${LABEL}`], { stdio: "ignore" });
    } catch {} // not loaded
  };

  if (argv.includes("--uninstall")) {
    bootout();
    rmSync(p.plist, { force: true });
    rmSync(p.link, { force: true });
    console.log("AgentFunnel removed. Your settings in ~/.agentfunnel were kept.");
    return;
  }
  if (!nodeOk(process.versions.node)) {
    console.error(`AgentFunnel needs Node 22.18 or newer (this is ${process.versions.node}).`);
    process.exit(1);
  }
  mkdirSync(dirname(p.link), { recursive: true });
  rmSync(p.link, { force: true });
  symlinkSync(process.execPath, p.link); // macOS names the process after the link: "AgentFunnel"
  mkdirSync(dirname(p.plist), { recursive: true });
  writeFileSync(p.plist, plistText({ link: p.link, mainTs: MAIN_TS, log: p.log, err: p.err }));
  bootout();
  for (let attempt = 1; ; attempt++) {
    try {
      execFileSync("launchctl", ["bootstrap", domain, p.plist], { stdio: "pipe" });
      break;
    } catch (e) {
      if (attempt === 3) throw e;
      await sleep(500); // launchd can take a moment to forget the old copy
    }
  }
  if (!(await healthy(`http://127.0.0.1:${DEFAULT_PORT}/health`, 5000))) {
    console.error(`AgentFunnel didn't start. See ${p.err} and ${p.log}`);
    process.exit(1);
  }
  const url = `http://localhost:${DEFAULT_PORT}`;
  execFileSync("open", [url]);
  console.log(`AgentFunnel is running and starts at login. Its page: ${url}`);
}

if (process.argv[1] !== undefined && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  await main(process.argv.slice(2));
}
