#pragma once
#include <Arduino.h>

// Wi-Fi + AgentFunnel discovery + polling, on its own FreeRTOS task (core 0).
void netStart();
bool netTake(String& body);      // newest unapplied /state body, if any
void netSetSince(uint32_t seq);  // the model's seq after applying a response
bool netDisconnected();          // Wi-Fi down, never reached AgentFunnel, or 3 failed polls in a row
