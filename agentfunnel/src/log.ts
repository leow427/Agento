// Appends timestamped lines to a file, rolling it over to <file>.1 above maxBytes.
// Without a path, lines go to stdout (running by hand in a terminal).
import { appendFileSync, mkdirSync, renameSync, statSync } from "node:fs";
import { dirname } from "node:path";

export const MAX_LOG_BYTES = 5 * 1024 * 1024;

export function makeLogger(path?: string, maxBytes = MAX_LOG_BYTES, now = () => new Date()): (msg: string) => void {
  return (msg) => {
    const line = `${now().toISOString()} ${msg}\n`;
    if (path === undefined) {
      process.stdout.write(line);
      return;
    }
    try {
      mkdirSync(dirname(path), { recursive: true });
      let size = 0;
      try {
        size = statSync(path).size;
      } catch {}
      if (size > 0 && size + Buffer.byteLength(line) > maxBytes) renameSync(path, path + ".1");
      appendFileSync(path, line);
    } catch (e) {
      process.stderr.write(`agentfunnel: cannot write log ${path}: ${e}\n${line}`);
    }
  };
}
