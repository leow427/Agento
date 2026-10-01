// Starts AgentFunnel: node src/main.ts [--port 41917] [--state PATH] [--config PATH] [--log PATH] [--no-bonjour]
import { homedir } from "node:os";
import { join } from "node:path";
import { parseArgs } from "node:util";
import { makeLogger } from "./log.ts";
import { Store } from "./store.ts";
import { DEFAULT_PORT, startServer } from "./server.ts";

process.title = "AgentFunnel";
const dir = join(homedir(), ".agentfunnel");
const { values } = parseArgs({
  options: {
    port: { type: "string", default: String(DEFAULT_PORT) },
    state: { type: "string", default: join(dir, "state.json") },
    config: { type: "string", default: join(dir, "config.json") },
    log: { type: "string" },
    "no-bonjour": { type: "boolean", default: false },
  },
});

const port = Number(values.port);
const log = makeLogger(values.log);
const store = new Store({ path: values.state, log });
store.load();

try {
  const running = await startServer({ store, port, bonjour: !values["no-bonjour"], log });
  const stop = () => running.close().then(() => process.exit(0));
  process.on("SIGINT", stop);
  process.on("SIGTERM", stop);
  log(`agentfunnel listening on 0.0.0.0:${running.port} (state ${values.state})`);
} catch (e) {
  if ((e as NodeJS.ErrnoException).code === "EADDRINUSE") {
    console.error(`agentfunnel: port ${port} already in use (is another AgentFunnel running?)`);
  } else {
    console.error(`agentfunnel: ${(e as Error).message}`);
  }
  process.exit(1);
}
