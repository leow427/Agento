#pragma once
// Screen-independent UI logic (unit-tested natively): swipes, text wrapping on a round screen,
// session navigation.
#include <stddef.h>
#include <stdint.h>

enum class Swipe : uint8_t { None, Left, Right, Up, Down };
const char* swipeName(Swipe s);

// Feed one touch sample per poll; returns a swipe once, when the finger lifts.
class SwipeDetector {
 public:
  static constexpr int MIN_PX = 60;
  Swipe feed(bool down, int x, int y, uint32_t nowMs);

 private:
  bool active_ = false;
  int x0_ = 0, y0_ = 0, x1_ = 0, y1_ = 0;
};

// Copies UTF-8 `src` into `dst` as printable ASCII ("…" -> "...", other non-ASCII -> "?").
void toAscii(char* dst, size_t n, const char* src);

struct WrapLine {
  uint16_t start, len;
};
// Word-wraps `text` from its end: line k (0 = bottom) holds at most maxChars(k, ctx) characters.
// Returns the number of lines written to out (bottom line first).
int wrapFromEnd(const char* text, int (*maxChars)(int k, void* ctx), void* ctx, WrapLine* out, int maxOut);

// Characters of width charW that fit on a line spanning [top, top+lineH) of a circle of radius r
// (centre at y = r), leaving `margin` px at each end.
int charsAcross(int top, int lineH, int charW, int r, int margin);

// Next visible index after `cur` in direction dir (+1/-1), wrapping; -1 if none. cur may be -1.
int stepSelection(int count, int cur, int dir, bool (*visible)(int i, void* ctx), void* ctx);

// Rank of the session at index i among visible sessions (0 = first on the lanyard). colourOrder()
// in slots.h turns the rank into a colour.
int colourRank(int i, bool (*visible)(int i, void* ctx), void* ctx);

// Battery icon: width of the charge fill inside an interior `width` px wide, and whether it's low.
int batteryFillPx(int percent, int width);
inline bool batteryLow(int percent) { return percent <= 15; }

// Dims the panel after a stretch with no touch; any touch brings it back.
class DimTimer {
 public:
  static constexpr uint32_t DIM_AFTER_MS = 30000;
  static constexpr uint8_t FULL = 255;  // 100% of the panel's range (the default awake level)
  static constexpr uint8_t DIM = 51;    // 20% (the default dimmed level)
  // Panel level (0-255) for a percentage, rounded: 100 -> 255, 20 -> 51.
  static uint8_t levelFromPercent(int percent);
  // Awake and dimmed levels in percent (the Mac's page sets them; each 5-100, dimmed <= awake).
  // Anything else is ignored. Returns true if the levels changed.
  bool setPercents(int maxPercent, int sleepPercent);
  // A touch (or anything that should count as one) restarts the countdown.
  // Returns true if the screen was dimmed, i.e. this touch woke it.
  bool touch(uint32_t nowMs);
  bool dimmed(uint32_t nowMs) const { return nowMs - lastMs_ >= DIM_AFTER_MS; }
  uint8_t brightness(uint32_t nowMs) const { return dimmed(nowMs) ? dim_ : full_; }

 private:
  uint32_t lastMs_ = 0;  // boot counts as activity
  uint8_t full_ = FULL, dim_ = DIM;
};
