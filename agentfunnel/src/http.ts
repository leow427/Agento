// Response and body helpers shared by the lanyard/hook routes and the page.
import type { IncomingMessage, ServerResponse } from "node:http";

export const MAX_BODY = 16 * 1024 * 1024;

export function send(res: ServerResponse, status: number, body = "", type = "text/plain") {
  res.writeHead(status, body === "" ? {} : { "content-type": type, "cache-control": "no-store" });
  res.end(body);
}

// Reads the body up to `max` bytes. Past the cap the rest is drained and discarded, so the client
// still gets a clean 413 instead of a reset connection.
export function readBody(req: IncomingMessage, max = MAX_BODY): Promise<Buffer | null> {
  return new Promise((resolve, reject) => {
    const chunks: Buffer[] = [];
    let size = 0;
    let tooBig = Number(req.headers["content-length"] ?? 0) > max;
    req.on("data", (c: Buffer) => {
      if (tooBig) return;
      size += c.length;
      if (size > max) {
        tooBig = true;
        chunks.length = 0;
      } else chunks.push(c);
    });
    req.on("end", () => resolve(tooBig ? null : Buffer.concat(chunks)));
    req.on("error", reject);
  });
}
