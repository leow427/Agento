// Starts AgentFunnel: node src/main.ts [--port 8765] [--state PATH] [--no-bonjour]
import { homedir } from "node:os";
import { join } from "node:path";
import { parseArgs } from "node:util";
import { Store } from "./store.ts";
import { startServer } from "./server.ts";

const { values } = parseArgs({
  options: {
    port: { type: "string", default: "8765" },
    state: { type: "string", default: join(homedir(), ".agentfunnel", "state.json") },
    "no-bonjour": { type: "boolean", default: false },
  },
});

const port = Number(values.port);
const log = (m: string) => console.log(`${new Date().toISOString()} ${m}`);
const store = new Store({ path: values.state, log: (m) => console.warn(m) });
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
