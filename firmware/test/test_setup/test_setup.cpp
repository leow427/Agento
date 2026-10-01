#include <unity.h>
#include <string.h>
#include <string>
#include <ArduinoJson.h>
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

void test_store_remove() {
  CredStore s;
  s.add("a", "");
  s.add("b", "");
  s.add("c", "");
  TEST_ASSERT_TRUE(s.remove("b"));
  TEST_ASSERT_EQUAL(2, s.count());
  TEST_ASSERT_EQUAL_STRING("c", s.at(0).ssid);
  TEST_ASSERT_EQUAL_STRING("a", s.at(1).ssid);
  TEST_ASSERT_FALSE(s.remove("zzz"));
  TEST_ASSERT_TRUE(s.remove("c"));
  TEST_ASSERT_TRUE(s.remove("a"));
  TEST_ASSERT_EQUAL(0, s.count());
}

void test_parse_bits_reads_running_colour_and_ops() {
  const char* j = R"({"seq":3,"running":false,"sessions":[],"messages":[],"config":{"colour":2},)"
                  R"("wifi":[{"id":4,"op":"add","ssid":"iPhone","pass":"secret99"},{"id":5,"op":"delete","ssid":"old"},{"id":6,"op":"rename","ssid":"x"}]})";
  LanyardBits b;
  TEST_ASSERT_TRUE(parseLanyardBits(j, strlen(j), b));
  TEST_ASSERT_FALSE(b.running);
  TEST_ASSERT_EQUAL(2, b.colour);
  TEST_ASSERT_EQUAL(3, b.opCount);
  TEST_ASSERT_EQUAL_UINT32(4, b.ops[0].id);
  TEST_ASSERT_EQUAL(OpKind::Add, b.ops[0].kind);
  TEST_ASSERT_EQUAL_STRING("iPhone", b.ops[0].ssid);
  TEST_ASSERT_EQUAL_STRING("secret99", b.ops[0].pass);
  TEST_ASSERT_EQUAL(OpKind::Delete, b.ops[1].kind);
  TEST_ASSERT_EQUAL_STRING("", b.ops[1].pass);
  TEST_ASSERT_EQUAL(OpKind::Invalid, b.ops[2].kind);
  TEST_ASSERT_EQUAL_STRING("invalid", opKindName(b.ops[2].kind));
}

void test_parse_bits_v1_response_defaults() {
  const char* j = R"({"seq":1,"sessions":[],"messages":[]})";
  LanyardBits b;
  TEST_ASSERT_TRUE(parseLanyardBits(j, strlen(j), b));
  TEST_ASSERT_TRUE(b.running);
  TEST_ASSERT_EQUAL(-1, b.colour);
  TEST_ASSERT_EQUAL(0, b.opCount);
}

void test_parse_bits_bad_json_and_bad_colour() {
  LanyardBits b;
  TEST_ASSERT_FALSE(parseLanyardBits("{nope", 5, b));
  TEST_ASSERT_FALSE(parseLanyardBits("[1]", 3, b));
  const char* j = R"({"config":{"colour":9}})";
  TEST_ASSERT_TRUE(parseLanyardBits(j, strlen(j), b));
  TEST_ASSERT_EQUAL(-1, b.colour);
}

void test_parse_bits_caps_ops() {
  std::string j = R"({"wifi":[)";
  for (int i = 1; i <= 10; i++) j += std::string(i > 1 ? "," : "") + R"({"id":)" + std::to_string(i) + R"(,"op":"delete","ssid":"n"})";
  j += "]}";
  LanyardBits b;
  TEST_ASSERT_TRUE(parseLanyardBits(j.c_str(), j.size(), b));
  TEST_ASSERT_EQUAL(MAX_OPS, b.opCount);
  TEST_ASSERT_EQUAL_UINT32(8, b.ops[7].id);
}

void test_parse_bits_keeps_special_ssids_and_rejects_overlong() {
  // 32 bytes of emoji, a quote and a backslash survive; 33 bytes make the op invalid.
  const char* j = R"({"wifi":[{"id":1,"op":"add","ssid":"😀😀😀😀😀😀😀😀","pass":""},)"
                  R"({"id":2,"op":"add","ssid":"Cafe \"5G\" \\ x","pass":"password1"},)"
                  R"({"id":3,"op":"add","ssid":"123456789012345678901234567890123","pass":""}]})";
  LanyardBits b;
  TEST_ASSERT_TRUE(parseLanyardBits(j, strlen(j), b));
  TEST_ASSERT_EQUAL_STRING("😀😀😀😀😀😀😀😀", b.ops[0].ssid);
  TEST_ASSERT_EQUAL(OpKind::Add, b.ops[0].kind);
  TEST_ASSERT_EQUAL_STRING("Cafe \"5G\" \\ x", b.ops[1].ssid);
  TEST_ASSERT_EQUAL(OpKind::Invalid, b.ops[2].kind);
}

static WifiOp op(uint32_t id, OpKind k, const char* ssid, const char* pass = "") {
  WifiOp o = {};
  o.id = id;
  o.kind = k;
  strcpy(o.ssid, ssid);
  strcpy(o.pass, pass);
  return o;
}

void test_apply_ops_add_delete_and_applied() {
  CredStore s;
  s.add("home", "password1");
  const WifiOp ops[] = {op(1, OpKind::Add, "phone", "password2"), op(2, OpKind::Delete, "home"), op(3, OpKind::Delete, "ghost")};
  uint32_t applied = 0;
  const OpsResult r = applyWifiOps(s, ops, 3, applied, "elsewhere");
  TEST_ASSERT_TRUE(r.changed);
  TEST_ASSERT_FALSE(r.currentRemoved);
  TEST_ASSERT_EQUAL_UINT32(3, applied);
  TEST_ASSERT_EQUAL(1, s.count());
  TEST_ASSERT_EQUAL_STRING("phone", s.at(0).ssid);
}

void test_apply_ops_skips_already_applied_ids() {
  CredStore s;
  const WifiOp ops[] = {op(4, OpKind::Add, "a"), op(5, OpKind::Add, "b")};
  uint32_t applied = 4;
  applyWifiOps(s, ops, 2, applied, "");
  TEST_ASSERT_EQUAL(1, s.count());
  TEST_ASSERT_EQUAL_STRING("b", s.at(0).ssid);
  TEST_ASSERT_EQUAL_UINT32(5, applied);
  const OpsResult again = applyWifiOps(s, ops, 2, applied, "");
  TEST_ASSERT_FALSE(again.changed);
}

void test_apply_ops_invalid_entry_counts_as_applied() {
  CredStore s;
  const WifiOp ops[] = {op(1, OpKind::Add, "net", "short"), op(2, OpKind::Invalid, "x"), op(3, OpKind::Add, "", "")};
  uint32_t applied = 0;
  const OpsResult r = applyWifiOps(s, ops, 3, applied, "");
  TEST_ASSERT_FALSE(r.changed);
  TEST_ASSERT_EQUAL(0, s.count());
  TEST_ASSERT_EQUAL_UINT32(3, applied);
}

void test_apply_ops_deleting_current_network() {
  CredStore s;
  s.add("home", "password1");
  s.add("phone", "password2");
  const WifiOp ops[] = {op(1, OpKind::Delete, "phone")};
  uint32_t applied = 0;
  const OpsResult r = applyWifiOps(s, ops, 1, applied, "phone");
  TEST_ASSERT_TRUE(r.currentRemoved);
  TEST_ASSERT_EQUAL(1, s.count());
}

void test_apply_ops_fourth_add_drops_oldest_and_same_name_replaces() {
  CredStore s;
  s.add("a", "");
  s.add("b", "");
  s.add("c", "");
  const WifiOp ops[] = {op(1, OpKind::Add, "d"), op(2, OpKind::Add, "c", "newpass99")};
  uint32_t applied = 0;
  applyWifiOps(s, ops, 2, applied, "");
  TEST_ASSERT_EQUAL(3, s.count());
  TEST_ASSERT_EQUAL_STRING("c", s.at(0).ssid);
  TEST_ASSERT_EQUAL_STRING("newpass99", s.at(0).pass);
  TEST_ASSERT_EQUAL_STRING("d", s.at(1).ssid);
  TEST_ASSERT_EQUAL_STRING("b", s.at(2).ssid);
}

void test_report_json_shape() {
  CredStore s;
  s.add("home", "password1");
  s.add("Cafe \"5G\"", "");
  const std::string out = reportJson("home", s, 2, true, false, 81, 7);
  TEST_ASSERT_EQUAL(std::string::npos, out.find("password1"));
  JsonDocument d;
  TEST_ASSERT_FALSE(deserializeJson(d, out));
  TEST_ASSERT_EQUAL_STRING("home", d["wifi"]);
  TEST_ASSERT_EQUAL(2, d["nets"].size());
  TEST_ASSERT_EQUAL_STRING("Cafe \"5G\"", d["nets"][0]);
  TEST_ASSERT_EQUAL_STRING("home", d["nets"][1]);
  TEST_ASSERT_EQUAL(2, d["colour"].as<int>());
  TEST_ASSERT_TRUE(d["battery"]["present"].as<bool>());
  TEST_ASSERT_FALSE(d["battery"]["charging"].as<bool>());
  TEST_ASSERT_EQUAL(81, d["battery"]["percent"].as<int>());
  TEST_ASSERT_EQUAL_UINT32(7, d["applied"].as<uint32_t>());
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
  RUN_TEST(test_store_remove);
  RUN_TEST(test_parse_bits_reads_running_colour_and_ops);
  RUN_TEST(test_parse_bits_v1_response_defaults);
  RUN_TEST(test_parse_bits_bad_json_and_bad_colour);
  RUN_TEST(test_parse_bits_caps_ops);
  RUN_TEST(test_parse_bits_keeps_special_ssids_and_rejects_overlong);
  RUN_TEST(test_apply_ops_add_delete_and_applied);
  RUN_TEST(test_apply_ops_skips_already_applied_ids);
  RUN_TEST(test_apply_ops_invalid_entry_counts_as_applied);
  RUN_TEST(test_apply_ops_deleting_current_network);
  RUN_TEST(test_apply_ops_fourth_add_drops_oldest_and_same_name_replaces);
  RUN_TEST(test_report_json_shape);
  return UNITY_END();
}
