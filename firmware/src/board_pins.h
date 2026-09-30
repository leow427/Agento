#pragma once
// Waveshare ESP32-S3-Touch-AMOLED-1.75C. Copied from the official example:
//   github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75C @ 6d19f7e16fb9a3be219e9eed43ca9eb56c88d01c
//   examples/arduino/libraries/Mylibrary/pin_config.h (pins)
//   examples/arduino/examples/01_HelloWorld/01_HelloWorld.ino (power-up, CO5300 constructor)
//   examples/arduino/examples/05_LVGL_Widgets/13_LVGL_Widgets.ino (CST9217 touch setup)
#include <Arduino.h>
#include <Wire.h>

constexpr int LCD_WIDTH = 466;
constexpr int LCD_HEIGHT = 466;
constexpr int LCD_CS = 12;
constexpr int LCD_SCK = 38;
constexpr int LCD_D0 = 4;
constexpr int LCD_D1 = 5;
constexpr int LCD_D2 = 6;
constexpr int LCD_D3 = 7;
constexpr int LCD_RST = 1;
// Arduino_CO5300(bus, rst, rotation, w, h, col_offset1, row_offset1, col_offset2, row_offset2)
constexpr int LCD_COL_OFFSET1 = 6;

constexpr int IIC_SDA = 15;
constexpr int IIC_SCL = 14;
constexpr int TP_INT = 11;
constexpr int TP_RST = 2;
constexpr uint8_t TP_ADDR = 0x5A;

// The official HelloWorld starts I2C (shared by the AXP2101 PMIC and touch) before gfx->begin().
// The display needs no IO expander or PMIC writes on this board.
inline void boardPowerUp() { Wire.begin(IIC_SDA, IIC_SCL); }
