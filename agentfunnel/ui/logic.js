// Pure helpers for the control page (also imported by the tests).

export const COLOURS = [
  { name: "orange", body: "#D97757", shadow: "#B05A3F" },
  { name: "blue", body: "#5B8DEF", shadow: "#3F66B0" },
  { name: "green", body: "#6BBF59", shadow: "#4A8F3C" },
  { name: "purple", body: "#A67CE0", shadow: "#7A55B0" },
  { name: "yellow", body: "#E8C547", shadow: "#B89A2E" },
  { name: "pink", body: "#E0709B", shadow: "#B04F76" },
];

// The chosen colour first, then the other five in their usual order (same as the lanyard).
export function colourOrder(first) {
  return [first, ...COLOURS.map((_, i) => i).filter((i) => i !== first)];
}

// Colour index per session, ranked among sessions that haven't ended; null for ended ones.
export function sessionColours(sessions, first) {
  const order = colourOrder(first);
  let rank = 0;
  return sessions.map((s) => (s.state === "ended" ? null : order[rank++ % order.length]));
}

export function ago(lastSeen, now) {
  const d = Math.max(0, now - lastSeen);
  if (d < 5) return "just now";
  if (d < 60) return `${Math.floor(d)} s ago`;
  if (d < 3600) return `${Math.floor(d / 60)} min ago`;
  if (d < 86400) return `${Math.floor(d / 3600)} h ago`;
  return `${Math.floor(d / 86400)} d ago`;
}

export function stateWords(state) {
  return { working: "working", your_turn: "your turn", needs_you: "needs you", ended: "ended" }[state] ?? state;
}

// The lanyard's network list once the pending changes are applied (newest first, at most 3).
export function effectiveNets(nets, pending) {
  let list = [...nets];
  for (const op of pending) {
    if (op.op === "add") list = [op.ssid, ...list.filter((s) => s !== op.ssid)].slice(0, 3);
    else list = list.filter((s) => s !== op.ssid);
  }
  return list;
}

export function addWarning(nets, pending, ssid) {
  const list = effectiveNets(nets, pending);
  if (list.includes(ssid) || list.length < 3) return null;
  return `The lanyard keeps 3 networks. Adding "${ssid}" replaces "${list[list.length - 1]}". Continue?`;
}

export function deleteWarning(nets, pending, current, ssid) {
  const after = effectiveNets(nets, pending).filter((s) => s !== ssid);
  if (after.length === 0) return "That's the lanyard's last network. It will go back to first-time setup. Continue?";
  if (ssid === current) return "The lanyard is on this network now. It will disconnect and try your other networks. Continue?";
  return null;
}

const STEPS = [
  { key: "not_installed", title: "Install Tailscale",
    help: { text: "Download Tailscale for Mac and install it.", link: { href: "https://tailscale.com/download/mac", label: "tailscale.com/download/mac" } } },
  { key: "not_running", title: "Open the Tailscale app",
    help: { text: "Open Tailscale from your Applications folder. Its icon appears in the menu bar." } },
  { key: "signed_out", title: "Sign in to Tailscale",
    help: { text: "Click the Tailscale icon in the menu bar and choose Log in." } },
  { key: "off", title: "Turn Tailscale on",
    help: { text: "Click the Tailscale icon in the menu bar and switch it on." } },
  { key: "funnel_not_allowed", title: "Allow Funnel",
    help: { text: "Funnel is what lets the lanyard reach this Mac from anywhere. Follow Tailscale's guide to allow it.",
      link: { href: "https://tailscale.com/kb/1223/funnel", label: "Tailscale Funnel guide" } } },
];

export function tailscaleSteps(step, name) {
  const at = step === "ready" ? STEPS.length : Math.max(0, STEPS.findIndex((s) => s.key === step));
  const out = STEPS.map((s, i) => ({ title: s.title, help: s.help, state: i < at ? "done" : i === at ? "current" : "todo" }));
  out.push({
    title: step === "ready" && name ? `Ready for away mode (coming next) · ${name}` : "Ready for away mode (coming next)",
    state: step === "ready" ? "done" : "todo",
  });
  return out;
}
