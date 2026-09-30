#include <unity.h>
#include <initializer_list>
#include "director.h"

void setUp() {}
void tearDown() {}

// Ticks in 10 ms steps up to `until`, recording each distinct animation entered.
struct Trace {
  uint8_t seen[32];
  int n = 0;
};
static uint32_t runTo(Director& d, uint32_t from, uint32_t until, Trace* t = nullptr) {
  for (uint32_t now = from; now <= until; now += 10) {
    d.tick(now);
    if (t && (t->n == 0 || t->seen[t->n - 1] != d.anim())) t->seen[t->n++] = d.anim();
  }
  return until;
}
static void expectTrace(const Trace& t, std::initializer_list<uint8_t> want) {
  TEST_ASSERT_EQUAL(want.size(), t.n);
  int i = 0;
  for (uint8_t a : want) TEST_ASSERT_EQUAL_UINT8_MESSAGE(a, t.seen[i++], "animation order");
}

void test_new_session_poofs_in_then_idles_forever() {
  Director d;
  d.appear(SState::YourTurn, 0);
  Trace t;
  runTo(d, 0, 60000, &t);
  expectTrace(t, {ANIM_POOF_IN, ANIM_IDLE});
}

void test_new_working_session_poofs_in_then_codes() {
  Director d;
  d.appear(SState::Working, 0);
  Trace t;
  runTo(d, 0, 60000, &t);
  expectTrace(t, {ANIM_POOF_IN, ANIM_TO_CODING, ANIM_CODING});  // idle is passed through in 0 ms
}

void test_working_and_back() {
  Director d;
  d.appear(SState::YourTurn, 0);
  runTo(d, 0, 1000);
  Trace t;
  d.setState(SState::Working);
  runTo(d, 1000, 20000, &t);
  d.setState(SState::YourTurn);
  runTo(d, 20000, 40000, &t);
  expectTrace(t, {ANIM_TO_CODING, ANIM_CODING, ANIM_TO_IDLE, ANIM_IDLE});
}

void test_needs_you_from_coding_goes_through_to_idle() {
  Director d;
  d.appear(SState::Working, 0);
  runTo(d, 0, 2000);
  Trace t;
  d.setState(SState::NeedsYou);
  runTo(d, 2000, 10000, &t);
  expectTrace(t, {ANIM_TO_IDLE, ANIM_NEEDS_YOU});
  Trace t2;
  d.setState(SState::Working);
  runTo(d, 10000, 12000, &t2);
  expectTrace(t2, {ANIM_TO_CODING, ANIM_CODING});
}

void test_ended_poofs_out_and_stays_gone() {
  Director d;
  d.appear(SState::Working, 0);
  runTo(d, 0, 2000);
  Trace t;
  d.setState(SState::Ended);
  runTo(d, 2000, 60000, &t);
  expectTrace(t, {ANIM_TO_IDLE, ANIM_POOF_OUT, ANIM_GONE});
  TEST_ASSERT_TRUE(d.gone());
}

void test_state_change_waits_for_a_transition_to_finish() {
  Director d;
  d.appear(SState::YourTurn, 0);  // poof_in runs 0..320 ms
  d.setState(SState::NeedsYou);
  d.tick(100);
  TEST_ASSERT_EQUAL_UINT8(ANIM_POOF_IN, d.anim());
  runTo(d, 100, 400);
  TEST_ASSERT_EQUAL_UINT8(ANIM_NEEDS_YOU, d.anim());
}

void test_a_gone_session_that_comes_back_poofs_in() {
  Director d;
  d.appear(SState::Ended, 0);
  runTo(d, 0, 2000);
  TEST_ASSERT_TRUE(d.gone());
  Trace t;
  d.setState(SState::YourTurn);
  runTo(d, 2000, 4000, &t);
  expectTrace(t, {ANIM_POOF_IN, ANIM_IDLE});
}

void test_empty_director_idles_forever() {
  Director d;  // no session: the orange idle
  Trace t;
  runTo(d, 0, 60000, &t);
  expectTrace(t, {ANIM_IDLE});
}

void test_tick_returns_valid_frames() {
  Director d;
  d.appear(SState::Working, 0);
  for (uint32_t now = 0; now < 30000; now += 7) {
    if (now == 9000) d.setState(SState::NeedsYou);
    if (now == 15000) d.setState(SState::Ended);
    TEST_ASSERT_LESS_THAN_UINT16(FRAME_COUNT, d.tick(now));
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_new_session_poofs_in_then_idles_forever);
  RUN_TEST(test_new_working_session_poofs_in_then_codes);
  RUN_TEST(test_working_and_back);
  RUN_TEST(test_needs_you_from_coding_goes_through_to_idle);
  RUN_TEST(test_ended_poofs_out_and_stays_gone);
  RUN_TEST(test_state_change_waits_for_a_transition_to_finish);
  RUN_TEST(test_a_gone_session_that_comes_back_poofs_in);
  RUN_TEST(test_empty_director_idles_forever);
  RUN_TEST(test_tick_returns_valid_frames);
  return UNITY_END();
}
