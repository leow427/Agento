#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "board_pins.h"

Arduino_DataBus* bus = new Arduino_ESP32QSPI(LCD_CS, LCD_SCK, LCD_D0, LCD_D1, LCD_D2, LCD_D3);
Arduino_GFX* gfx = new Arduino_CO5300(bus, LCD_RST, 0, LCD_WIDTH, LCD_HEIGHT, LCD_COL_OFFSET1, 0, 0, 0);

void setup() {
  Serial.begin(115200);
  boardPowerUp();
  if (!gfx->begin()) {
    Serial.println("gfx->begin() failed");
    while (true) delay(1000);
  }
  gfx->fillScreen(0x0000);
  ((Arduino_CO5300*)gfx)->setBrightness(128);
  // Corner squares make clipping by the round bezel visible; the centre square proves placement.
  gfx->fillRect(0, 0, 40, 40, 0xF800);
  gfx->fillRect(LCD_WIDTH - 40, 0, 40, 40, 0x07E0);
  gfx->fillRect(0, LCD_HEIGHT - 40, 40, 40, 0x001F);
  gfx->fillRect(LCD_WIDTH - 40, LCD_HEIGHT - 40, 40, 40, 0xFFFF);
  gfx->fillRect(LCD_WIDTH / 2 - 16, LCD_HEIGHT / 2 - 16, 32, 32, 0xDBAA);
  Serial.println("bring-up ok");
}

void loop() {
  delay(2000);
  Serial.println("display ready");
}
