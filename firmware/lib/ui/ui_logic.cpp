#include "ui_logic.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

const char* swipeName(Swipe s) {
  switch (s) {
    case Swipe::Left: return "left";
    case Swipe::Right: return "right";
    case Swipe::Up: return "up";
    case Swipe::Down: return "down";
    default: return "none";
  }
}

Swipe SwipeDetector::feed(bool down, int x, int y, uint32_t) {
  if (down) {
    if (!active_) {
      active_ = true;
      x0_ = x;
      y0_ = y;
    }
    x1_ = x;
    y1_ = y;
    return Swipe::None;
  }
  if (!active_) return Swipe::None;
  active_ = false;
  const int dx = x1_ - x0_, dy = y1_ - y0_;
  if (abs(dx) < MIN_PX && abs(dy) < MIN_PX) return Swipe::None;
  if (abs(dx) >= abs(dy)) return dx < 0 ? Swipe::Left : Swipe::Right;
  return dy < 0 ? Swipe::Up : Swipe::Down;
}

void toAscii(char* dst, size_t n, const char* src) {
  if (n == 0) return;
  size_t o = 0;
  auto put = [&](char c) {
    if (o + 1 < n) dst[o++] = c;
  };
  for (const uint8_t* p = (const uint8_t*)src; *p;) {
    if (*p < 0x80) {
      put(*p >= 0x20 ? (char)*p : ' ');
      p++;
      continue;
    }
    int len = (*p & 0xE0) == 0xC0 ? 2 : (*p & 0xF0) == 0xE0 ? 3 : (*p & 0xF8) == 0xF0 ? 4 : 1;
    if (len == 3 && p[0] == 0xE2 && p[1] == 0x80 && p[2] == 0xA6) {
      put('.'); put('.'); put('.');
    } else {
      put('?');
    }
    for (int i = 0; i < len && *p; i++) p++;
  }
  dst[o] = 0;
}

int wrapFromEnd(const char* text, int (*maxChars)(int, void*), void* ctx, WrapLine* out, int maxOut) {
  int end = (int)strlen(text);
  int n = 0;
  while (n < maxOut) {
    while (end > 0 && text[end - 1] == ' ') end--;
    if (end == 0) break;
    const int w = maxChars(n, ctx);
    if (w <= 0) break;
    int start = end - w < 0 ? 0 : end - w;
    if (start > 0 && text[start - 1] != ' ') {
      // Back up to a word boundary if there is one inside the window; otherwise hard-split.
      int b = start;
      while (b < end && text[b] != ' ') b++;
      if (b < end) start = b + 1;
    }
    while (start < end && text[start] == ' ') start++;
    out[n].start = (uint16_t)start;
    out[n].len = (uint16_t)(end - start);
    n++;
    end = start;
  }
  return n;
}

int charsAcross(int top, int lineH, int charW, int r, int margin) {
  const int d1 = abs(top - r), d2 = abs(top + lineH - r);
  const int d = d1 > d2 ? d1 : d2;
  if (d >= r) return 0;
  const float chord = 2.0f * sqrtf((float)(r * r - d * d)) - 2.0f * margin;
  return chord <= 0 ? 0 : (int)(chord / charW);
}

int stepSelection(int count, int cur, int dir, bool (*visible)(int, void*), void* ctx) {
  if (count <= 0) return -1;
  int i = cur;
  for (int k = 0; k < count; k++) {
    i = ((i + dir) % count + count) % count;
    if (visible(i, ctx)) return i;
  }
  return -1;
}

int colourRank(int i, bool (*visible)(int, void*), void* ctx) {
  int rank = 0;
  for (int k = 0; k < i; k++) rank += visible(k, ctx) ? 1 : 0;
  return rank;
}

int batterySegments(int percent) {
  if (percent <= 0) return 0;
  if (percent >= 100) return 4;
  return (percent + 24) / 25;
}
