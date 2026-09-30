#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "board_pins.h"
#include "player.h"

static constexpr int CELL = 16;
static constexpr int ORIGIN = 2;
static constexpr uint8_t BUTTON_PIN = 0;
static constexpr uint32_t DEBOUNCE_MS = 250;

Arduino_DataBus* bus = new Arduino_ESP32QSPI(LCD_CS, LCD_SCK, LCD_D0, LCD_D1, LCD_D2, LCD_D3);
Arduino_CO5300* gfx = new Arduino_CO5300(bus, LCD_RST, 0, LCD_WIDTH, LCD_HEIGHT, LCD_COL_OFFSET1, 0, 0, 0);

static Player player;
static uint8_t shown[GRID * GRID];  // palette index currently on screen; 0xFF = unknown
static bool buttonWasDown = false;
static uint32_t lastPress = 0;

// Paint only changed cells, merging horizontal runs of the same colour into one fillRect.
static void drawFrame(uint16_t f) {
  const uint8_t* px = FRAMES[f];
  for (int y = 0; y < GRID; y++) {
    int x = 0;
    while (x < GRID) {
      const int i = y * GRID + x;
      if (px[i] == shown[i]) { x++; continue; }
      int end = x + 1;
      while (end < GRID && px[y * GRID + end] == px[i] && px[y * GRID + end] != shown[y * GRID + end]) end++;
      for (int k = x; k < end; k++) shown[y * GRID + k] = px[i];
      gfx->fillRect(ORIGIN + x * CELL, ORIGIN + y * CELL, (end - x) * CELL, CELL, PALETTE565[px[i]]);
      x = end;
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  boardPowerUp();
  if (!gfx->begin()) {
    Serial.println("gfx->begin() failed");
    while (true) delay(1000);
  }
  gfx->fillScreen(0x0000);
  gfx->setBrightness(128);
  memset(shown, 0, sizeof(shown));  // screen is black == palette index 0
  player = Player(millis());
  Serial.println("display ready");
}

void loop() {
  const uint32_t now = millis();
  // One toggle per physical press: act on the down edge only, with a lockout for bounce.
  const bool down = digitalRead(BUTTON_PIN) == LOW;
  if (down && !buttonWasDown && now - lastPress > DEBOUNCE_MS) {
    player.toggle(now);
    lastPress = now;
    Serial.println("button toggle");
  }
  buttonWasDown = down;

  static uint16_t lastFrame = 0xFFFF;
  static uint8_t lastAnim = 0xFF;
  const uint16_t f = player.tick(now);
  if (player.anim() != lastAnim) {
    lastAnim = player.anim();
    Serial.printf("anim %u\n", lastAnim);
  }
  if (f != lastFrame) {
    const uint32_t t0 = micros();
    drawFrame(f);
    Serial.printf("frame %u drawn in %lu us\n", f, (unsigned long)(micros() - t0));
    lastFrame = f;
  }
}
