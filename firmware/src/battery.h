#pragma once
// Battery level from the AXP2101 PMIC (setup as in Waveshare's 03_LVGL_AXP2101_ADC_Data example).
struct BatteryReading {
  bool present = false;  // a battery is connected
  bool charging = false;
  int percent = 0;
};
bool batteryBegin();             // after Wire.begin(); false if the PMIC doesn't answer
BatteryReading batteryRead();
