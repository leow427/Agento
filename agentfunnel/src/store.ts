// Sessions, slots, messages, seq, expiry, ordering, load/save. The clock is injectable (seconds).
import { mkdirSync, readFileSync, renameSync, writeFileSync } from "node:fs";
import { basename, dirname } from "node:path";
import { interpret } from "./events.ts";
import type { HookPayload, SessionState } from "./events.ts";

export const MAX_SESSIONS = 6;
export const MAX_MESSAGES = 20;
export const ENDED_TTL = 5 * 60;
export const SILENT_TTL = 30 * 60;

export type Message = { seq: number; sid: string; ts: number; text: string };
export type PublicSession = { id: string; slot: number; project: string; state: SessionState; updated: number };
export type Snapshot = { seq: number; sessions: PublicSession[]; messages: Message[] };

type Session = PublicSession & {
  stateAt: number; // sent_at of the last event that changed the state
  endedAt: number | null; // server time the session entered `ended`
  messages: Message[];
};

export type StoreOptions = {
  now?: () => number;
  path?: string;
  log?: (msg: string) => void;
};

const STATES: readonly string[] = ["your_turn", "working", "needs_you", "ended"];

export class Store {
  private seq = 0;
  private sessions = new Map<string, Session>();
  private readonly now: () => number;
  private readonly path: string | undefined;
  private readonly log: (msg: string) => void;

  constructor(opts: StoreOptions = {}) {
    this.now = opts.now ?? (() => Date.now() / 1000);
    this.path = opts.path;
    this.log = opts.log ?? ((m) => console.warn(m));
  }

  // Returns true if the event changed anything.
  apply(p: HookPayload): boolean {
    const sid = typeof p.session_id === "string" ? p.session_id : "";
    if (sid === "") return false;
    const now = this.now();
    const sentAt = typeof p.sent_at === "number" && Number.isFinite(p.sent_at) ? p.sent_at : now;
    const { state, text } = interpret(p);

    let s = this.sessions.get(sid);
    if (s === undefined) {
      // A full house may be holding slots that are already due to expire.
      const slot = this.freeSlot() ?? (this.expire() ? this.freeSlot() : undefined);
      if (slot === undefined) return false;
      s = { id: sid, slot, project: "", state: state ?? "your_turn", updated: now, stateAt: sentAt, endedAt: null, messages: [] };
      if (s.state === "ended") s.endedAt = now;
      this.sessions.set(sid, s);
    } else if (state !== undefined && sentAt >= s.stateAt) {
      s.state = state;
      s.stateAt = sentAt;
      s.endedAt = state === "ended" ? now : null;
    }
    s.updated = now;
    if (s.project === "" && typeof p.cwd === "string") s.project = basename(p.cwd);
    if (text !== undefined) {
      s.messages.push({ seq: ++this.seq, sid, ts: sentAt, text });
      if (s.messages.length > MAX_MESSAGES) s.messages.splice(0, s.messages.length - MAX_MESSAGES);
    }
    this.save();
    return true;
  }

  // Removes sessions that ended 5 min ago or went silent for 30 min. Returns true if any were removed.
  expire(): boolean {
    const now = this.now();
    let removed = false;
    for (const [id, s] of this.sessions) {
      const endedLongAgo = s.state === "ended" && s.endedAt !== null && now - s.endedAt >= ENDED_TTL;
      if (endedLongAgo || now - s.updated >= SILENT_TTL) {
        this.sessions.delete(id);
        removed = true;
      }
    }
    if (removed) this.save();
    return removed;
  }

  snapshot(since: number): Snapshot {
    this.expire();
    const from = since > this.seq || !(since >= 0) ? 0 : since;
    const all = [...this.sessions.values()].sort((a, b) => a.slot - b.slot);
    return {
      seq: this.seq,
      sessions: all.map(({ id, slot, project, state, updated }) => ({ id, slot, project, state, updated })),
      messages: all
        .flatMap((s) => s.messages)
        .filter((m) => m.seq > from)
        .sort((a, b) => a.seq - b.seq)
        .map((m) => ({ ...m })),
    };
  }

  save(): void {
    if (this.path === undefined) return;
    const data = { seq: this.seq, sessions: [...this.sessions.values()] };
    mkdirSync(dirname(this.path), { recursive: true });
    const tmp = this.path + ".tmp";
    writeFileSync(tmp, JSON.stringify(data));
    renameSync(tmp, this.path);
  }

  load(): void {
    if (this.path === undefined) return;
    let raw: string;
    try {
      raw = readFileSync(this.path, "utf8");
    } catch (e) {
      if ((e as NodeJS.ErrnoException).code !== "ENOENT") this.log(`agentfunnel: cannot read state ${this.path}: ${e}`);
      return;
    }
    try {
      const data = JSON.parse(raw);
      if (!Number.isInteger(data?.seq) || data.seq < 0 || !Array.isArray(data.sessions)) throw new Error("bad shape");
      const sessions = new Map<string, Session>();
      for (const s of data.sessions) {
        if (!isSession(s)) throw new Error("bad session");
        sessions.set(s.id, s);
      }
      this.seq = data.seq;
      this.sessions = sessions;
    } catch (e) {
      this.log(`agentfunnel: ignoring unreadable state ${this.path}: ${(e as Error).message}`);
      this.seq = 0;
      this.sessions = new Map();
      return;
    }
    this.expire();
  }

  private freeSlot(): number | undefined {
    const used = new Set([...this.sessions.values()].map((s) => s.slot));
    for (let i = 0; i < MAX_SESSIONS; i++) if (!used.has(i)) return i;
    return undefined;
  }
}

function isSession(s: unknown): s is Session {
  if (typeof s !== "object" || s === null) return false;
  const o = s as Record<string, unknown>;
  return (
    typeof o.id === "string" &&
    Number.isInteger(o.slot) && (o.slot as number) >= 0 && (o.slot as number) < MAX_SESSIONS &&
    typeof o.project === "string" &&
    typeof o.state === "string" && STATES.includes(o.state) &&
    typeof o.updated === "number" &&
    typeof o.stateAt === "number" &&
    (o.endedAt === null || typeof o.endedAt === "number") &&
    Array.isArray(o.messages)
  );
}
