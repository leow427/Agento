#include <unity.h>
#include <string.h>
#include <string>
#include "provision.h"
#include "buddy_svg.h"
#include "frames_data.h"

void setUp() {}
void tearDown() {}

void test_store_keeps_newest_first_dedupes_and_caps_at_3() {
  CredStore s;
  s.add("home", "password1");
  s.add("phone", "password2");
  s.add("home", "newpass99");
  TEST_ASSERT_EQUAL(2, s.count());
  TEST_ASSERT_EQUAL_STRING("home", s.at(0).ssid);
  TEST_ASSERT_EQUAL_STRING("newpass99", s.at(0).pass);
  TEST_ASSERT_EQUAL_STRING("phone", s.at(1).ssid);
  s.add("a", "");
  s.add("b", "");
  TEST_ASSERT_EQUAL(3, s.count());
  TEST_ASSERT_EQUAL_STRING("b", s.at(0).ssid);
  TEST_ASSERT_EQUAL_STRING("home", s.at(2).ssid);
}

void test_store_round_trips_through_text() {
  CredStore a;
  a.add("cafe \"wifi\"", "p@ss;word:1");
  a.add("home", "hunter22");
  std::string text = a.serialize();
  CredStore b;
  TEST_ASSERT_TRUE(b.deserialize(text.c_str()));
  TEST_ASSERT_EQUAL(2, b.count());
  TEST_ASSERT_EQUAL_STRING("home", b.at(0).ssid);
  TEST_ASSERT_EQUAL_STRING("cafe \"wifi\"", b.at(1).ssid);
  TEST_ASSERT_EQUAL_STRING("p@ss;word:1", b.at(1).pass);
}

void test_store_rejects_garbage_and_starts_empty() {
  CredStore s;
  s.add("x", "");
  TEST_ASSERT_FALSE(s.deserialize("not json"));
  TEST_ASSERT_EQUAL(0, s.count());
  TEST_ASSERT_TRUE(s.deserialize(""));  // nothing saved yet
  TEST_ASSERT_EQUAL(0, s.count());
}

void test_store_clear() {
  CredStore s;
  s.add("x", "");
  s.clear();
  TEST_ASSERT_EQUAL(0, s.count());
}

void test_validate_creds() {
  TEST_ASSERT_NULL(validateCreds("home", "password"));
  TEST_ASSERT_NULL(validateCreds("open-net", ""));
  TEST_ASSERT_NOT_NULL(validateCreds("", "password"));
  TEST_ASSERT_NOT_NULL(validateCreds("home", "short"));
  TEST_ASSERT_NOT_NULL(validateCreds(std::string(33, 's').c_str(), "password"));
  TEST_ASSERT_NOT_NULL(validateCreds("home", std::string(64, 'p').c_str()));
  TEST_ASSERT_NULL(validateCreds("home", std::string(63, 'p').c_str()));
}

void test_wifi_qr_payload_escapes_specials() {
  char out[160];
  wifiQrPayload(out, sizeof out, "TerminalBuddy-A1B2", "abcd2345");
  TEST_ASSERT_EQUAL_STRING("WIFI:T:WPA;S:TerminalBuddy-A1B2;P:abcd2345;;", out);
  wifiQrPayload(out, sizeof out, "a;b", "c:d\\e\"f,");
  TEST_ASSERT_EQUAL_STRING("WIFI:T:WPA;S:a\\;b;P:c\\:d\\\\e\\\"f\\,;;", out);
}

void test_ap_name_from_mac() {
  const uint8_t mac[6] = {0x24, 0x0a, 0xc4, 0x12, 0xa1, 0xb2};
  char out[32];
  apName(out, sizeof out, mac);
  TEST_ASSERT_EQUAL_STRING("TerminalBuddy-A1B2", out);
}

static uint32_t counter = 0;
static uint32_t fakeRng() { return counter++; }

void test_ap_password_is_8_unambiguous_chars() {
  char out[9];
  counter = 0;
  apPassword(out, fakeRng);
  TEST_ASSERT_EQUAL(8, strlen(out));
  for (const char* p = out; *p; p++) TEST_ASSERT_NULL(strchr("0O1lI", *p));
}

void test_flow_join_open_connect_done() {
  SetupFlow f;
  TEST_ASSERT_EQUAL(SetupStep::Join, f.step());
  f.clients(1);
  TEST_ASSERT_EQUAL(SetupStep::Open, f.step());
  f.clients(0);
  TEST_ASSERT_EQUAL(SetupStep::Join, f.step());
  f.clients(1);
  f.submit("home");
  TEST_ASSERT_EQUAL(SetupStep::Connecting, f.step());
  TEST_ASSERT_EQUAL_STRING("home", f.ssid());
  f.clients(0);  // the phone may drop while the radio changes channel
  TEST_ASSERT_EQUAL(SetupStep::Connecting, f.step());
  f.joined(true);
  TEST_ASSERT_EQUAL(SetupStep::Done, f.step());
}

void test_flow_failure_then_retry() {
  SetupFlow f;
  f.clients(1);
  f.submit("home");
  f.joined(false);
  TEST_ASSERT_EQUAL(SetupStep::Failed, f.step());
  TEST_ASSERT_EQUAL_STRING("home", f.ssid());
  f.clients(1);
  TEST_ASSERT_EQUAL(SetupStep::Failed, f.step());  // stays until the next try
  f.submit("home2");
  TEST_ASSERT_EQUAL(SetupStep::Connecting, f.step());
}

void test_flow_status_json() {
  SetupFlow f;
  f.clients(1);
  f.submit("my \"net\"");
  f.joined(false);
  const std::string j = f.statusJson();
  TEST_ASSERT_EQUAL_STRING("{\"step\":\"failed\",\"ssid\":\"my \\\"net\\\"\"}", j.c_str());
}

void test_buddy_svg_draws_the_idle_frame_in_slot_colours() {
  const std::string svg = buddySvg(ANIMS[ANIM_IDLE].steps[0].frame, 1);
  TEST_ASSERT_EQUAL(0, svg.find("<svg"));
  TEST_ASSERT_NOT_EQUAL(std::string::npos, svg.find("#5b8def"));  // slot 1 body
  TEST_ASSERT_EQUAL(std::string::npos, svg.find("#d97757"));     // not clay
  TEST_ASSERT_NOT_EQUAL(std::string::npos, svg.find("#141313"));  // ink
  TEST_ASSERT_NOT_EQUAL(std::string::npos, svg.find("shape-rendering=\"crispEdges\""));
  TEST_ASSERT_TRUE(svg.size() < 8000);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_store_keeps_newest_first_dedupes_and_caps_at_3);
  RUN_TEST(test_store_round_trips_through_text);
  RUN_TEST(test_store_rejects_garbage_and_starts_empty);
  RUN_TEST(test_store_clear);
  RUN_TEST(test_validate_creds);
  RUN_TEST(test_wifi_qr_payload_escapes_specials);
  RUN_TEST(test_ap_name_from_mac);
  RUN_TEST(test_ap_password_is_8_unambiguous_chars);
  RUN_TEST(test_flow_join_open_connect_done);
  RUN_TEST(test_flow_failure_then_retry);
  RUN_TEST(test_flow_status_json);
  RUN_TEST(test_buddy_svg_draws_the_idle_frame_in_slot_colours);
  return UNITY_END();
}
