// What the lanyard last reported, and when we last heard from it. Memory only: after a restart
// the page says "not seen yet" until its next poll (about a second).
import { COLOUR_COUNT, validateBrightness } from "./config.ts";

export type Battery = { present: boolean; charging: boolean; percent: number };
export type Report = {
  wifi: string; nets: string[]; colour: number; battery: Battery; applied: number;
  brightness?: { max: number; sleep: number };  // absent from older firmware
};
export type LanyardView = { lastSeen: number } & Partial<Report>;

export function parseReport(x: unknown): Report | null {
  if (typeof x !== "object" || x === null || Array.isArray(x)) return null;
  const o = x as Record<string, unknown>;
  const b = o.battery as Record<string, unknown> | null;
  const ok =
    typeof o.wifi === "string" &&
    Array.isArray(o.nets) && o.nets.every((n) => typeof n === "string") &&
    Number.isInteger(o.colour) && (o.colour as number) >= 0 && (o.colour as number) < COLOUR_COUNT &&
    typeof b === "object" && b !== null &&
    typeof b.present === "boolean" && typeof b.charging === "boolean" && typeof b.percent === "number" &&
    Number.isInteger(o.applied) && (o.applied as number) >= 0;
  if (!ok) return null;
  const br = o.brightness as Record<string, unknown> | undefined;
  const brightness = br !== undefined && validateBrightness(br.max, br.sleep) === null
    ? { max: br.max as number, sleep: br.sleep as number }
    : undefined;
  return {
    wifi: o.wifi as string,
    nets: [...(o.nets as string[])],
    colour: o.colour as number,
    battery: { present: b.present as boolean, charging: b.charging as boolean, percent: b.percent as number },
    applied: o.applied as number,
    ...(brightness && { brightness }),
  };
}

export class Lanyard {
  private lastSeen: number | null = null;
  private last: Report | null = null;
  private readonly now: () => number;

  constructor(now: () => number = () => Date.now() / 1000) {
    this.now = now;
  }

  seen(): void {
    this.lastSeen = this.now();
  }

  report(r: Report): void {
    this.last = r;
    this.seen();
  }

  applied(): number | null {
    return this.last?.applied ?? null;
  }

  view(): LanyardView | null {
    if (this.lastSeen === null) return null;
    return { lastSeen: this.lastSeen, ...(this.last ?? {}) };
  }
}
