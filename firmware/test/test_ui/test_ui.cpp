#include <unity.h>
#include <string.h>
#include "ui_logic.h"

void setUp() {}
void tearDown() {}

static Swipe swipe(int x0, int y0, int x1, int y1) {
  SwipeDetector d;
  TEST_ASSERT_EQUAL(Swipe::None, d.feed(true, x0, y0, 0));
  TEST_ASSERT_EQUAL(Swipe::None, d.feed(true, (x0 + x1) / 2, (y0 + y1) / 2, 50));
  TEST_ASSERT_EQUAL(Swipe::None, d.feed(true, x1, y1, 100));
  return d.feed(false, 0, 0, 150);
}

void test_swipes_by_dominant_axis() {
  TEST_ASSERT_EQUAL(Swipe::Left, swipe(300, 230, 100, 250));
  TEST_ASSERT_EQUAL(Swipe::Right, swipe(100, 230, 320, 200));
  TEST_ASSERT_EQUAL(Swipe::Up, swipe(230, 350, 250, 120));
  TEST_ASSERT_EQUAL(Swipe::Down, swipe(230, 100, 220, 330));
}

void test_short_moves_and_taps_are_not_swipes() {
  TEST_ASSERT_EQUAL(Swipe::None, swipe(230, 230, 260, 240));
  TEST_ASSERT_EQUAL(Swipe::None, swipe(230, 230, 230, 230));
}

void test_release_without_touch_is_nothing() {
  SwipeDetector d;
  TEST_ASSERT_EQUAL(Swipe::None, d.feed(false, 0, 0, 0));
}

void test_one_swipe_per_gesture() {
  SwipeDetector d;
  d.feed(true, 300, 230, 0);
  d.feed(true, 100, 230, 50);
  TEST_ASSERT_EQUAL(Swipe::Left, d.feed(false, 0, 0, 100));
  TEST_ASSERT_EQUAL(Swipe::None, d.feed(false, 0, 0, 120));
}

void test_sanitize_maps_utf8_to_ascii() {
  char out[64];
  toAscii(out, sizeof out, "a\xE2\x80\xA6 b \xC3\xA9 \xF0\x9F\x98\x80!");
  TEST_ASSERT_EQUAL_STRING("a... b ? ?!", out);
  toAscii(out, 5, "abcdefgh");
  TEST_ASSERT_EQUAL_STRING("abcd", out);
}

static int widths[] = {10, 8, 8};
static int widthFor(int k, void*) { return widths[k < 3 ? k : 2]; }

void test_wrap_from_end_fills_bottom_line_first() {
  WrapLine lines[8];
  const char* t = "one two three four five";
  const int n = wrapFromEnd(t, widthFor, nullptr, lines, 8);
  // bottom line (k=0) takes up to 10 chars, then 8, then 8
  TEST_ASSERT_EQUAL(3, n);
  TEST_ASSERT_EQUAL_STRING_LEN("four five", t + lines[0].start, lines[0].len);
  TEST_ASSERT_EQUAL_STRING_LEN("three", t + lines[1].start, lines[1].len);
  TEST_ASSERT_EQUAL_STRING_LEN("one two", t + lines[2].start, lines[2].len);
}

void test_wrap_hard_splits_long_words() {
  WrapLine lines[8];
  const char* t = "abcdefghijklmnop";
  const int n = wrapFromEnd(t, widthFor, nullptr, lines, 8);
  TEST_ASSERT_EQUAL(2, n);
  TEST_ASSERT_EQUAL_STRING_LEN("ghijklmnop", t + lines[0].start, lines[0].len);
  TEST_ASSERT_EQUAL_STRING_LEN("abcdef", t + lines[1].start, lines[1].len);
}

void test_wrap_stops_at_max_lines() {
  WrapLine lines[2];
  const int n = wrapFromEnd("a b c d e f g h i j k l m n o p q r s t", widthFor, nullptr, lines, 2);
  TEST_ASSERT_EQUAL(2, n);
}

void test_wrap_empty_text_is_one_empty_line() {
  WrapLine lines[2];
  TEST_ASSERT_EQUAL(0, wrapFromEnd("", widthFor, nullptr, lines, 2));
}

void test_chars_that_fit_across_the_circle() {
  // 466 px circle, r = 233; a 16 px line through the centre, 12 px chars, 20 px margin each side.
  TEST_ASSERT_EQUAL((466 - 40) / 12, charsAcross(225, 16, 12, 233, 20));
  // Near the top edge the chord is short.
  TEST_ASSERT_TRUE(charsAcross(10, 16, 12, 233, 20) < 10);
  TEST_ASSERT_EQUAL(0, charsAcross(-40, 16, 12, 233, 20));
  TEST_ASSERT_EQUAL(0, charsAcross(470, 16, 12, 233, 20));
}

static bool vis[6];
static bool isVisible(int i, void*) { return vis[i]; }

void test_step_selection_wraps_and_skips_hidden() {
  bool v[6] = {true, false, true, true, false, false};
  memcpy(vis, v, sizeof vis);
  TEST_ASSERT_EQUAL(2, stepSelection(4, 0, +1, isVisible, nullptr));
  TEST_ASSERT_EQUAL(3, stepSelection(4, 2, +1, isVisible, nullptr));
  TEST_ASSERT_EQUAL(0, stepSelection(4, 3, +1, isVisible, nullptr));
  TEST_ASSERT_EQUAL(3, stepSelection(4, 0, -1, isVisible, nullptr));
}

void test_step_selection_with_nothing_visible() {
  memset(vis, 0, sizeof vis);
  TEST_ASSERT_EQUAL(-1, stepSelection(3, 0, +1, isVisible, nullptr));
  TEST_ASSERT_EQUAL(-1, stepSelection(0, 0, +1, isVisible, nullptr));
}

void test_first_visible() {
  bool v[6] = {false, false, true, true, false, false};
  memcpy(vis, v, sizeof vis);
  TEST_ASSERT_EQUAL(2, stepSelection(4, -1, +1, isVisible, nullptr));
}

void test_colour_rank_counts_visible_sessions_before() {
  bool v[6] = {false, true, true, false, true, false};
  memcpy(vis, v, sizeof vis);
  TEST_ASSERT_EQUAL(0, colourRank(1, isVisible, nullptr));  // first visible is orange
  TEST_ASSERT_EQUAL(1, colourRank(2, isVisible, nullptr));
  TEST_ASSERT_EQUAL(2, colourRank(4, isVisible, nullptr));
  TEST_ASSERT_EQUAL(0, colourRank(0, isVisible, nullptr));
}

void test_battery_fill_width() {
  TEST_ASSERT_EQUAL(0, batteryFillPx(0, 40));
  TEST_ASSERT_EQUAL(1, batteryFillPx(1, 40));  // any charge shows at least a sliver
  TEST_ASSERT_EQUAL(20, batteryFillPx(50, 40));
  TEST_ASSERT_EQUAL(40, batteryFillPx(100, 40));
  TEST_ASSERT_EQUAL(40, batteryFillPx(130, 40));
  TEST_ASSERT_EQUAL(0, batteryFillPx(-5, 40));
}

void test_battery_low() {
  TEST_ASSERT_TRUE(batteryLow(15));
  TEST_ASSERT_FALSE(batteryLow(16));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_swipes_by_dominant_axis);
  RUN_TEST(test_short_moves_and_taps_are_not_swipes);
  RUN_TEST(test_release_without_touch_is_nothing);
  RUN_TEST(test_one_swipe_per_gesture);
  RUN_TEST(test_sanitize_maps_utf8_to_ascii);
  RUN_TEST(test_wrap_from_end_fills_bottom_line_first);
  RUN_TEST(test_wrap_hard_splits_long_words);
  RUN_TEST(test_wrap_stops_at_max_lines);
  RUN_TEST(test_wrap_empty_text_is_one_empty_line);
  RUN_TEST(test_chars_that_fit_across_the_circle);
  RUN_TEST(test_step_selection_wraps_and_skips_hidden);
  RUN_TEST(test_step_selection_with_nothing_visible);
  RUN_TEST(test_first_visible);
  RUN_TEST(test_colour_rank_counts_visible_sessions_before);
  RUN_TEST(test_battery_fill_width);
  RUN_TEST(test_battery_low);
  return UNITY_END();
}
