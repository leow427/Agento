#pragma once
#include <stdint.h>
#include "frames_data.h"

// One colour per session slot. At render time the body (palette 1) and shadow (palette 2)
// entries are swapped for the slot's pair; everything else keeps the art palette.
constexpr uint16_t rgb565(uint32_t rgb) {
  return (uint16_t)(((rgb >> 8) & 0xF800) | ((rgb >> 5) & 0x07E0) | ((rgb & 0xFF) >> 3));
}

constexpr int SLOT_COUNT = 6;
constexpr uint8_t PAL_BODY = 1, PAL_SHADOW = 2;
constexpr uint32_t SLOT_BODY_RGB[SLOT_COUNT] = {0xD97757, 0x5B8DEF, 0x6BBF59, 0xA67CE0, 0xE8C547, 0xE0709B};
constexpr uint32_t SLOT_SHADOW_RGB[SLOT_COUNT] = {0xB05A3F, 0x3F66B0, 0x4A8F3C, 0x7A55B0, 0xB89A2E, 0xB04F76};
constexpr uint16_t SLOT_BODY565[SLOT_COUNT] = {
    rgb565(SLOT_BODY_RGB[0]), rgb565(SLOT_BODY_RGB[1]), rgb565(SLOT_BODY_RGB[2]),
    rgb565(SLOT_BODY_RGB[3]), rgb565(SLOT_BODY_RGB[4]), rgb565(SLOT_BODY_RGB[5])};
constexpr uint16_t SLOT_SHADOW565[SLOT_COUNT] = {
    rgb565(SLOT_SHADOW_RGB[0]), rgb565(SLOT_SHADOW_RGB[1]), rgb565(SLOT_SHADOW_RGB[2]),
    rgb565(SLOT_SHADOW_RGB[3]), rgb565(SLOT_SHADOW_RGB[4]), rgb565(SLOT_SHADOW_RGB[5])};

inline uint32_t slotRgb(uint8_t paletteIndex, uint8_t slot) {
  if (slot >= SLOT_COUNT) slot = 0;
  if (paletteIndex == PAL_BODY) return SLOT_BODY_RGB[slot];
  if (paletteIndex == PAL_SHADOW) return SLOT_SHADOW_RGB[slot];
  return PALETTE_RGB[paletteIndex];
}

inline uint16_t slotColour(uint8_t paletteIndex, uint8_t slot) {
  if (slot >= SLOT_COUNT) slot = 0;
  if (paletteIndex == 1) return SLOT_BODY565[slot];
  if (paletteIndex == 2) return SLOT_SHADOW565[slot];
  return PALETTE565[paletteIndex];
}

// Colour for the session at visible rank `rank` when `first` is the chosen first colour (set on
// the Mac's page): `first`, then the other five in their usual order.
inline uint8_t colourOrder(uint8_t first, int rank) {
  if (first >= SLOT_COUNT) first = 0;
  const int r = rank % SLOT_COUNT;
  if (r == 0) return first;
  return (uint8_t)(r - 1 < first ? r - 1 : r);
}
