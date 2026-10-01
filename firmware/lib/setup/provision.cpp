#include "provision.h"
#include <stdio.h>
#include <string.h>
#include <ArduinoJson.h>

static void copy(char* dst, const char* src, size_t n) {
  strncpy(dst, src ? src : "", n - 1);
  dst[n - 1] = 0;
}

void CredStore::add(const char* ssid, const char* pass) {
  Cred c;
  copy(c.ssid, ssid, sizeof c.ssid);
  copy(c.pass, pass, sizeof c.pass);
  int keep = 0;
  Cred rest[MAX_NETWORKS];
  for (int i = 0; i < n_; i++)
    if (strcmp(creds_[i].ssid, c.ssid) != 0 && keep < MAX_NETWORKS - 1) rest[keep++] = creds_[i];
  creds_[0] = c;
  for (int i = 0; i < keep; i++) creds_[i + 1] = rest[i];
  n_ = keep + 1;
}

bool CredStore::remove(const char* ssid) {
  for (int i = 0; i < n_; i++) {
    if (strcmp(creds_[i].ssid, ssid) != 0) continue;
    for (int j = i; j + 1 < n_; j++) creds_[j] = creds_[j + 1];
    n_--;
    return true;
  }
  return false;
}

std::string CredStore::serialize() const {
  JsonDocument doc;
  JsonArray a = doc.to<JsonArray>();
  for (int i = 0; i < n_; i++) {
    JsonObject o = a.add<JsonObject>();
    o["ssid"] = creds_[i].ssid;
    o["pass"] = creds_[i].pass;
  }
  std::string out;
  serializeJson(doc, out);
  return out;
}

bool CredStore::deserialize(const char* text) {
  n_ = 0;
  if (!text || !*text) return true;
  JsonDocument doc;
  if (deserializeJson(doc, text) || !doc.is<JsonArray>()) return false;
  JsonArrayConst a = doc.as<JsonArrayConst>();
  // Oldest last in the file; add() puts each new one first, so walk backwards.
  for (int i = (int)a.size() - 1; i >= 0; i--) {
    const char* s = a[i]["ssid"] | "";
    if (*s) add(s, a[i]["pass"] | "");
  }
  return true;
}

const char* validateCreds(const char* ssid, const char* pass) {
  const size_t s = ssid ? strlen(ssid) : 0, p = pass ? strlen(pass) : 0;
  if (s == 0) return "pick a network";
  if (s > 32) return "network name is too long";
  if (p != 0 && p < 8) return "Wi-Fi passwords are at least 8 characters";
  if (p > 63) return "password is too long";
  return nullptr;
}

static void appendEscaped(std::string& out, const char* s) {
  for (; *s; s++) {
    if (strchr("\\;,:\"", *s)) out += '\\';
    out += *s;
  }
}

void wifiQrPayload(char* out, size_t n, const char* ssid, const char* pass) {
  std::string t = "WIFI:T:WPA;S:";
  appendEscaped(t, ssid);
  t += ";P:";
  appendEscaped(t, pass);
  t += ";;";
  copy(out, t.c_str(), n);
}

void apName(char* out, size_t n, const uint8_t mac[6]) { snprintf(out, n, "TerminalBuddy-%02X%02X", mac[4], mac[5]); }

void apPassword(char* out, uint32_t (*rng)()) {
  static const char ALPHABET[] = "abcdefghjkmnpqrstuvwxyz23456789";
  for (int i = 0; i < 8; i++) out[i] = ALPHABET[rng() % (sizeof ALPHABET - 1)];
  out[8] = 0;
}

const char* setupStepName(SetupStep s) {
  switch (s) {
    case SetupStep::Open: return "open";
    case SetupStep::Connecting: return "connecting";
    case SetupStep::Failed: return "failed";
    case SetupStep::Done: return "done";
    default: return "join";
  }
}

void SetupFlow::clients(int n) {
  if (step_ == SetupStep::Join && n > 0) step_ = SetupStep::Open;
  else if (step_ == SetupStep::Open && n == 0) step_ = SetupStep::Join;
}

void SetupFlow::submit(const char* ssid) {
  copy(ssid_, ssid, sizeof ssid_);
  step_ = SetupStep::Connecting;
}

void SetupFlow::joined(bool ok) {
  if (step_ == SetupStep::Connecting) step_ = ok ? SetupStep::Done : SetupStep::Failed;
}

std::string SetupFlow::statusJson() const {
  JsonDocument doc;
  doc["step"] = setupStepName(step_);
  doc["ssid"] = ssid_;
  std::string out;
  serializeJson(doc, out);
  return out;
}

// ---------- settings from the Mac's control page ----------

static constexpr int COLOUR_COUNT = 6;

const char* opKindName(OpKind k) {
  return k == OpKind::Add ? "add" : k == OpKind::Delete ? "delete" : "invalid";
}

// Copies src into dst (size n); false if it doesn't fit (so a long name is never silently cut).
static bool copyWhole(char* dst, const char* src, size_t n) {
  if (strlen(src) >= n) {
    dst[0] = 0;
    return false;
  }
  strcpy(dst, src);
  return true;
}

bool parseLanyardBits(const char* json, size_t len, LanyardBits& out) {
  out = LanyardBits();
  JsonDocument filter;
  filter["running"] = true;
  filter["config"]["colour"] = true;
  filter["config"]["maxBrightness"] = true;
  filter["config"]["sleepBrightness"] = true;
  JsonObject f = filter["wifi"].add<JsonObject>();
  f["id"] = true;
  f["op"] = true;
  f["ssid"] = true;
  f["pass"] = true;
  JsonDocument doc;
  if (deserializeJson(doc, json, len, DeserializationOption::Filter(filter)) || !doc.is<JsonObject>()) return false;
  out.running = doc["running"] | true;
  const int c = doc["config"]["colour"] | -1;
  out.colour = c >= 0 && c < COLOUR_COUNT ? c : -1;
  const int bm = doc["config"]["maxBrightness"] | -1, bs = doc["config"]["sleepBrightness"] | -1;
  if (bm >= 5 && bm <= 100 && bs >= 5 && bs <= bm) {
    out.maxBrightness = bm;
    out.sleepBrightness = bs;
  }
  for (JsonObjectConst o : doc["wifi"].as<JsonArrayConst>()) {
    if (out.opCount >= MAX_OPS) break;
    WifiOp& w = out.ops[out.opCount++];
    w.id = o["id"] | 0u;
    const char* k = o["op"] | "";
    w.kind = !strcmp(k, "add") ? OpKind::Add : !strcmp(k, "delete") ? OpKind::Delete : OpKind::Invalid;
    const bool fits = copyWhole(w.ssid, o["ssid"] | "", sizeof w.ssid) & copyWhole(w.pass, o["pass"] | "", sizeof w.pass);
    if (!fits) w.kind = OpKind::Invalid;
  }
  return true;
}

OpsResult applyWifiOps(CredStore& store, const WifiOp* ops, int n, uint32_t& applied, const char* currentSsid) {
  OpsResult r;
  for (int i = 0; i < n; i++) {
    const WifiOp& o = ops[i];
    if (o.id <= applied) continue;
    applied = o.id;
    if (o.kind == OpKind::Add && validateCreds(o.ssid, o.pass) == nullptr) {
      store.add(o.ssid, o.pass);
      r.changed = true;
    } else if (o.kind == OpKind::Delete && *o.ssid && store.remove(o.ssid)) {
      r.changed = true;
      if (currentSsid && !strcmp(currentSsid, o.ssid)) r.currentRemoved = true;
    }
  }
  return r;
}

std::string reportJson(const char* wifi, const CredStore& store, int colour, bool batteryPresent, bool charging,
                       int percent, uint32_t applied, int maxBrightness, int sleepBrightness) {
  JsonDocument doc;
  doc["wifi"] = wifi ? wifi : "";
  JsonArray nets = doc["nets"].to<JsonArray>();
  for (int i = 0; i < store.count(); i++) nets.add(store.at(i).ssid);
  doc["colour"] = colour;
  JsonObject b = doc["battery"].to<JsonObject>();
  b["present"] = batteryPresent;
  b["charging"] = charging;
  b["percent"] = percent;
  doc["applied"] = applied;
  JsonObject br = doc["brightness"].to<JsonObject>();
  br["max"] = maxBrightness;
  br["sleep"] = sleepBrightness;
  std::string out;
  serializeJson(doc, out);
  return out;
}
