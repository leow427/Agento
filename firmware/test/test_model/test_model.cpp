#include <unity.h>
#include <string.h>
#include <string>
#include "session_model.h"
#include "link.h"

void setUp() {}
void tearDown() {}

static bool feed(Model& m, const std::string& j) { return m.apply(j.c_str(), j.size()); }

static std::string sess(const char* id, int slot, const char* project, const char* state) {
  char b[256];
  snprintf(b, sizeof b, "{\"id\":\"%s\",\"slot\":%d,\"project\":\"%s\",\"state\":\"%s\",\"updated\":1.5}", id, slot, project, state);
  return b;
}
static std::string msg(int seq, const char* sid, const std::string& text) {
  return "{\"seq\":" + std::to_string(seq) + ",\"sid\":\"" + sid + "\",\"ts\":1.0,\"text\":\"" + text + "\"}";
}
static std::string resp(int seq, const std::string& sessions, const std::string& messages = "") {
  return "{\"seq\":" + std::to_string(seq) + ",\"sessions\":[" + sessions + "],\"messages\":[" + messages + "]}";
}

void test_bad_json_changes_nothing() {
  Model m;
  TEST_ASSERT_TRUE(feed(m, resp(3, sess("a", 0, "P", "working"), msg(3, "a", "x"))));
  TEST_ASSERT_FALSE(feed(m, "{nope"));
  TEST_ASSERT_FALSE(feed(m, "{\"seq\":1}"));  // no sessions array
  TEST_ASSERT_EQUAL(0, m.changeCount());
  TEST_ASSERT_EQUAL(1, m.count());
  TEST_ASSERT_EQUAL_UINT32(3, m.seq());
}

void test_new_sessions_appear_sorted_by_slot() {
  Model m;
  feed(m, resp(0, sess("b", 2, "Beta", "your_turn") + "," + sess("a", 0, "Agento", "working")));
  TEST_ASSERT_EQUAL(2, m.count());
  TEST_ASSERT_EQUAL_STRING("a", m.at(0).id);
  TEST_ASSERT_EQUAL_STRING("Agento", m.at(0).project);
  TEST_ASSERT_EQUAL_UINT8(0, m.at(0).slot);
  TEST_ASSERT_EQUAL(SState::Working, m.at(0).state);
  TEST_ASSERT_EQUAL_STRING("b", m.at(1).id);
  TEST_ASSERT_EQUAL(2, m.changeCount());
  TEST_ASSERT_EQUAL(Change::Appeared, m.change(0).kind);
  TEST_ASSERT_EQUAL(Change::Appeared, m.change(1).kind);
}

void test_missing_session_is_removed() {
  Model m;
  feed(m, resp(0, sess("a", 0, "A", "working") + "," + sess("b", 1, "B", "working")));
  feed(m, resp(0, sess("b", 1, "B", "working")));
  TEST_ASSERT_EQUAL(1, m.count());
  TEST_ASSERT_EQUAL_STRING("b", m.at(0).id);
  TEST_ASSERT_EQUAL(1, m.changeCount());
  TEST_ASSERT_EQUAL(Change::Removed, m.change(0).kind);
  TEST_ASSERT_EQUAL_STRING("a", m.change(0).id);
}

void test_state_change_is_reported_once() {
  Model m;
  feed(m, resp(0, sess("a", 0, "A", "working")));
  feed(m, resp(0, sess("a", 0, "A", "needs_you")));
  TEST_ASSERT_EQUAL(1, m.changeCount());
  TEST_ASSERT_EQUAL(Change::StateChanged, m.change(0).kind);
  TEST_ASSERT_EQUAL(SState::Working, m.change(0).from);
  TEST_ASSERT_EQUAL(SState::NeedsYou, m.change(0).to);
  feed(m, resp(0, sess("a", 0, "A", "needs_you")));
  TEST_ASSERT_EQUAL(0, m.changeCount());
}

void test_all_states_parse_and_unknown_is_your_turn() {
  Model m;
  feed(m, resp(0, sess("a", 0, "A", "your_turn") + "," + sess("b", 1, "B", "ended") + "," + sess("c", 2, "C", "weird")));
  TEST_ASSERT_EQUAL(SState::YourTurn, m.at(0).state);
  TEST_ASSERT_EQUAL(SState::Ended, m.at(1).state);
  TEST_ASSERT_EQUAL(SState::YourTurn, m.at(2).state);
}

void test_messages_go_to_their_session_oldest_first() {
  Model m;
  feed(m, resp(3, sess("a", 0, "A", "working") + "," + sess("b", 1, "B", "working"),
                msg(1, "a", "> hi") + "," + msg(2, "b", "Bash: ls") + "," + msg(3, "a", "Read: x.ts")));
  TEST_ASSERT_EQUAL_UINT32(3, m.seq());
  TEST_ASSERT_EQUAL(2, m.at(0).msgCount);
  TEST_ASSERT_EQUAL_STRING("> hi", m.at(0).msg(0).text);
  TEST_ASSERT_EQUAL_STRING("Read: x.ts", m.at(0).msg(1).text);
  TEST_ASSERT_EQUAL(1, m.at(1).msgCount);
}

void test_old_and_orphan_messages_are_ignored() {
  Model m;
  feed(m, resp(2, sess("a", 0, "A", "working"), msg(1, "a", "one") + "," + msg(2, "zzz", "orphan")));
  feed(m, resp(3, sess("a", 0, "A", "working"), msg(1, "a", "one") + "," + msg(3, "a", "three")));
  TEST_ASSERT_EQUAL(2, m.at(0).msgCount);
  TEST_ASSERT_EQUAL_STRING("three", m.at(0).msg(1).text);
}

void test_each_session_keeps_its_last_20_messages() {
  Model m;
  std::string msgs;
  for (int i = 1; i <= 25; i++) msgs += (i > 1 ? "," : "") + msg(i, "a", "m" + std::to_string(i));
  feed(m, resp(25, sess("a", 0, "A", "working"), msgs));
  TEST_ASSERT_EQUAL(20, m.at(0).msgCount);
  TEST_ASSERT_EQUAL_STRING("m6", m.at(0).msg(0).text);
  TEST_ASSERT_EQUAL_STRING("m25", m.at(0).msg(19).text);
  feed(m, resp(26, sess("a", 0, "A", "working"), msg(26, "a", "m26")));
  TEST_ASSERT_EQUAL_STRING("m7", m.at(0).msg(0).text);
  TEST_ASSERT_EQUAL_STRING("m26", m.at(0).msg(19).text);
}

void test_server_reset_rebuilds_messages() {
  Model m;
  feed(m, resp(50, sess("a", 0, "A", "working"), msg(50, "a", "old")));
  // The Mac lost its state: its seq is lower than ours, so it sent full history from 0.
  feed(m, resp(2, sess("a", 0, "A", "working"), msg(1, "a", "new1") + "," + msg(2, "a", "new2")));
  TEST_ASSERT_EQUAL_UINT32(2, m.seq());
  TEST_ASSERT_EQUAL(2, m.at(0).msgCount);
  TEST_ASSERT_EQUAL_STRING("new1", m.at(0).msg(0).text);
}

void test_removed_session_takes_its_messages() {
  Model m;
  feed(m, resp(1, sess("a", 0, "A", "working"), msg(1, "a", "x")));
  feed(m, resp(1, ""));
  feed(m, resp(1, sess("a", 0, "A", "working")));
  TEST_ASSERT_EQUAL(0, m.at(0).msgCount);
}

void test_long_text_is_cut_at_a_utf8_boundary() {
  Model m;
  std::string t;
  for (int i = 0; i < 200; i++) t += "\xE2\x80\xA6";  // 200 x "…" (3 bytes each)
  feed(m, resp(1, sess("a", 0, "A", "working"), msg(1, "a", t)));
  const char* s = m.at(0).msg(0).text;
  const size_t n = strlen(s);
  TEST_ASSERT_TRUE(n > 0 && n < MSG_BYTES);
  TEST_ASSERT_EQUAL(0, n % 3);
}

void test_long_project_is_cut() {
  Model m;
  std::string p(100, 'p');
  feed(m, resp(0, sess("a", 0, p.c_str(), "working")));
  TEST_ASSERT_EQUAL(PROJECT_BYTES - 1, strlen(m.at(0).project));
}

void test_at_most_6_sessions_and_bad_slots_dropped() {
  Model m;
  std::string s;
  for (int i = 0; i < 8; i++) s += (i ? "," : "") + sess(("s" + std::to_string(i)).c_str(), i % 6, "P", "working");
  s += "," + sess("bad", 9, "P", "working");
  feed(m, resp(0, s));
  TEST_ASSERT_EQUAL(6, m.count());
}

void test_link_is_down_without_wifi_or_after_3_failed_polls() {
  Link l;
  TEST_ASSERT_TRUE(l.disconnected());  // nothing yet
  l.setWifi(true);
  l.pollOk();
  TEST_ASSERT_FALSE(l.disconnected());
  l.pollFailed();
  l.pollFailed();
  TEST_ASSERT_FALSE(l.disconnected());
  l.pollFailed();
  TEST_ASSERT_TRUE(l.disconnected());
  l.pollOk();
  TEST_ASSERT_FALSE(l.disconnected());
  l.setWifi(false);
  TEST_ASSERT_TRUE(l.disconnected());
}

void test_backoff_doubles_to_a_cap_and_resets() {
  Backoff b(1000, 30000);
  TEST_ASSERT_EQUAL_UINT32(1000, b.next());
  TEST_ASSERT_EQUAL_UINT32(2000, b.next());
  TEST_ASSERT_EQUAL_UINT32(4000, b.next());
  for (int i = 0; i < 10; i++) b.next();
  TEST_ASSERT_EQUAL_UINT32(30000, b.next());
  b.reset();
  TEST_ASSERT_EQUAL_UINT32(1000, b.next());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_bad_json_changes_nothing);
  RUN_TEST(test_new_sessions_appear_sorted_by_slot);
  RUN_TEST(test_missing_session_is_removed);
  RUN_TEST(test_state_change_is_reported_once);
  RUN_TEST(test_all_states_parse_and_unknown_is_your_turn);
  RUN_TEST(test_messages_go_to_their_session_oldest_first);
  RUN_TEST(test_old_and_orphan_messages_are_ignored);
  RUN_TEST(test_each_session_keeps_its_last_20_messages);
  RUN_TEST(test_server_reset_rebuilds_messages);
  RUN_TEST(test_removed_session_takes_its_messages);
  RUN_TEST(test_long_text_is_cut_at_a_utf8_boundary);
  RUN_TEST(test_long_project_is_cut);
  RUN_TEST(test_at_most_6_sessions_and_bad_slots_dropped);
  RUN_TEST(test_link_is_down_without_wifi_or_after_3_failed_polls);
  RUN_TEST(test_backoff_doubles_to_a_cap_and_resets);
  return UNITY_END();
}
