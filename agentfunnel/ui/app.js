// AgentFunnel's page: polls /api/status every 2 s and redraws. Data from the lanyard (network
// names, project names) only ever goes in through textContent.
import { COLOURS, colourOrder, sessionColours, ago, stateWords, addWarning, deleteWarning, tailscaleSteps } from "./logic.js";

const $ = (id) => document.getElementById(id);
let status = null;

function el(tag, props = {}, ...kids) {
  const e = document.createElement(tag);
  Object.assign(e, props);
  for (const k of kids) e.append(k);
  return e;
}

// Draws one frame of the real art, with the body (O) and shadow (D) swapped for the colour.
function drawBuddy(canvas, colour, frame = 0) {
  const D = window.DATA;
  const g = canvas.getContext("2d");
  const cell = canvas.width / D.grid;
  const c = COLOURS[colour] ?? COLOURS[0];
  g.clearRect(0, 0, canvas.width, canvas.height);
  D.frames[frame].forEach((row, y) => {
    for (let x = 0; x < row.length; x++) {
      const ch = row[x];
      const fill = ch === "O" ? c.body : ch === "D" ? c.shadow : D.palette[ch];
      if (!fill) continue;
      g.fillStyle = fill;
      g.fillRect(x * cell, y * cell, cell, cell);
    }
  });
}

function buddy(colour, px) {
  const c = el("canvas", { width: px, height: px });
  drawBuddy(c, colour);
  return c;
}

async function post(path, body) {
  const r = await fetch(path, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify(body) });
  if (!r.ok) throw new Error((await r.text()) || `failed (${r.status})`);
  await refresh();
}

async function refresh() {
  try {
    const r = await fetch("/api/status", { cache: "no-store" });
    if (!r.ok) throw new Error(String(r.status));
    status = await r.json();
    $("offline").hidden = true;
    render(status);
  } catch {
    $("offline").hidden = false;
  }
}

function render(s) {
  $("toggle").textContent = s.running ? "Stop" : "Start";
  $("toggle").classList.toggle("stop", s.running);
  $("run-state").textContent = s.running ? "running · sessions go to your lanyard" : "stopped · the lanyard shows disconnected";
  renderLanyard(s);
  renderSessions(s);
  renderColour(s);
  renderWifi(s);
  renderAway(s);
}

function renderLanyard(s) {
  const l = s.lanyard;
  $("seen").textContent = l ? ago(l.lastSeen, Date.now() / 1000) : "not seen yet";
  $("net").textContent = l?.wifi || "—";
  const b = l?.battery;
  $("battery").textContent = !b ? "—" : !b.present ? "no battery" : `${b.percent}%${b.charging ? " · charging" : ""}`;
}

function renderSessions(s) {
  const colours = sessionColours(s.sessions, s.colour);
  $("session-list").replaceChildren(
    ...s.sessions.map((x, i) => {
      const name = el("span", { className: "project", textContent: x.project || "session" });
      if (colours[i] !== null) name.style.color = COLOURS[colours[i]].body;
      return el("li", {}, name, el("span", { className: "state", textContent: stateWords(x.state) }));
    }),
  );
  $("no-sessions").hidden = s.sessions.length > 0;
}

let shownColour = -1;
function renderColour(s) {
  if (shownColour !== s.colour) {
    shownColour = s.colour;
    for (const b of $("swatches").children) b.classList.toggle("on", Number(b.dataset.colour) === s.colour);
    $("order").replaceChildren(...colourOrder(s.colour).map((c) => buddy(c, 58)));
  }
  $("colour-wait").hidden = s.lanyard?.colour === s.colour;
}

function renderWifi(s) {
  const l = s.lanyard;
  const known = Array.isArray(l?.nets);
  $("wifi-note").textContent = known ? "" : "The lanyard hasn't reported its networks yet. Switch it on near your Wi-Fi.";
  for (const e of $("add-net").elements) e.disabled = !known;
  $("nets").replaceChildren(
    ...(known ? l.nets : []).map((ssid) => {
      const del = el("button", { className: "small", textContent: "delete" });
      del.onclick = () => removeNet(ssid);
      const tag = ssid === l.wifi ? el("span", { className: "tag", textContent: "connected" }) : "";
      return el("li", {}, el("span", { textContent: ssid }), tag, del);
    }),
  );
  $("pending").replaceChildren(
    ...s.wifiPending.map((op) => {
      const cancel = el("button", { className: "small", textContent: "cancel" });
      cancel.onclick = () => post("/api/wifi/cancel", { id: op.id }).catch(showWifiError);
      const what = `${op.op === "add" ? "adding" : "deleting"} ${op.ssid}`;
      return el("li", { className: "wait" }, el("span", { textContent: what }), el("span", { textContent: "waiting for lanyard…" }), cancel);
    }),
  );
}

function renderAway(s) {
  $("steps").replaceChildren(
    ...tailscaleSteps(s.tailscale.step, s.tailscale.name).map((st) => {
      const mark = st.state === "done" ? "✓" : st.state === "current" ? "→" : "·";
      const li = el("li", { className: st.state }, el("span", { className: "mark", textContent: mark }), el("span", { textContent: st.title }));
      if (st.state === "current" && st.help) {
        const p = el("p", { className: "help", textContent: st.help.text + " " });
        if (st.help.link) p.append(el("a", { href: st.help.link.href, target: "_blank", rel: "noopener", textContent: st.help.link.label }));
        li.append(p);
      }
      return li;
    }),
  );
}

function showWifiError(err) {
  $("add-error").textContent = err.message;
}

function removeNet(ssid) {
  const l = status.lanyard;
  const w = deleteWarning(l.nets, status.wifiPending, l.wifi, ssid);
  if (w && !confirm(w)) return;
  post("/api/wifi", { op: "delete", ssid }).catch(showWifiError);
}

function setup() {
  $("swatches").replaceChildren(
    ...COLOURS.map((c, i) => {
      const b = el("button", { className: "swatch", title: c.name });
      b.dataset.colour = String(i);
      b.append(buddy(i, 58), el("span", { textContent: c.name }));
      b.onclick = () => post("/api/colour", { colour: i }).catch(() => {});
      return b;
    }),
  );
  $("toggle").onclick = () => {
    if (status) post("/api/running", { running: !status.running }).catch(() => {});
  };
  $("show").onchange = () => ($("pass").type = $("show").checked ? "text" : "password");
  $("add-net").onsubmit = async (e) => {
    e.preventDefault();
    const ssid = $("ssid").value;
    const pass = $("pass").value;
    const w = addWarning(status.lanyard.nets, status.wifiPending, ssid);
    if (w && !confirm(w)) return;
    try {
      await post("/api/wifi", { op: "add", ssid, pass });
      $("ssid").value = "";
      $("pass").value = "";
      $("add-error").textContent = "";
    } catch (err) {
      showWifiError(err);
    }
  };
  const idle = window.DATA.anims.idle.steps;
  let k = 0;
  drawBuddy($("hero"), 0, idle[0].frame);
  setInterval(() => {
    k = (k + 1) % idle.length;
    drawBuddy($("hero"), status?.colour ?? 0, idle[k].frame);
  }, 400);
  refresh();
  setInterval(refresh, 2000);
}

setup();
