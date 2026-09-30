// Claude Code hook: stdin JSON + sent_at -> POST /event. No other logic. Always exits 0, within ~1 s.
import { request } from "node:http";

const URL_ = process.env.AGENTFUNNEL_URL ?? "http://127.0.0.1:8765/event";
const TIMEOUT_MS = 1000;

const done = () => process.exit(0);
process.on("uncaughtException", done);
setTimeout(done, TIMEOUT_MS).unref(); // hard deadline, even if stdin or the server hangs

const chunks: Buffer[] = [];
process.stdin.on("data", (c: Buffer) => chunks.push(c));
process.stdin.on("error", done);
process.stdin.on("end", () => {
  let payload: unknown;
  try {
    payload = JSON.parse(Buffer.concat(chunks).toString("utf8"));
  } catch {
    return done();
  }
  if (typeof payload !== "object" || payload === null || Array.isArray(payload)) return done();
  const body = JSON.stringify({ ...payload, sent_at: Date.now() / 1000 });
  const req = request(URL_, {
    method: "POST",
    headers: { "content-type": "application/json", "content-length": Buffer.byteLength(body) },
    timeout: TIMEOUT_MS,
  });
  req.on("response", (res) => {
    res.resume();
    res.on("end", done);
  });
  req.on("timeout", done);
  req.on("error", done);
  req.end(body);
});
