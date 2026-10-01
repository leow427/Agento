// The control page and its API. This Mac only: refused for other machines, other Host names
// (DNS rebinding) and other websites' requests (Origin), so nothing else can change settings.
import { readFile } from "node:fs/promises";
import { join } from "node:path";
import type { IncomingMessage, ServerResponse } from "node:http";
import type { Config } from "./config.ts";
import { validateWifi } from "./config.ts";
import { readBody, send } from "./http.ts";
import type { Lanyard } from "./lanyard.ts";
import type { Store } from "./store.ts";
import type { TsStatus } from "./tailscale.ts";

export type LocalCheck = {
  remote: string | undefined;
  host: string | undefined;
  port: number;
  method: string;
  contentType: string | undefined;
  origin: string | undefined;
};

export type PanelCtx = {
  store: Store;
  config: Config;
  lanyard: Lanyard;
  tailscale: { status(): Promise<TsStatus> };
  port: number;
  uiDir: string;
  framesPath: string;
};

const LOOPBACK = new Set(["127.0.0.1", "::1", "::ffff:127.0.0.1"]);
const API_MAX = 64 * 1024;

// Request path -> [file in uiDir, content type]. Nothing outside this list is ever read.
const FILES = new Map<string, [string, string]>([
  ["/", ["index.html", "text/html; charset=utf-8"]],
  ["/ui/app.js", ["app.js", "text/javascript; charset=utf-8"]],
  ["/ui/logic.js", ["logic.js", "text/javascript; charset=utf-8"]],
  ["/ui/style.css", ["style.css", "text/css; charset=utf-8"]],
]);

export function checkLocal(c: LocalCheck): string | null {
  if (!LOOPBACK.has(c.remote ?? "")) return "this page only opens on the Mac running AgentFunnel";
  const host = (c.host ?? "").toLowerCase();
  if (![`localhost:${c.port}`, `127.0.0.1:${c.port}`, `[::1]:${c.port}`].includes(host)) return "unexpected Host";
  if (c.origin !== undefined && c.origin.toLowerCase() !== `http://${host}`) return "unexpected Origin";
  if (c.method === "POST" && !(c.contentType ?? "").toLowerCase().startsWith("application/json")) return "expected JSON";
  return null;
}

export function isPanelPath(path: string): boolean {
  return FILES.has(path) || path === "/frames.js" || path.startsWith("/api/");
}

async function readJson(req: IncomingMessage): Promise<Record<string, unknown> | null> {
  const body = await readBody(req, API_MAX);
  if (body === null) return null;
  try {
    const v = JSON.parse(body.toString("utf8"));
    return typeof v === "object" && v !== null && !Array.isArray(v) ? v : null;
  } catch {
    return null;
  }
}

async function status(ctx: PanelCtx) {
  return {
    running: ctx.config.running,
    colour: ctx.config.colour,
    lanyard: ctx.lanyard.view(),
    sessions: ctx.store.snapshot(0).sessions.map(({ project, state, slot }) => ({ project, state, slot })),
    wifiPending: ctx.config.pendingPublic(),
    tailscale: await ctx.tailscale.status(),
  };
}

export async function servePanel(ctx: PanelCtx, req: IncomingMessage, res: ServerResponse, url: URL): Promise<void> {
  const why = checkLocal({
    remote: req.socket.remoteAddress,
    host: req.headers.host,
    port: ctx.port,
    method: req.method ?? "GET",
    contentType: req.headers["content-type"],
    origin: req.headers.origin,
  });
  if (why !== null) {
    req.resume();
    return send(res, 403, why);
  }
  const path = url.pathname;
  if (req.method === "GET") {
    const file = FILES.get(path);
    if (file !== undefined) return send(res, 200, await readFile(join(ctx.uiDir, file[0]), "utf8"), file[1]);
    if (path === "/frames.js") return send(res, 200, await readFile(ctx.framesPath, "utf8"), "text/javascript; charset=utf-8");
    if (path === "/api/status") return send(res, 200, JSON.stringify(await status(ctx)), "application/json");
  }
  if (req.method === "POST" && path.startsWith("/api/")) {
    const b = await readJson(req);
    if (b === null) return send(res, 400, "bad json");
    switch (path) {
      case "/api/running":
        if (typeof b.running !== "boolean") return send(res, 400, "running must be true or false");
        ctx.config.setRunning(b.running);
        return send(res, 204);
      case "/api/colour":
        if (!Number.isInteger(b.colour) || (b.colour as number) < 0 || (b.colour as number) > 5) return send(res, 400, "colour must be 0-5");
        ctx.config.setColour(b.colour as number);
        return send(res, 204);
      case "/api/wifi": {
        const err = validateWifi(b.op, b.ssid, b.pass);
        if (err !== null) return send(res, 400, err);
        ctx.config.addWifi(b.op as "add" | "delete", b.ssid as string, b.op === "add" ? ((b.pass as string | undefined) ?? "") : undefined);
        return send(res, 204);
      }
      case "/api/wifi/cancel":
        if (!Number.isInteger(b.id)) return send(res, 400, "id must be a number");
        ctx.config.cancel(b.id as number);
        return send(res, 204);
    }
  }
  req.resume();
  send(res, 404, "not found");
}
