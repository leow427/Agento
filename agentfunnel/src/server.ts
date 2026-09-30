// HTTP handlers and the Bonjour child process.
import { createServer } from "node:http";
import type { IncomingMessage, ServerResponse } from "node:http";
import { spawn } from "node:child_process";
import type { ChildProcess } from "node:child_process";
import type { AddressInfo } from "node:net";
import type { Store } from "./store.ts";

export const MAX_BODY = 16 * 1024 * 1024;
export const SERVICE_TYPE = "_agentfunnel._tcp";

export type ServerOptions = {
  store: Store;
  port: number;
  host?: string;
  bonjour?: boolean;
  log?: (msg: string) => void;
};

export type Running = { port: number; close: () => Promise<void> };

function send(res: ServerResponse, status: number, body = "", type = "text/plain") {
  res.writeHead(status, body === "" ? {} : { "content-type": type });
  res.end(body);
}

// Reads the body up to MAX_BODY. Past the cap the rest is drained and discarded, so the client
// still gets a clean 413 instead of a reset connection.
function readBody(req: IncomingMessage): Promise<Buffer | null> {
  return new Promise((resolve, reject) => {
    const chunks: Buffer[] = [];
    let size = 0;
    let tooBig = Number(req.headers["content-length"] ?? 0) > MAX_BODY;
    req.on("data", (c: Buffer) => {
      if (tooBig) return;
      size += c.length;
      if (size > MAX_BODY) {
        tooBig = true;
        chunks.length = 0;
      } else chunks.push(c);
    });
    req.on("end", () => resolve(tooBig ? null : Buffer.concat(chunks)));
    req.on("error", reject);
  });
}

async function handle(store: Store, log: (m: string) => void, req: IncomingMessage, res: ServerResponse) {
  const url = new URL(req.url ?? "/", "http://x");
  if (req.method === "POST" && url.pathname === "/event") {
    const body = await readBody(req);
    if (body === null) return send(res, 413, "too large");
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
    const since = Number(url.searchParams.get("since") ?? 0);
    return send(res, 200, JSON.stringify(store.snapshot(Number.isFinite(since) ? since : 0)), "application/json");
  }
  if (req.method === "GET" && url.pathname === "/health") return send(res, 200, "ok");
  req.resume();
  send(res, 404, "not found");
}

export function startServer(opts: ServerOptions): Promise<Running> {
  const log = opts.log ?? (() => {});
  const server = createServer((req, res) => {
    handle(opts.store, log, req, res).catch((e) => {
      log(`request failed: ${e}`);
      if (!res.headersSent) send(res, 500, "error");
    });
  });
  return new Promise((resolve, reject) => {
    server.once("error", reject);
    server.listen(opts.port, opts.host ?? "0.0.0.0", () => {
      server.off("error", reject);
      const port = (server.address() as AddressInfo).port;
      let dnsSd: ChildProcess | null = null;
      if (opts.bonjour !== false) {
        dnsSd = spawn("dns-sd", ["-R", "AgentFunnel", SERVICE_TYPE, "local", String(port)], { stdio: "ignore" });
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
