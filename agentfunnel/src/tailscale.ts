// Where the Mac is in Tailscale setup, for the page's checklist. Read-only: nothing is changed.
import { execFile } from "node:child_process";
import { existsSync } from "node:fs";

export type Step = "not_installed" | "not_running" | "signed_out" | "off" | "funnel_not_allowed" | "ready";
export type TsStatus = { step: Step; name?: string };
export type Runner = (cli: string, args: string[], timeoutMs: number) => Promise<{ ok: boolean; stdout: string }>;

// launchd's PATH is minimal, so look in the known places.
export const CLI_PATHS = ["/Applications/Tailscale.app/Contents/MacOS/Tailscale", "/usr/local/bin/tailscale", "/opt/homebrew/bin/tailscale"];
const CACHE_S = 10;
const TIMEOUT_MS = 3000;

export function findCli(exists: (p: string) => boolean = existsSync): string | null {
  return CLI_PATHS.find((p) => exists(p)) ?? null;
}

export function stepFromStatus(json: unknown): TsStatus {
  const s = json as { BackendState?: unknown; Self?: { DNSName?: unknown; CapMap?: unknown } } | null;
  switch (s?.BackendState) {
    case "NeedsLogin":
    case "NoState":
      return { step: "signed_out" };
    case "Stopped":
      return { step: "off" };
    case "Running":
      break;
    default:
      return { step: "not_running" };
  }
  const name = typeof s.Self?.DNSName === "string" ? s.Self.DNSName.replace(/\.$/, "") : undefined;
  const caps = s.Self?.CapMap;
  const funnel = typeof caps === "object" && caps !== null && "funnel" in caps;
  const out: TsStatus = { step: funnel ? "ready" : "funnel_not_allowed" };
  if (name !== undefined) out.name = name;
  return out;
}

// The Mac app's binary only acts as the CLI when started from a shell (it checks SHLVL); under
// launchd it would start as the app and hang. TAILSCALE_BE_CLI forces CLI mode.
const execRunner: Runner = (cli, args, timeoutMs) =>
  new Promise((resolve) => {
    const env = { ...process.env, TAILSCALE_BE_CLI: "1" };
    execFile(cli, args, { timeout: timeoutMs, env }, (err, stdout) => resolve({ ok: !err, stdout: String(stdout) }));
  });

export class TailscaleCheck {
  private readonly find: () => string | null;
  private readonly run: Runner;
  private readonly now: () => number;
  private cached: { at: number; value: Promise<TsStatus> } | null = null;

  constructor(opts: { find?: () => string | null; run?: Runner; now?: () => number } = {}) {
    this.find = opts.find ?? (() => findCli());
    this.run = opts.run ?? execRunner;
    this.now = opts.now ?? (() => Date.now() / 1000);
  }

  status(): Promise<TsStatus> {
    const now = this.now();
    if (this.cached && now - this.cached.at < CACHE_S) return this.cached.value;
    const value = this.check();
    this.cached = { at: now, value };
    return value;
  }

  private async check(): Promise<TsStatus> {
    const cli = this.find();
    if (cli === null) return { step: "not_installed" };
    const r = await this.run(cli, ["status", "--json"], TIMEOUT_MS);
    try {
      return stepFromStatus(JSON.parse(r.stdout));
    } catch {
      return { step: "not_running" };
    }
  }
}
