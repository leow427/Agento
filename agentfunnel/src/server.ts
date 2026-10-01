// HTTP handlers and the Bonjour child process.
import { createServer } from "node:http";
import type { IncomingMessage, ServerResponse } from "node:http";
import { spawn } from "node:child_process";
import type { ChildProcess } from "node:child_process";
import type { AddressInfo } from "node:net";
import { fileURLToPath } from "node:url";
import { Config } from "./config.ts";
import { MAX_BODY, readBody, send } from "./http.ts";
import { Lanyard, parseReport } from "./lanyard.ts";
import type { Report } from "./lanyard.ts";
import { isPanelPath, servePanel } from "./panel.ts";
import type { PanelCtx } from "./panel.ts";
import type { Store } from "./store.ts";
import { TailscaleCheck } from "./tailscale.ts";
import type { TsStatus } from "./tailscale.ts";

export { MAX_BODY };
export const SERVICE_TYPE = "_agentfunnel._tcp";
export const DEFAULT_PORT = 41917; // 8765 is Ultraseek's, AnkiConnect's and the websockets examples'

export type ServerOptions = {
  store: Store;
  config?: Config;
  lanyard?: Lanyard;
  port: number;
  host?: string;
  bonjour?: boolean;
  log?: (msg: string) => void;
  tailscale?: { status(): Promise<TsStatus> };
  uiDir?: string;
  framesPath?: string;
};

export type Running = { port: number; close: () => Promise<void> };

type Ctx = { store: Store; config: Config; lanyard: Lanyard; log: (m: string) => void; panel: PanelCtx };

async function handle(ctx: Ctx, req: IncomingMessage, res: ServerResponse) {
  const { store, config, lanyard, log } = ctx;
  const url = new URL(req.url ?? "/", "http://x");
  if (isPanelPath(url.pathname)) return servePanel(ctx.panel, req, res, url);
  if (req.method === "POST" && url.pathname === "/event") {
    const body = await readBody(req);
    if (body === null) return send(res, 413, "too large");
    if (!config.running) return send(res, 204); // switched off on the page: the hook never notices
    let payload: unknown;
    try {
      payload = JSON.parse(body.toString("utf8"));
    } catch {
      return send(res, 400, "bad json");
    }
    if (typeof payload !== "object" || payload === null || Array.isArray(payload)) return send(res, 400, "bad json");
    const p = payload as Record<string, unknown>;
    if (store.apply(p)) log(`event ${String(p.hook_event_name)} ${String(p.session_id).slice(0, 8)}`);
    return send(res, 204);
  }
  if (req.method === "GET" && url.pathname === "/state") {
    if (url.searchParams.get("lanyard") === "1") lanyard.seen();
    const since = Number(url.searchParams.get("since") ?? 0);
    const snap = store.snapshot(Number.isFinite(since) ? since : 0);
    // Off: the lanyard shows disconnected and keeps its sessions, but settings still reach it.
    const body = config.running ? { ...snap, running: true } : { seq: snap.seq, sessions: [], messages: [], running: false };
    const out = { ...body, config: { colour: config.colour }, wifi: config.pendingAfter(lanyard.applied()) };
    return send(res, 200, JSON.stringify(out), "application/json");
  }
  if (req.method === "POST" && url.pathname === "/lanyard") {
    const body = await readBody(req);
    if (body === null) return send(res, 413, "too large");
    let r: Report | null = null;
    try {
      r = parseReport(JSON.parse(body.toString("utf8")));
    } catch {}
    if (r === null) return send(res, 400, "bad report");
    lanyard.report(r);
    config.lanyardApplied(r.applied);
    return send(res, 204);
  }
  if (req.method === "GET" && url.pathname === "/health") return send(res, 200, "ok");
  req.resume();
  send(res, 404, "not found");
}

export function startServer(opts: ServerOptions): Promise<Running> {
  const log = opts.log ?? (() => {});
  const config = opts.config ?? new Config();
  const lanyard = opts.lanyard ?? new Lanyard();
  const panel: PanelCtx = {
    store: opts.store,
    config,
    lanyard,
    tailscale: opts.tailscale ?? new TailscaleCheck(),
    port: 0, // filled in once listening
    uiDir: opts.uiDir ?? fileURLToPath(new URL("../ui/", import.meta.url)),
    framesPath: opts.framesPath ?? fileURLToPath(new URL("../../preview/data.js", import.meta.url)),
  };
  const ctx: Ctx = { store: opts.store, config, lanyard, log, panel };
  const server = createServer((req, res) => {
    handle(ctx, req, res).catch((e) => {
      log(`request failed: ${e}`);
      if (!res.headersSent) send(res, 500, "error");
    });
  });
  return new Promise((resolve, reject) => {
    server.once("error", reject);
    server.listen(opts.port, opts.host ?? "0.0.0.0", () => {
      server.off("error", reject);
      const port = (server.address() as AddressInfo).port;
      panel.port = port;
      let dnsSd: ChildProcess | null = null;
      if (opts.bonjour !== false) {
        dnsSd = spawn("dns-sd", ["-R", "Agento", SERVICE_TYPE, "local", String(port)], { stdio: "ignore" });
        dnsSd.on("error", (e) => log(`bonjour unavailable: ${e.message}`));
      }
      resolve({
        port,
        close: () =>
          new Promise<void>((done) => {
            dnsSd?.kill();
            server.closeAllConnections();
            server.close(() => done());
          }),
      });
    });
  });
}
