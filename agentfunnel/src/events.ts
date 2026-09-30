// Hook payload -> (state change, message text). Pure functions, no I/O.

export type SessionState = "your_turn" | "working" | "needs_you" | "ended";
export type HookPayload = Record<string, unknown>;
export type Interpretation = { state: SessionState | undefined; text: string | undefined };

const STATE_BY_EVENT = new Map<string, SessionState>([
  ["SessionStart", "your_turn"],
  ["Stop", "your_turn"],
  ["UserPromptSubmit", "working"],
  ["PreToolUse", "working"],
  ["PostToolUse", "working"],
  ["Notification", "needs_you"],
  ["SessionEnd", "ended"],
]);

export const TOOL_LIMIT = 80;
export const TEXT_LIMIT = 200;

// Collapse whitespace runs to one space, then cut to `limit` code points with "…" as the last one.
export function clip(text: string, limit: number): string {
  const chars = [...text.replace(/\s+/g, " ").trim()];
  return chars.length <= limit ? chars.join("") : chars.slice(0, limit - 1).join("") + "…";
}

function str(v: unknown): string | undefined {
  return typeof v === "string" && v.trim() !== "" ? v : undefined;
}

function baseName(path: string | undefined): string | undefined {
  return path === undefined ? undefined : str(path.split("/").pop());
}

function host(url: string | undefined): string | undefined {
  if (url === undefined) return undefined;
  try {
    return str(new URL(url).host);
  } catch {
    return undefined;
  }
}

function toolDetail(tool: string, input: Record<string, unknown>): string | undefined {
  switch (tool) {
    case "Bash":
      return str(input.command);
    case "Edit":
    case "Write":
    case "Read":
      return baseName(str(input.file_path));
    case "NotebookEdit":
      return baseName(str(input.notebook_path) ?? str(input.file_path));
    case "Grep":
    case "Glob":
      return str(input.pattern);
    case "WebFetch":
      return host(str(input.url));
    case "WebSearch":
      return str(input.query);
    case "Task":
    case "Agent":
      return str(input.description);
    default:
      return undefined;
  }
}

function messageText(p: HookPayload): string | undefined {
  switch (p.hook_event_name) {
    case "PreToolUse": {
      const tool = str(p.tool_name) ?? "Tool";
      const input = typeof p.tool_input === "object" && p.tool_input !== null ? (p.tool_input as Record<string, unknown>) : {};
      const detail = toolDetail(tool, input);
      return clip(detail === undefined ? tool : `${tool}: ${detail}`, TOOL_LIMIT);
    }
    case "Notification": {
      const m = str(p.message);
      return m === undefined ? undefined : clip(m, TEXT_LIMIT);
    }
    case "Stop": {
      const m = str(p.last_assistant_message);
      return m === undefined ? undefined : clip(m, TEXT_LIMIT);
    }
    case "UserPromptSubmit": {
      const m = str(p.prompt);
      return m === undefined ? undefined : clip("> " + m, TEXT_LIMIT);
    }
    default:
      return undefined;
  }
}

export function interpret(p: HookPayload): Interpretation {
  const name = typeof p.hook_event_name === "string" ? p.hook_event_name : "";
  return { state: STATE_BY_EVENT.get(name), text: messageText(p) };
}
