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
