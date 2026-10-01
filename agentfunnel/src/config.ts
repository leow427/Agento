// On/off, the first character colour, and the queue of Wi-Fi changes waiting for the lanyard.
// Saved to config.json with mode 0600, because a queued change can hold a Wi-Fi password.
import { chmodSync, mkdirSync, readFileSync, renameSync, writeFileSync } from "node:fs";
import { dirname } from "node:path";

export const COLOUR_COUNT = 6;

// Screen brightness in percent: awake ("max") and after 30 s untouched ("sleep"). Defaults are the
// lanyard's behaviour before this was adjustable.
export const BRIGHTNESS_MIN = 5;
export const BRIGHTNESS_MAX = 100;
export const DEFAULT_BRIGHTNESS = { max: 100, sleep: 20 };
export type Brightness = { max: number; sleep: number };

// null = fine.
export function validateBrightness(max: unknown, sleep: unknown): string | null {
  for (const v of [max, sleep])
    if (!Number.isInteger(v) || (v as number) < BRIGHTNESS_MIN || (v as number) > BRIGHTNESS_MAX)
      return `brightness must be ${BRIGHTNESS_MIN}-${BRIGHTNESS_MAX}`;
  if ((sleep as number) > (max as number)) return "sleep brightness can't be above max brightness";
  return null;
}

export type WifiOp = { id: number; op: "add" | "delete"; ssid: string; pass?: string };
export type PublicWifiOp = { id: number; op: "add" | "delete"; ssid: string };

// Same rules as the lanyard's validateCreds (bytes, not characters). null = fine.
export function validateWifi(op: unknown, ssid: unknown, pass: unknown): string | null {
  if (op !== "add" && op !== "delete") return "op must be add or delete";
  if (typeof ssid !== "string" || ssid.length === 0) return "pick a network";
  if (Buffer.byteLength(ssid) > 32) return "network name is too long";
  if (op === "delete") return null;
  const p = pass ?? "";
  if (typeof p !== "string") return "bad password";
  const n = Buffer.byteLength(p);
  if (n !== 0 && n < 8) return "Wi-Fi passwords are at least 8 characters";
  if (n > 63) return "password is too long";
  return null;
}

export class Config {
  private on = true;
  private first = 0;
  private bright: Brightness = { ...DEFAULT_BRIGHTNESS };
  private nextOpId = 1;
  private ops: WifiOp[] = [];
  private readonly path: string | undefined;
  private readonly log: (m: string) => void;
  private readonly now: () => number;

  constructor(opts: { path?: string; log?: (m: string) => void; now?: () => number } = {}) {
    this.path = opts.path;
    this.log = opts.log ?? ((m) => console.warn(m));
    this.now = opts.now ?? (() => Date.now() / 1000);
  }

  // With no usable config.json (first run, or it was lost), op ids start from the clock in seconds,
  // so they stay above any id the lanyard has already applied (it remembers the last one in NVS).
  private seedIds(): void {
    this.nextOpId = Math.max(1, Math.floor(this.now()));
  }

  get running(): boolean {
    return this.on;
  }
  get colour(): number {
    return this.first;
  }

  get brightness(): Brightness {
    return { ...this.bright };
  }

  setBrightness(max: number, sleep: number): void {
    const err = validateBrightness(max, sleep);
    if (err !== null) throw new RangeError(err);
    this.bright = { max, sleep };
    this.save();
  }

  setRunning(on: boolean): void {
    this.on = on;
    this.save();
  }

  setColour(c: number): void {
    if (!Number.isInteger(c) || c < 0 || c >= COLOUR_COUNT) throw new RangeError(`colour must be 0-${COLOUR_COUNT - 1}`);
    this.first = c;
    this.save();
  }

  addWifi(op: "add" | "delete", ssid: string, pass?: string): WifiOp {
    const o: WifiOp = op === "add" ? { id: this.nextOpId++, op, ssid, pass: pass ?? "" } : { id: this.nextOpId++, op, ssid };
    this.ops.push(o);
    this.save();
    return { ...o };
  }

  cancel(id: number): boolean {
    const before = this.ops.length;
    this.ops = this.ops.filter((o) => o.id !== id);
    if (this.ops.length === before) return false;
    this.save();
    return true;
  }

  // What the lanyard still needs: ops above its last applied id (all of them before it has reported).
  pendingAfter(applied: number | null): WifiOp[] {
    return this.ops.filter((o) => applied === null || o.id > applied).map((o) => ({ ...o }));
  }

  pendingPublic(): PublicWifiOp[] {
    return this.ops.map(({ id, op, ssid }) => ({ id, op, ssid }));
  }

  // The lanyard has applied every op up to `applied`.
  lanyardApplied(applied: number): void {
    if (!Number.isInteger(applied) || applied < 0) return;
    if (applied >= this.nextOpId) {
      // Its counter comes from an older config.json: nothing queued here has been seen, so
      // renumber it above what the lanyard has applied.
      for (const o of this.ops) o.id = ++applied;
      this.nextOpId = applied + 1;
      this.save();
      return;
    }
    const before = this.ops.length;
    this.ops = this.ops.filter((o) => o.id > applied);
    if (this.ops.length !== before) this.save();
  }

  save(): void {
    if (this.path === undefined) return;
    const data = { running: this.on, colour: this.first, brightness: this.bright, nextOpId: this.nextOpId, wifiOps: this.ops };
    mkdirSync(dirname(this.path), { recursive: true });
    const tmp = this.path + ".tmp";
    writeFileSync(tmp, JSON.stringify(data), { mode: 0o600 });
    chmodSync(tmp, 0o600); // in case an old tmp file had wider permissions
    renameSync(tmp, this.path);
  }

  load(): void {
    if (this.path === undefined) return;
    let raw: string;
    try {
      raw = readFileSync(this.path, "utf8");
    } catch (e) {
      if ((e as NodeJS.ErrnoException).code !== "ENOENT") this.log(`agentfunnel: cannot read config ${this.path}: ${e}`);
      this.seedIds();
      return;
    }
    try {
      const d = JSON.parse(raw);
      const okColour = Number.isInteger(d?.colour) && d.colour >= 0 && d.colour < COLOUR_COUNT;
      if (typeof d?.running !== "boolean" || !okColour || !Number.isInteger(d.nextOpId) || d.nextOpId < 1 || !Array.isArray(d.wifiOps))
        throw new Error("bad shape");
      for (const o of d.wifiOps)
        if (!Number.isInteger(o?.id) || validateWifi(o.op, o.ssid, o.pass) !== null) throw new Error("bad wifi op");
      this.on = d.running;
      this.first = d.colour;
      // Absent in files from before brightness was adjustable: keep the defaults.
      this.bright = d.brightness !== undefined && validateBrightness(d.brightness?.max, d.brightness?.sleep) === null
        ? { max: d.brightness.max, sleep: d.brightness.sleep }
        : { ...DEFAULT_BRIGHTNESS };
      this.nextOpId = d.nextOpId;
      this.ops = d.wifiOps;
    } catch (e) {
      this.log(`agentfunnel: ignoring unreadable config ${this.path}: ${(e as Error).message}`);
      this.seedIds();
    }
  }
}
