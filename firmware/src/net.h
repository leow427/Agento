#pragma once
#include <Arduino.h>
#include "battery.h"
#include "provision.h"

// Wi-Fi + AgentFunnel discovery + polling, on its own FreeRTOS task (core 0). With no usable
// saved network it runs first-time setup instead: its own access point plus a captive web page.
void netStart();
bool netTake(String& body);      // newest unapplied /state body, if any
void netSetSince(uint32_t seq);  // the model's seq after applying a response
bool netDisconnected();          // Wi-Fi down, never reached AgentFunnel, 3 failed polls in a row, or switched off on the Mac
void netRequestSetup();          // start setup now (BOOT held)
uint8_t netFirstColour();                     // first character colour 0-5, from the Mac's page (NVS)
void netBrightness(int& maxPercent, int& sleepPercent);  // awake / dimmed brightness in percent (NVS)
void netSetBattery(const BatteryReading& b);  // latest reading, for the report to the Mac

struct SetupView {
  bool active = false;
  SetupStep step = SetupStep::Join;
  char apSsid[33] = "";
  char apPass[9] = "";
  char ssid[33] = "";  // the network being joined
};
void netSetupView(SetupView& out);
