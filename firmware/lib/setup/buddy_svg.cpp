#include "buddy_svg.h"
#include <stdio.h>
#include "frames_data.h"
#include "slots.h"

std::string buddySvg(uint16_t frame, uint8_t slot) {
  const uint8_t* px = FRAMES[frame];
  std::string out = "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 29 29\" shape-rendering=\"crispEdges\">";
  char buf[96];
  for (int y = 0; y < GRID; y++) {
    int x = 0;
    while (x < GRID) {
      const uint8_t c = px[y * GRID + x];
      int end = x + 1;
      while (end < GRID && px[y * GRID + end] == c) end++;
      if (c) {
        snprintf(buf, sizeof buf, "<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"1\" fill=\"#%06lx\"/>", x, y, end - x,
                 (unsigned long)slotRgb(c, slot));
        out += buf;
      }
      x = end;
    }
  }
  return out + "</svg>";
}
