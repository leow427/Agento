#define XPOWERS_CHIP_AXP2101
#include "battery.h"
#include <XPowersLib.h>
#include "board_pins.h"

static XPowersPMU pmu;
static bool ok = false;

bool batteryBegin() {
  ok = pmu.begin(Wire, AXP2101_SLAVE_ADDRESS, IIC_SDA, IIC_SCL);
  if (!ok) return false;
  pmu.enableBattDetection();
  pmu.enableBattVoltageMeasure();
  pmu.enableVbusVoltageMeasure();
  return true;
}

BatteryReading batteryRead() {
  BatteryReading r;
  if (!ok) return r;
  r.present = pmu.isBatteryConnect();
  r.charging = pmu.isCharging();
  r.percent = r.present ? pmu.getBatteryPercent() : 0;
  return r;
}
