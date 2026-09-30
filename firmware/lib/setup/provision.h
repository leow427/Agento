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
