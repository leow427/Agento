#include "net.h"
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <atomic>
#include "link.h"

#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef WIFI_SSID
#define WIFI_SSID ""
#define WIFI_PASS ""
#endif

static constexpr uint16_t DEFAULT_PORT = 8765;
static constexpr uint32_t POLL_MS = 1000;

static SemaphoreHandle_t lock;
static String mailbox;
static bool mailFull = false;
static std::atomic<uint32_t> since{0};  // 0 on boot: the Mac sends its full history
static std::atomic<bool> down{true};

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

static bool poll() {
  HTTPClient http;
  http.setConnectTimeout(900);
  http.setTimeout(900);
  const String url = "http://" + host + ":" + port + "/state?since=" + String((uint32_t)since);
  if (!http.begin(url)) return false;
  const int code = http.GET();
  if (code != 200) {
    Serial.printf("poll failed: %s (%d)\n", code < 0 ? HTTPClient::errorToString(code).c_str() : "http", code);
    http.end();
    return false;
  }
  String body = http.getString();
  http.end();
  xSemaphoreTake(lock, portMAX_DELAY);
  mailbox = std::move(body);  // an unapplied older body is superseded: `since` hasn't moved
  mailFull = true;
  xSemaphoreGive(lock);
  return true;
}

static void netTask(void*) {
  Link link;
  Backoff backoff(1000, 30000);
  bool wasUp = false, haveHost = false, everPolled = false;
  uint32_t nextJoin = 0, nextResolve = 0, nextPoll = 0;
  int fails = 0;
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);  // we reconnect ourselves, with backoff
  if (!*WIFI_SSID) Serial.println("wifi: no secrets.h (copy secrets.example.h); staying offline");

  for (;;) {
    const uint32_t now = millis();
    const bool up = WiFi.status() == WL_CONNECTED;
    if (up && !wasUp) {
      Serial.printf("wifi connected: %s ip %s\n", WIFI_SSID, WiFi.localIP().toString().c_str());
      backoff.reset();
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

    if (!up && *WIFI_SSID && (int32_t)(now - nextJoin) >= 0) {
      const uint32_t wait = backoff.next();
      Serial.printf("wifi connecting to %s (next retry in %lu ms)\n", WIFI_SSID, (unsigned long)(8000 + wait));
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASS);
      nextJoin = now + 8000 + wait;  // give the join ~8 s, then back off
    }
    if (up && !haveHost && (int32_t)(now - nextResolve) >= 0) {
      haveHost = resolve();
      nextResolve = now + 5000;
    }
    if (up && haveHost && (int32_t)(now - nextPoll) >= 0) {
      nextPoll = now + POLL_MS;
      if (poll()) {
        if (!everPolled || fails) Serial.printf("agentfunnel connected (since=%lu)\n", (unsigned long)since);
        everPolled = true;
        fails = 0;
        link.pollOk();
      } else {
        link.pollFailed();
        if (++fails == Link::MAX_FAILS) {
          Serial.println("agentfunnel: 3 failed polls, looking it up again");
          haveHost = false;  // the Mac may have a new address
        }
      }
    }
    down = link.disconnected();
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void netStart() {
  lock = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(netTask, "net", 8192, nullptr, 1, nullptr, 0);
}
