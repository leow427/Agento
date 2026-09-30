#include <unity.h>
#include <math.h>
#include "frames_data.h"
#include "slots.h"

// Palette indexes (PALETTE_ORDER ".ODKCSLPE" in art/palette.py).
enum { P_NONE = 0, P_BODY = 1, P_SHADOW = 2, P_INK = 3, P_CREAM = 4, P_SLATE = 5, P_LIGHT = 6 };

void setUp() {}
void tearDown() {}

static int count(uint16_t f, uint8_t idx, int maxRow = GRID) {
  int n = 0;
  for (int i = 0; i < maxRow * GRID; i++) n += FRAMES[f][i] == idx;
  return n;
}

void test_every_frame_stays_inside_the_circle() {
  for (int f = 0; f < FRAME_COUNT; f++)
    for (int i = 0; i < GRID * GRID; i++) {
      if (!FRAMES[f][i]) continue;
      const float dx = i % GRID + 0.5f - GRID / 2.0f, dy = i / GRID + 0.5f - GRID / 2.0f;
      TEST_ASSERT_TRUE_MESSAGE(sqrtf(dx * dx + dy * dy) <= 13.5f, "pixel outside the round screen");
    }
}

void test_needs_you_loops_and_the_bang_blinks() {
  const Anim& a = ANIMS[ANIM_NEEDS_YOU];
  TEST_ASSERT_TRUE(a.loop);
  TEST_ASSERT_EQUAL_UINT8(ANIM_NEEDS_YOU, a.next);
  int on = 0, off = 0;
  for (int s = 0; s < a.count; s++) {
    const uint16_t f = a.steps[s].frame;
    TEST_ASSERT_GREATER_OR_EQUAL(5, count(f, P_INK));  // he is there, '>' visible
    (count(f, P_CREAM, 8) > 0 ? on : off)++;           // '!' lives above his head
  }
  TEST_ASSERT_GREATER_THAN(0, on);
  TEST_ASSERT_GREATER_THAN(0, off);
}

void test_poof_in_appears_from_smoke_then_idles() {
  const Anim& a = ANIMS[ANIM_POOF_IN];
  TEST_ASSERT_FALSE(a.loop);
  TEST_ASSERT_EQUAL_UINT8(ANIM_IDLE, a.next);
  TEST_ASSERT_EQUAL(0, count(a.steps[0].frame, P_BODY));
  TEST_ASSERT_GREATER_THAN(0, count(a.steps[0].frame, 7));  // smoke
  TEST_ASSERT_GREATER_THAN(20, count(a.steps[a.count - 1].frame, P_BODY));
}

void test_poof_out_vanishes_into_gone() {
  const Anim& a = ANIMS[ANIM_POOF_OUT];
  TEST_ASSERT_FALSE(a.loop);
  TEST_ASSERT_EQUAL_UINT8(ANIM_GONE, a.next);
  TEST_ASSERT_GREATER_THAN(20, count(a.steps[0].frame, P_BODY));
  TEST_ASSERT_EQUAL(0, count(a.steps[a.count - 1].frame, P_BODY));
  const Anim& g = ANIMS[ANIM_GONE];
  TEST_ASSERT_EQUAL_UINT8(ANIM_GONE, g.next);
  for (int s = 0; s < g.count; s++)
    for (int i = 0; i < GRID * GRID; i++) TEST_ASSERT_EQUAL_UINT8(0, FRAMES[g.steps[s].frame][i]);
}

void test_disconnected_icon_is_grey_with_a_clay_slash_and_no_character() {
  const Anim& a = ANIMS[ANIM_DISCONNECTED];
  TEST_ASSERT_EQUAL_UINT8(ANIM_DISCONNECTED, a.next);
  const uint16_t f = a.steps[0].frame;
  TEST_ASSERT_GREATER_THAN(10, count(f, P_LIGHT));
  TEST_ASSERT_GREATER_THAN(5, count(f, P_BODY));
  TEST_ASSERT_EQUAL(0, count(f, P_INK));
}

void test_slot_zero_is_the_art_palette() {
  TEST_ASSERT_EQUAL_HEX16(PALETTE565[P_BODY], SLOT_BODY565[0]);
  TEST_ASSERT_EQUAL_HEX16(PALETTE565[P_SHADOW], SLOT_SHADOW565[0]);
}

void test_slot_colours_match_the_spec() {
  TEST_ASSERT_EQUAL_HEX16(rgb565(0x5B8DEF), SLOT_BODY565[1]);
  TEST_ASSERT_EQUAL_HEX16(rgb565(0x3F66B0), SLOT_SHADOW565[1]);
  TEST_ASSERT_EQUAL_HEX16(rgb565(0xE0709B), SLOT_BODY565[5]);
  TEST_ASSERT_EQUAL_HEX16(rgb565(0xB04F76), SLOT_SHADOW565[5]);
  TEST_ASSERT_EQUAL_HEX16(0xDBAA, rgb565(0xD97757));
}

void test_slot_colour_swaps_only_body_and_shadow() {
  TEST_ASSERT_EQUAL_HEX16(SLOT_BODY565[2], slotColour(P_BODY, 2));
  TEST_ASSERT_EQUAL_HEX16(SLOT_SHADOW565[2], slotColour(P_SHADOW, 2));
  TEST_ASSERT_EQUAL_HEX16(PALETTE565[P_INK], slotColour(P_INK, 2));
  TEST_ASSERT_EQUAL_HEX16(PALETTE565[P_BODY], slotColour(P_BODY, 0));
  TEST_ASSERT_EQUAL_HEX16(PALETTE565[P_BODY], slotColour(P_BODY, 200));  // out of range -> slot 0
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_every_frame_stays_inside_the_circle);
  RUN_TEST(test_needs_you_loops_and_the_bang_blinks);
  RUN_TEST(test_poof_in_appears_from_smoke_then_idles);
  RUN_TEST(test_poof_out_vanishes_into_gone);
  RUN_TEST(test_disconnected_icon_is_grey_with_a_clay_slash_and_no_character);
  RUN_TEST(test_slot_zero_is_the_art_palette);
  RUN_TEST(test_slot_colours_match_the_spec);
  RUN_TEST(test_slot_colour_swaps_only_body_and_shadow);
  return UNITY_END();
}
