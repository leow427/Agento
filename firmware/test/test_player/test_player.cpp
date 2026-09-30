#include <unity.h>
#include "player.h"

void setUp() {}
void tearDown() {}

void test_starts_idle() {
  Player p(0);
  p.tick(0);
  TEST_ASSERT_EQUAL_UINT8(ANIM_IDLE, p.anim());
}

void test_full_cycle_times() {
  Player p(0);
  p.tick(6399); TEST_ASSERT_EQUAL_UINT8(ANIM_IDLE, p.anim());
  p.tick(6400); TEST_ASSERT_EQUAL_UINT8(ANIM_TO_CODING, p.anim());
  p.tick(6719); TEST_ASSERT_EQUAL_UINT8(ANIM_TO_CODING, p.anim());
  p.tick(6720); TEST_ASSERT_EQUAL_UINT8(ANIM_CODING, p.anim());
  p.tick(13759); TEST_ASSERT_EQUAL_UINT8(ANIM_CODING, p.anim());
  p.tick(13760); TEST_ASSERT_EQUAL_UINT8(ANIM_TO_IDLE, p.anim());
  p.tick(14079); TEST_ASSERT_EQUAL_UINT8(ANIM_TO_IDLE, p.anim());
  p.tick(14080); TEST_ASSERT_EQUAL_UINT8(ANIM_IDLE, p.anim());
}

void test_tick_returns_valid_frame_indexes() {
  Player p(0);
  for (uint32_t t = 0; t < 30000; t += 7) {
    TEST_ASSERT_LESS_THAN_UINT16(FRAME_COUNT, p.tick(t));
  }
}

void test_toggle_from_idle_and_coding() {
  Player p(0);
  p.tick(100);
  p.toggle(100);
  TEST_ASSERT_EQUAL_UINT8(ANIM_TO_CODING, p.anim());
  p.tick(420);
  TEST_ASSERT_EQUAL_UINT8(ANIM_CODING, p.anim());
  p.toggle(500);
  TEST_ASSERT_EQUAL_UINT8(ANIM_TO_IDLE, p.anim());
}

void test_toggle_ignored_during_transitions() {
  Player p(0);
  p.toggle(0);
  p.toggle(50);
  TEST_ASSERT_EQUAL_UINT8(ANIM_TO_CODING, p.anim());
  p.tick(330);
  p.toggle(330);
  p.toggle(331);
  TEST_ASSERT_EQUAL_UINT8(ANIM_TO_IDLE, p.anim());
}

void test_big_time_gap_catches_up() {
  Player p(0);
  uint16_t f = p.tick(10000000u);
  TEST_ASSERT_LESS_THAN_UINT16(FRAME_COUNT, f);
  TEST_ASSERT_LESS_THAN_UINT8(ANIM_COUNT, p.anim());
}

void test_survives_millis_wraparound() {
  const uint32_t t0 = 0xFFFFF000u;  // about 4 seconds before the 32-bit millis() wraps
  Player p(t0);
  p.tick(t0 + 6399u); TEST_ASSERT_EQUAL_UINT8(ANIM_IDLE, p.anim());
  p.tick(t0 + 6400u); TEST_ASSERT_EQUAL_UINT8(ANIM_TO_CODING, p.anim());
  p.tick(t0 + 6720u); TEST_ASSERT_EQUAL_UINT8(ANIM_CODING, p.anim());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_starts_idle);
  RUN_TEST(test_full_cycle_times);
  RUN_TEST(test_tick_returns_valid_frame_indexes);
  RUN_TEST(test_toggle_from_idle_and_coding);
  RUN_TEST(test_toggle_ignored_during_transitions);
  RUN_TEST(test_big_time_gap_catches_up);
  RUN_TEST(test_survives_millis_wraparound);
  return UNITY_END();
}
