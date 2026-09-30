// Terminal Buddy wearable: mirrors AgentFunnel's Claude Code sessions as pixel-art characters.
// Core 0 runs Wi-Fi, mDNS discovery and the 1 s poll (net.cpp); core 1 (this loop) applies each
// response to the model, animates, reads touch and draws.
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <TouchDrvCSTXXX.hpp>
#include "board_pins.h"
#include "director.h"
#include "net.h"
#include "session_model.h"
#include "slots.h"
#include "ui_logic.h"

static constexpr int CELL = 16;
static constexpr int ORIGIN = 2;
static constexpr int R = LCD_WIDTH / 2;
static constexpr int CHAR_W = 12, CHAR_H = 16;  // built-in font at size 2
static constexpr int NAME_Y = 32;               // project name baseline row (above the '!')
static constexpr uint16_t CREAM = 0xFFDE, GREY = 0xB574;

Arduino_DataBus* bus = new Arduino_ESP32QSPI(LCD_CS, LCD_SCK, LCD_D0, LCD_D1, LCD_D2, LCD_D3);
Arduino_CO5300* gfx = new Arduino_CO5300(bus, LCD_RST, 0, LCD_WIDTH, LCD_HEIGHT, LCD_COL_OFFSET1, 0, 0, 0);
TouchDrvCST92xx touch;
static bool touchOk = false;
static volatile bool touchIrq = false;

static Model model;
static Director directors[MAX_SESSIONS];  // indexed by slot
static Director nobody;                    // the orange idle shown when there are no sessions
static Player disconnectedPlayer;
static SwipeDetector swipes;

enum class Screen : uint8_t { Main, Messages, Disconnected };
static const char* screenName(Screen s) {
  return s == Screen::Main ? "main" : s == Screen::Messages ? "messages" : "disconnected";
}
static Screen screen = Screen::Disconnected;
static bool wantMessages = false;  // user swiped up (kept while disconnected)
static char selectedId[ID_BYTES] = "";
static bool redrawAll = true;

static uint8_t shown[GRID * GRID];  // palette index on screen; 0 = black
static int shownSlot = -1;

// ---------- drawing ----------

// Paint only changed cells, merging horizontal runs of the same colour into one fillRect.
static void drawFrame(uint16_t f, uint8_t slot) {
  const uint8_t* px = FRAMES[f];
  for (int y = 0; y < GRID; y++) {
    int x = 0;
    while (x < GRID) {
      const int i = y * GRID + x;
      if (px[i] == shown[i]) { x++; continue; }
      int end = x + 1;
      while (end < GRID && px[y * GRID + end] == px[i] && px[y * GRID + end] != shown[y * GRID + end]) end++;
      for (int k = x; k < end; k++) shown[y * GRID + k] = px[i];
      gfx->fillRect(ORIGIN + x * CELL, ORIGIN + y * CELL, (end - x) * CELL, CELL, slotColour(px[i], slot));
      x = end;
    }
  }
}

static void clearScreen() {
  gfx->fillScreen(0x0000);
  memset(shown, 0, sizeof shown);
}

// One centred line of text, cut to what fits across the circle at that height.
static void drawCentred(const char* text, int top, uint16_t colour) {
  char buf[64];
  toAscii(buf, sizeof buf, text);
  const int fit = charsAcross(top, CHAR_H, CHAR_W, R, 12);
  if ((int)strlen(buf) > fit) {
    if (fit < 2) return;
    buf[fit - 1] = '~';
    buf[fit] = 0;
  }
  gfx->setTextSize(2);
  gfx->setTextColor(colour);
  gfx->setCursor(R - (int)strlen(buf) * CHAR_W / 2, top);
  gfx->print(buf);
}

struct WrapCtx { int bottom; int lineH; };
static int widthAt(int k, void* ctx) {
  const WrapCtx* c = (const WrapCtx*)ctx;
  return charsAcross(c->bottom - (k + 1) * c->lineH, CHAR_H, CHAR_W, R, 14);
}

// Messages page: project name on top, messages newest at the bottom, wrapped to the circle.
static void drawMessages(const Session& s) {
  clearScreen();
  const uint16_t colour = SLOT_BODY565[s.slot];
  drawCentred(s.project[0] ? s.project : "session", 34, colour);
  gfx->drawFastHLine(R - 70, 56, 140, SLOT_SHADOW565[s.slot]);
  const int lineH = 19, topLimit = 64;
  int bottom = LCD_HEIGHT - 36;
  if (s.msgCount == 0) {
    drawCentred("no messages yet", R - 8, GREY);
    return;
  }
  static char text[MSG_BYTES];
  static WrapLine lines[24];
  for (int m = s.msgCount - 1; m >= 0 && bottom - lineH >= topLimit; m--) {
    toAscii(text, sizeof text, s.msg(m).text);
    WrapCtx ctx{bottom, lineH};
    const int n = wrapFromEnd(text, widthAt, &ctx, lines, 24);
    const uint16_t c = strncmp(text, "> ", 2) == 0 ? colour : CREAM;
    for (int k = 0; k < n; k++) {
      const int top = bottom - (k + 1) * lineH;
      if (top < topLimit) break;
      char line[64];
      const int len = lines[k].len < 63 ? lines[k].len : 63;
      memcpy(line, text + lines[k].start, len);
      line[len] = 0;
      gfx->setTextSize(2);
      gfx->setTextColor(c);
      gfx->setCursor(R - len * CHAR_W / 2, top);
      gfx->print(line);
    }
    bottom -= n * lineH + 8;
  }
}

// ---------- sessions ----------

static bool visibleAt(int i, void*) { return !directors[model.at(i).slot].gone(); }

static int selectedIndex() {
  const int i = selectedId[0] ? model.find(selectedId) : -1;
  return i >= 0 && visibleAt(i, nullptr) ? i : -1;
}

static void select(int i) {
  const char* id = i >= 0 ? model.at(i).id : "";
  if (strcmp(id, selectedId) == 0) return;
  strlcpy(selectedId, id, sizeof selectedId);
  if (i >= 0) Serial.printf("select session %s slot %u (%s)\n", model.at(i).project, model.at(i).slot, stateName(model.at(i).state));
  else Serial.println("select none: showing idle buddy");
  redrawAll = true;
}

static void applyResponse(String& body, uint32_t now) {
  if (!model.apply(body.c_str(), body.length())) {
    Serial.println("agentfunnel: bad /state response");
    return;
  }
  netSetSince(model.seq());
  for (int c = 0; c < model.changeCount(); c++) {
    const Change& ch = model.change(c);
    const int i = model.find(ch.id);
    const char* project = i >= 0 ? model.at(i).project : "";
    switch (ch.kind) {
      case Change::Appeared:
        directors[ch.slot].appear(ch.to, now);
        Serial.printf("session appeared: %s slot %u state %s\n", project, ch.slot, stateName(ch.to));
        break;
      case Change::Removed:
        directors[ch.slot].appear(SState::Ended, now);  // slot is free: park its director as gone
        Serial.printf("session removed: slot %u\n", ch.slot);
        break;
      case Change::StateChanged:
        directors[ch.slot].setState(ch.to);
        Serial.printf("session state: %s slot %u %s -> %s\n", project, ch.slot, stateName(ch.from), stateName(ch.to));
        break;
    }
  }
}

// ---------- touch ----------

static void initTouch() {
  pinMode(TP_RST, OUTPUT);
  digitalWrite(TP_RST, LOW);
  delay(30);
  digitalWrite(TP_RST, HIGH);
  delay(50);
  touch.setPins(TP_RST, TP_INT);
  touchOk = touch.begin(Wire, TP_ADDR, IIC_SDA, IIC_SCL);
  if (!touchOk) {
    Serial.println("touch not found");
    return;
  }
  touch.setMaxCoordinates(LCD_WIDTH, LCD_HEIGHT);
  touch.setMirrorXY(true, true);
  pinMode(TP_INT, INPUT_PULLUP);
  attachInterrupt(TP_INT, [] { touchIrq = true; }, FALLING);
  Serial.printf("touch ready: %s\n", touch.getModelName());
}

// The controller pulses INT while a finger is down; a quiet INT line means the finger lifted.
static Swipe pollTouch(uint32_t now) {
  static uint32_t lastIrq = 0, lastRead = 0;
  static bool down = false;
  if (!touchOk) return Swipe::None;
  if (touchIrq) {
    touchIrq = false;
    lastIrq = now;
    if (now - lastRead >= 10) {
      lastRead = now;
      int16_t x[2], y[2];
      if (touch.getPoint(x, y, 1)) {
        down = true;
        return swipes.feed(true, x[0], y[0], now);
      }
    }
  }
  if (down && now - lastIrq > 80) {
    down = false;
    return swipes.feed(false, 0, 0, now);
  }
  return Swipe::None;
}

// ---------- main ----------

void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);  // never stall the animation when no host reads USB serial
  boardPowerUp();
  if (!gfx->begin()) {
    Serial.println("gfx->begin() failed");
    while (true) delay(1000);
  }
  gfx->setBrightness(128);
  clearScreen();
  Serial.println("display ready");
  initTouch();
  const uint32_t now = millis();
  nobody.appear(SState::YourTurn, now);
  for (Director& d : directors) d.appear(SState::Ended, now);
  disconnectedPlayer.setLoopForever(true);
  disconnectedPlayer.play(ANIM_DISCONNECTED, now);
  netStart();
}

void loop() {
  const uint32_t now = millis();

  String body;
  if (netTake(body)) applyResponse(body, now);

  const Swipe sw = pollTouch(now);
  if (sw != Swipe::None) {
    Serial.printf("swipe %s\n", swipeName(sw));
    if ((sw == Swipe::Left || sw == Swipe::Right) && screen == Screen::Main) {
      // left = previous session, right = next
      const int i = stepSelection(model.count(), selectedIndex(), sw == Swipe::Right ? +1 : -1, visibleAt, nullptr);
      if (i >= 0) select(i);
    } else if (sw == Swipe::Up) {
      wantMessages = true;
    } else if (sw == Swipe::Down) {
      wantMessages = false;
    }
  }

  // Every director keeps time, so a session is in the right pose when it's swiped to.
  uint16_t frames[MAX_SESSIONS];
  for (int s = 0; s < MAX_SESSIONS; s++) frames[s] = directors[s].tick(now);
  const uint16_t nobodyFrame = nobody.tick(now);

  if (selectedIndex() < 0) select(stepSelection(model.count(), -1, +1, visibleAt, nullptr));
  const int sel = selectedIndex();

  const Screen want = netDisconnected() ? Screen::Disconnected
                      : (wantMessages && sel >= 0) ? Screen::Messages
                                                   : Screen::Main;
  if (want != screen) {
    Serial.printf("screen: %s -> %s\n", screenName(screen), screenName(want));
    screen = want;
    redrawAll = true;
  }

  static uint32_t shownMsgSeq = 0;
  switch (screen) {
    case Screen::Disconnected:
      if (redrawAll) clearScreen();
      drawFrame(disconnectedPlayer.tick(now), 0);
      break;
    case Screen::Messages: {
      const Session& s = model.at(sel);
      const uint32_t newest = s.msgCount ? s.msg(s.msgCount - 1).seq : 0;
      if (redrawAll || newest != shownMsgSeq) {
        drawMessages(s);
        shownMsgSeq = newest;
      }
      break;
    }
    case Screen::Main: {
      const uint8_t slot = sel >= 0 ? model.at(sel).slot : 0;
      if (redrawAll || shownSlot != slot) {
        clearScreen();
        if (sel >= 0) drawCentred(model.at(sel).project, NAME_Y, SLOT_BODY565[slot]);
        shownSlot = slot;
      }
      drawFrame(sel >= 0 ? frames[slot] : nobodyFrame, slot);
      break;
    }
  }
  redrawAll = false;

  static uint32_t lastStatus = 0;
  if (now - lastStatus >= 5000) {
    lastStatus = now;
    Serial.printf("status: screen=%s sessions=%d seq=%lu selected=%s link=%s touch=%s heap=%lu\n", screenName(screen),
                  model.count(), (unsigned long)model.seq(), sel >= 0 ? model.at(sel).project : "-",
                  netDisconnected() ? "down" : "up", touchOk ? "ok" : "none", (unsigned long)ESP.getFreeHeap());
  }
}
