// Starts Agento (the AgentFunnel service): node src/main.ts [--port 41917] [--state PATH] [--config PATH] [--log PATH] [--no-bonjour]
import { homedir } from "node:os";
import { join } from "node:path";
import { parseArgs } from "node:util";
import { Config } from "./config.ts";
import { Lanyard } from "./lanyard.ts";
import { makeLogger } from "./log.ts";
import { Store } from "./store.ts";
import { DEFAULT_PORT, startServer } from "./server.ts";

process.title = "Agento";
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
const config = new Config({ path: values.config, log });
config.load();

try {
  const running = await startServer({ store, config, lanyard: new Lanyard(), port, bonjour: !values["no-bonjour"], log });
  const stop = () => running.close().then(() => process.exit(0));
  process.on("SIGINT", stop);
  process.on("SIGTERM", stop);
  log(`agentfunnel listening on 0.0.0.0:${running.port} (state ${values.state})`);
} catch (e) {
  if ((e as NodeJS.ErrnoException).code === "EADDRINUSE") {
    console.error(`agentfunnel: port ${port} already in use (is another Agento running?)`);
  } else {
    console.error(`agentfunnel: ${(e as Error).message}`);
  }
  process.exit(1);
}
