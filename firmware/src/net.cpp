#include "net.h"
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <atomic>
#include "buddy_svg.h"
#include "frames_data.h"
#include "link.h"
#include "provision.h"
#include "slots.h"

#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef WIFI_SSID
#define WIFI_SSID ""
#define WIFI_PASS ""
#endif

static constexpr uint16_t DEFAULT_PORT = 41917;
static constexpr uint32_t POLL_MS = 1000;
static constexpr uint32_t JOIN_MS = 10000;  // how long one join attempt may take
static constexpr int ROUNDS_BEFORE_SETUP = 2;  // full passes over the saved networks before asking for new ones

static SemaphoreHandle_t lock;
static String mailbox;
static bool mailFull = false;
static std::atomic<uint32_t> since{0};  // 0 on boot: the Mac sends its full history
static std::atomic<bool> down{true};
static std::atomic<bool> setupRequested{false};
static std::atomic<uint8_t> firstColour{0};
static std::atomic<bool> stopped{false};  // AgentFunnel is switched off on the Mac
static uint32_t applied = 0;              // last Wi-Fi change from the Mac we applied (NVS "applied")
static BatteryReading batteryNow;         // guarded by `lock`
static bool needRejoin = false, needSetup = false;
static String lastReport;
static uint32_t lastReportAt = 0;

static CredStore store;
static SetupFlow flow;
static SetupView view;  // guarded by `lock`

bool netTake(String& body) {
  if (!mailFull) return false;
  xSemaphoreTake(lock, portMAX_DELAY);
  body = std::move(mailbox);
  mailbox = String();
  mailFull = false;
  xSemaphoreGive(lock);
  return true;
}

void netSetSince(uint32_t seq) { since = seq; }
bool netDisconnected() { return down; }
void netRequestSetup() { setupRequested = true; }
uint8_t netFirstColour() { return firstColour; }

void netSetBattery(const BatteryReading& b) {
  xSemaphoreTake(lock, portMAX_DELAY);
  batteryNow = b;
  xSemaphoreGive(lock);
}

void netSetupView(SetupView& out) {
  xSemaphoreTake(lock, portMAX_DELAY);
  out = view;
  xSemaphoreGive(lock);
}

static void publishView(bool active) {
  xSemaphoreTake(lock, portMAX_DELAY);
  view.active = active;
  view.step = flow.step();
  strlcpy(view.ssid, flow.ssid(), sizeof view.ssid);
  xSemaphoreGive(lock);
}

// ---------- saved networks ----------

static void loadStore() {
  Preferences p;
  p.begin("buddy", true);
  const String text = p.getString("nets", "");
  const uint8_t c = p.getUChar("colour", 0);
  firstColour = c < SLOT_COUNT ? c : 0;
  applied = p.getUInt("applied", 0);
  p.end();
  if (!store.deserialize(text.c_str())) Serial.println("wifi: saved networks unreadable; starting fresh");
  Serial.printf("wifi: %d saved network(s)%s\n", store.count(), *WIFI_SSID ? " + secrets.h" : "");
}

static void saveStore() {
  Preferences p;
  p.begin("buddy", false);
  p.putString("nets", store.serialize().c_str());
  p.end();
}

static void saveColour(uint8_t c) {
  Preferences p;
  p.begin("buddy", false);
  p.putUChar("colour", c);
  p.end();
}

static void saveApplied(uint32_t a) {
  Preferences p;
  p.begin("buddy", false);
  p.putUInt("applied", a);
  p.end();
}

// Saved networks (newest first), then secrets.h as a developer default.
static int candidateCount() { return store.count() + (*WIFI_SSID ? 1 : 0); }
static void candidate(int i, const char** ssid, const char** pass) {
  if (i < store.count()) {
    *ssid = store.at(i).ssid;
    *pass = store.at(i).pass;
  } else {
    *ssid = WIFI_SSID;
    *pass = WIFI_PASS;
  }
}

// ---------- setup mode: our own access point + captive web page ----------

extern const uint8_t portal_html_start[] asm("_binary_src_portal_html_start");
extern const uint8_t portal_html_end[] asm("_binary_src_portal_html_end");

static DNSServer dns;
static WebServer web(80);
static bool setupOn = false;
static bool pendingJoin = false;
static Cred pending;

static uint8_t animByName(const String& name) {
  if (name == "coding") return ANIM_CODING;
  if (name == "needs_you") return ANIM_NEEDS_YOU;
  return ANIM_IDLE;
}

static void sendJson(int code, const String& body) {
  web.sendHeader("Cache-Control", "no-store");
  web.send(code, "application/json", body);
}

static String scanJson() {
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_FAILED || n == -2) n = WiFi.scanNetworks(false, false);
  for (uint32_t t0 = millis(); n == WIFI_SCAN_RUNNING && millis() - t0 < 6000; n = WiFi.scanComplete()) delay(50);
  String out = "[";
  int shown = 0;
  for (int i = 0; i < n && shown < 15; i++) {
    const String ssid = WiFi.SSID(i);
    if (ssid.isEmpty()) continue;
    bool dup = false;  // list each name once, at its strongest
    for (int j = 0; j < n && !dup; j++) dup = j != i && WiFi.SSID(j) == ssid && WiFi.RSSI(j) > WiFi.RSSI(i);
    for (int j = 0; j < i && !dup; j++) dup = WiFi.SSID(j) == ssid && WiFi.RSSI(j) == WiFi.RSSI(i);
    if (dup) continue;
    String esc = ssid;
    esc.replace("\\", "\\\\");
    esc.replace("\"", "\\\"");
    out += String(shown++ ? "," : "") + "{\"ssid\":\"" + esc + "\",\"rssi\":" + WiFi.RSSI(i) +
           ",\"secure\":" + (WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "false" : "true") + "}";
  }
  WiFi.scanDelete();
  return out + "]";
}

static void startSetup() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  xSemaphoreTake(lock, portMAX_DELAY);
  apName(view.apSsid, sizeof view.apSsid, mac);
  apPassword(view.apPass, [] { return (uint32_t)esp_random(); });
  xSemaphoreGive(lock);
  flow = SetupFlow();
  WiFi.disconnect();
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(view.apSsid, view.apPass);
  delay(100);
  const IPAddress ip = WiFi.softAPIP();
  dns.setErrorReplyCode(DNSReplyCode::NoError);
  dns.start(53, "*", ip);

  web.on("/", HTTP_GET, [] {
    web.sendHeader("Cache-Control", "no-store");
    web.send_P(200, "text/html", (const char*)portal_html_start, portal_html_end - portal_html_start);
  });
  web.on("/buddy.svg", HTTP_GET, [] {
    const uint8_t a = animByName(web.arg("a"));
    const int s = constrain(web.arg("s").toInt(), 0, ANIMS[a].count - 1);
    web.sendHeader("Cache-Control", "max-age=86400");
    web.send(200, "image/svg+xml", buddySvg(ANIMS[a].steps[s].frame, 0).c_str());
  });
  web.on("/scan", HTTP_GET, [] { sendJson(200, scanJson()); });
  web.on("/status", HTTP_GET, [] { sendJson(200, flow.statusJson().c_str()); });
  web.on("/connect", HTTP_POST, [] {
    const String ssid = web.arg("ssid"), pass = web.arg("pass");
    if (const char* err = validateCreds(ssid.c_str(), pass.c_str())) {
      sendJson(400, String("{\"error\":\"") + err + "\"}");
      return;
    }
    strlcpy(pending.ssid, ssid.c_str(), sizeof pending.ssid);
    strlcpy(pending.pass, pass.c_str(), sizeof pending.pass);
    pendingJoin = true;
    flow.submit(pending.ssid);
    Serial.printf("setup: page asked to join %s\n", pending.ssid);
    sendJson(202, "{}");
  });
  // Captive portal: every other URL (Apple's hotspot-detect, Android's generate_204...) goes to the page.
  web.onNotFound([] {
    web.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/");
    web.send(302, "text/plain", "");
  });
  web.begin();
  WiFi.scanNetworks(true);  // warm the list before the phone asks
  setupOn = true;
  publishView(true);
  Serial.printf("setup: access point %s up at %s\n", view.apSsid, ip.toString().c_str());
}

static void stopSetup() {
  web.stop();
  dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  setupOn = false;
  publishView(false);
  Serial.println("setup: access point off");
}

// Runs one setup-mode iteration. Returns true once we are online and setup has finished.
static bool setupTick() {
  static uint32_t joinStarted = 0, doneAt = 0;
  static int lastClients = -1;
  dns.processNextRequest();
  web.handleClient();
  const int clients = WiFi.softAPgetStationNum();
  if (clients != lastClients) {
    Serial.printf("setup: %d phone(s) on the access point\n", clients);
    lastClients = clients;
  }
  const SetupStep before = flow.step();
  flow.clients(clients);

  if (pendingJoin) {
    pendingJoin = false;
    WiFi.disconnect(false, false);
    WiFi.begin(pending.ssid, pending.pass);
    joinStarted = millis();
  }
  if (flow.step() == SetupStep::Connecting) {
    if (WiFi.status() == WL_CONNECTED) {
      store.add(pending.ssid, pending.pass);
      saveStore();
      flow.joined(true);
      doneAt = millis();
      Serial.printf("setup: joined %s, saved\n", pending.ssid);
    } else if (millis() - joinStarted > JOIN_MS) {
      WiFi.disconnect(false, false);
      flow.joined(false);
      Serial.printf("setup: could not join %s\n", pending.ssid);
    }
  }
  if (flow.step() != before) {
    Serial.printf("setup step: %s -> %s\n", setupStepName(before), setupStepName(flow.step()));
    publishView(true);
  }
  // Leave the page a few seconds to show "all set" before the access point goes away.
  return flow.step() == SetupStep::Done && millis() - doneAt > 8000;
}

// ---------- normal mode: join, find AgentFunnel, poll ----------

static String host;
static uint16_t port = 0;

static bool resolve() {
  const int n = MDNS.queryService("agentfunnel", "tcp");
  if (n > 0) {
    host = MDNS.address(0).toString();
    port = MDNS.port(0);
    Serial.printf("agentfunnel found at %s:%u (mdns %s)\n", host.c_str(), port, MDNS.hostname(0).c_str());
    return true;
  }
#ifdef AGENTFUNNEL_HOST
  host = AGENTFUNNEL_HOST;
  port = DEFAULT_PORT;
  Serial.printf("agentfunnel not found via mdns; using AGENTFUNNEL_HOST %s:%u\n", host.c_str(), port);
  return true;
#else
  Serial.println("agentfunnel not found via mdns (no AGENTFUNNEL_HOST fallback)");
  return false;
#endif
}

// Settings from the Mac's page, carried in every /state response.
static void applyBits(const LanyardBits& b) {
  if (b.colour >= 0 && b.colour != firstColour) {
    firstColour = b.colour;
    saveColour(b.colour);
    Serial.printf("config: colour %d\n", b.colour);
  }
  for (int i = 0; i < b.opCount; i++)
    if (b.ops[i].id > applied)
      Serial.printf("wifi op %lu: %s %s\n", (unsigned long)b.ops[i].id, opKindName(b.ops[i].kind), b.ops[i].ssid);
  const uint32_t before = applied;
  const String current = WiFi.SSID();
  const OpsResult r = applyWifiOps(store, b.ops, b.opCount, applied, current.c_str());
  if (r.changed) saveStore();
  if (applied != before) saveApplied(applied);
  if (r.currentRemoved) {
    if (candidateCount() == 0) needSetup = true;
    else needRejoin = true;
  }
}

static bool poll() {
  HTTPClient http;
  http.setConnectTimeout(900);
  http.setTimeout(900);
  const String url = "http://" + host + ":" + port + "/state?lanyard=1&since=" + String((uint32_t)since);
  if (!http.begin(url)) return false;
  const int code = http.GET();
  if (code != 200) {
    Serial.printf("poll failed: %s (%d)\n", code < 0 ? HTTPClient::errorToString(code).c_str() : "http", code);
    http.end();
    return false;
  }
  String body = http.getString();
  http.end();
  static LanyardBits bits;  // ~800 bytes: keep it off the task stack
  if (parseLanyardBits(body.c_str(), body.length(), bits)) {
    applyBits(bits);
    if (bits.running == stopped) Serial.println(bits.running ? "agentfunnel switched on" : "agentfunnel switched off on the Mac");
    stopped = !bits.running;
    if (!bits.running) return true;  // keep the sessions as they were; the screen shows disconnected
  }
  xSemaphoreTake(lock, portMAX_DELAY);
  mailbox = std::move(body);  // an unapplied older body is superseded: `since` hasn't moved
  mailFull = true;
  xSemaphoreGive(lock);
  return true;
}

// POST /lanyard: sent after connecting, whenever it changes, and at least every 30 s.
static void sendReport() {
  BatteryReading b;
  xSemaphoreTake(lock, portMAX_DELAY);
  b = batteryNow;
  xSemaphoreGive(lock);
  const String body = reportJson(WiFi.SSID().c_str(), store, firstColour, b.present, b.charging, b.percent, applied).c_str();
  if (body == lastReport && millis() - lastReportAt < 30000) return;
  HTTPClient http;
  http.setConnectTimeout(900);
  http.setTimeout(900);
  if (!http.begin("http://" + host + ":" + port + "/lanyard")) return;
  http.addHeader("content-type", "application/json");
  const int code = http.POST(body);
  http.end();
  if (code != 204) {
    Serial.printf("report failed (%d)\n", code);
    return;
  }
  lastReport = body;
  lastReportAt = millis();
  Serial.println("report sent");
}

static void netTask(void*) {
  Link link;
  Backoff backoff(1000, 30000);
  bool wasUp = false, haveHost = false, everPolled = false, everJoined = false;
  uint32_t nextJoin = 0, nextResolve = 0, nextPoll = 0;
  int fails = 0, attempt = 0;

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);  // we reconnect ourselves, with backoff
  loadStore();
  if (candidateCount() == 0) {
    Serial.println("wifi: no saved networks; starting setup");
    startSetup();
  }

  for (;;) {
    const uint32_t now = millis();
    if (setupRequested.exchange(false) && !setupOn) {
      Serial.println("setup: requested (BOOT held)");
      MDNS.end();
      startSetup();
    }
    if (setupOn) {
      down = true;
      if (setupTick()) {
        stopSetup();
        wasUp = false;  // treat as a fresh connection below
        everJoined = true;
      }
      vTaskDelay(pdMS_TO_TICKS(5));
      continue;
    }

    const bool up = WiFi.status() == WL_CONNECTED;
    if (up && !wasUp) {
      Serial.printf("wifi connected: %s ip %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
      backoff.reset();
      attempt = 0;
      everJoined = true;
      MDNS.begin("terminal-buddy");
      haveHost = false;
      nextResolve = now;
    } else if (!up && wasUp) {
      Serial.println("wifi lost");
      MDNS.end();
      nextJoin = now;
    }
    wasUp = up;
    link.setWifi(up);

    if (!up && candidateCount() > 0 && (int32_t)(now - nextJoin) >= 0) {
      // Before we've ever been online, a couple of failed rounds means these networks aren't around: ask.
      if (!everJoined && attempt >= ROUNDS_BEFORE_SETUP * candidateCount()) {
        Serial.println("wifi: none of the saved networks answered; starting setup");
        startSetup();
        continue;
      }
      const char *ssid, *pass;
      candidate(attempt % candidateCount(), &ssid, &pass);
      const uint32_t wait = attempt < candidateCount() ? 0 : backoff.next();
      Serial.printf("wifi connecting to %s (attempt %d)\n", ssid, attempt + 1);
      WiFi.disconnect();
      WiFi.begin(ssid, pass);
      attempt++;
      nextJoin = now + JOIN_MS + wait;
    }
    if (up && !haveHost && (int32_t)(now - nextResolve) >= 0) {
      haveHost = resolve();
      if (haveHost) lastReport = String();  // report right after (re)connecting
      nextResolve = now + 5000;
    }
    if (up && haveHost && (int32_t)(now - nextPoll) >= 0) {
      nextPoll = now + POLL_MS;
      if (poll()) {
        if (!everPolled || fails) Serial.printf("agentfunnel connected (since=%lu)\n", (unsigned long)since);
        everPolled = true;
        fails = 0;
        link.pollOk();
        sendReport();
      } else {
        link.pollFailed();
        if (++fails == Link::MAX_FAILS) {
          Serial.println("agentfunnel: 3 failed polls, looking it up again");
          haveHost = false;  // the Mac may have a new address
        }
      }
    }
    if (needSetup) {
      needSetup = false;
      Serial.println("wifi: the Mac deleted the last network; starting setup");
      MDNS.end();
      startSetup();
      continue;
    }
    if (needRejoin) {
      needRejoin = false;
      Serial.println("wifi: the Mac deleted the network we're on; joining another");
      WiFi.disconnect();
    }
    down = link.disconnected() || stopped;
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void netStart() {
  lock = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(netTask, "net", 12288, nullptr, 1, nullptr, 0);
}
