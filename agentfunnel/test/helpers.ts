import { spawn } from "node:child_process";
import { createServer } from "node:net";
import { fileURLToPath } from "node:url";

export const SRC = fileURLToPath(new URL("../src/", import.meta.url));

export function freePort(): Promise<number> {
  return new Promise((resolve) => {
    const s = createServer();
    s.listen(0, "127.0.0.1", () => {
      const port = (s.address() as { port: number }).port;
      s.close(() => resolve(port));
    });
  });
}

export type Result = { code: number | null; stdout: string; stderr: string; ms: number };

// Runs `node <script> ...args`, feeding `input` on stdin; resolves when the process exits.
export function run(script: string, args: string[], input = "", env: Record<string, string> = {}): Promise<Result> {
  return new Promise((resolve) => {
    const t0 = Date.now();
    const p = spawn(process.execPath, ["--no-warnings", SRC + script, ...args], { env: { ...process.env, ...env } });
    let stdout = "";
    let stderr = "";
    p.stdout.on("data", (d) => (stdout += d));
    p.stderr.on("data", (d) => (stderr += d));
    p.on("exit", (code) => resolve({ code, stdout, stderr, ms: Date.now() - t0 }));
    p.stdin.on("error", () => {});
    p.stdin.end(input);
  });
}
