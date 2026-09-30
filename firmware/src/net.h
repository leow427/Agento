#pragma once
#include <Arduino.h>
#include "provision.h"

// Wi-Fi + AgentFunnel discovery + polling, on its own FreeRTOS task (core 0). With no usable
// saved network it runs first-time setup instead: its own access point plus a captive web page.
void netStart();
bool netTake(String& body);      // newest unapplied /state body, if any
void netSetSince(uint32_t seq);  // the model's seq after applying a response
bool netDisconnected();          // Wi-Fi down, never reached AgentFunnel, or 3 failed polls in a row
void netRequestSetup();          // start setup now (BOOT held)

struct SetupView {
  bool active = false;
  SetupStep step = SetupStep::Join;
  char apSsid[33] = "";
  char apPass[9] = "";
  char ssid[33] = "";  // the network being joined
};
void netSetupView(SetupView& out);
