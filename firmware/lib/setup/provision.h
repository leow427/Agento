#pragma once
// First-run Wi-Fi setup logic (no Arduino dependencies; unit-tested natively).
#include <stddef.h>
#include <stdint.h>
#include <string>

constexpr int MAX_NETWORKS = 3;

struct Cred {
  char ssid[33];
  char pass[64];
};

// Saved networks, newest first, at most MAX_NETWORKS, one entry per SSID.
class CredStore {
 public:
  void add(const char* ssid, const char* pass);
  bool remove(const char* ssid);  // true if it was there
  void clear() { n_ = 0; }
  int count() const { return n_; }
  const Cred& at(int i) const { return creds_[i]; }
  std::string serialize() const;        // JSON, for NVS
  bool deserialize(const char* text);   // false (and empty) on garbage; "" = nothing saved

 private:
  Cred creds_[MAX_NETWORKS] = {};
  int n_ = 0;
};

// nullptr if usable, else a short reason for the web page.
const char* validateCreds(const char* ssid, const char* pass);

// The text of a "join this Wi-Fi" QR code (the iPhone camera understands it).
void wifiQrPayload(char* out, size_t n, const char* ssid, const char* pass);

// "TerminalBuddy-XXXX" from the last two bytes of the MAC.
void apName(char* out, size_t n, const uint8_t mac[6]);

// 8 characters with no look-alikes (no 0/O, 1/l/I), for typing off the round screen. out needs 9 bytes.
void apPassword(char* out, uint32_t (*rng)());

enum class SetupStep : uint8_t { Join, Open, Connecting, Failed, Done };
const char* setupStepName(SetupStep s);

// What the round screen and the web page show during setup.
class SetupFlow {
 public:
  void clients(int n);              // phones connected to our access point
  void submit(const char* ssid);    // the page sent a network to join
  void joined(bool ok);             // result of that join
  SetupStep step() const { return step_; }
  const char* ssid() const { return ssid_; }
  std::string statusJson() const;   // {"step":"...","ssid":"..."} for GET /status

 private:
  SetupStep step_ = SetupStep::Join;
  char ssid_[33] = "";
};

// ---------- settings from the Mac's control page ----------
// The Mac queues Wi-Fi changes with increasing ids and sends them in GET /state; we apply each id
// once (the last applied id is kept in NVS) and say so in POST /lanyard.

enum class OpKind : uint8_t { Add, Delete, Invalid };
const char* opKindName(OpKind k);

struct WifiOp {
  uint32_t id;
  OpKind kind;
  char ssid[33];
  char pass[64];
};

constexpr int MAX_OPS = 8;  // per poll; the rest arrive on the next one

struct LanyardBits {
  bool running = true;  // false: AgentFunnel is switched off on the Mac
  int colour = -1;      // the first character colour 0-5, or -1 if absent
  WifiOp ops[MAX_OPS] = {};
  int opCount = 0;
};

// Reads running/config/wifi from a /state body (everything else is skipped). False on bad JSON.
bool parseLanyardBits(const char* json, size_t len, LanyardBits& out);

struct OpsResult {
  bool changed = false;         // the saved networks changed (save them)
  bool currentRemoved = false;  // the network we're on was deleted
};
// Applies ops with id > applied, in order, and moves `applied` past each one (invalid ones too).
OpsResult applyWifiOps(CredStore& store, const WifiOp* ops, int n, uint32_t& applied, const char* currentSsid);

// Body of POST /lanyard. Network names only, never passwords.
std::string reportJson(const char* wifi, const CredStore& store, int colour, bool batteryPresent, bool charging,
                       int percent, uint32_t applied);
