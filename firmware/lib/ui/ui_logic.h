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
