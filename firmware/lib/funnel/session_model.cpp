#include "session_model.h"
#include <string.h>
#include <ArduinoJson.h>

const char* stateName(SState s) {
  switch (s) {
    case SState::Working: return "working";
    case SState::NeedsYou: return "needs_you";
    case SState::Ended: return "ended";
    default: return "your_turn";
  }
}

static SState parseState(const char* s) {
  if (!s) return SState::YourTurn;
  if (!strcmp(s, "working")) return SState::Working;
  if (!strcmp(s, "needs_you")) return SState::NeedsYou;
  if (!strcmp(s, "ended")) return SState::Ended;
  return SState::YourTurn;
}

void copyUtf8(char* dst, const char* src, size_t n) {
  if (n == 0) return;
  size_t len = src ? strlen(src) : 0;
  if (len >= n) {
    len = n - 1;
    while (len > 0 && ((uint8_t)src[len] & 0xC0) == 0x80) len--;  // don't split a code point
  }
  if (len) memcpy(dst, src, len);
  dst[len] = 0;
}

void Session::push(uint32_t seq, const char* text) {
  Msg* m;
  if (msgCount < MAX_MSGS) {
    m = &msgs[(msgHead + msgCount++) % MAX_MSGS];
  } else {
    m = &msgs[msgHead];
    msgHead = (msgHead + 1) % MAX_MSGS;
  }
  m->seq = seq;
  copyUtf8(m->text, text, MSG_BYTES);
}

int Model::find(const char* id) const {
  for (int i = 0; i < count_; i++)
    if (!strcmp(sessions_[i].id, id)) return i;
  return -1;
}

void Model::addChange(Change::Kind k, const Session& s, SState from, SState to) {
  if (changeCount_ >= 2 * MAX_SESSIONS) return;
  Change& c = changes_[changeCount_++];
  c.kind = k;
  copyUtf8(c.id, s.id, ID_BYTES);
  c.slot = s.slot;
  c.from = from;
  c.to = to;
}

bool Model::apply(const char* json, size_t len) {
  changeCount_ = 0;
  JsonDocument doc;
  if (deserializeJson(doc, json, len)) return false;
  JsonArrayConst list = doc["sessions"].as<JsonArrayConst>();
  if (list.isNull() || !doc["seq"].is<uint32_t>()) return false;

  const uint32_t serverSeq = doc["seq"].as<uint32_t>();
  const bool reset = serverSeq < seq_;  // the Mac lost its state and resent history from 0
  const uint32_t after = reset ? 0 : seq_;

  // Build the new session list, carrying over sessions we already know.
  static Session next[MAX_SESSIONS];
  int n = 0;
  bool slotUsed[MAX_SESSIONS] = {};
  for (JsonObjectConst js : list) {
    const char* id = js["id"] | "";
    const int slot = js["slot"] | -1;
    if (!*id || slot < 0 || slot >= MAX_SESSIONS || slotUsed[slot] || n >= MAX_SESSIONS) continue;
    slotUsed[slot] = true;
    Session& s = next[n++];
    const int old = find(id);
    const SState st = parseState(js["state"]);
    if (old >= 0) {
      s = sessions_[old];
      if (reset) s.msgCount = s.msgHead = 0;
      if (s.state != st) addChange(Change::StateChanged, s, s.state, st);
    } else {
      memset(&s, 0, sizeof s);
      copyUtf8(s.id, id, ID_BYTES);
      s.slot = slot;
      s.state = st;
      addChange(Change::Appeared, s, st, st);
    }
    s.state = st;
    copyUtf8(s.project, js["project"] | "", PROJECT_BYTES);
  }
  for (int i = 0; i < count_; i++) {
    bool kept = false;
    for (int j = 0; j < n && !kept; j++) kept = !strcmp(next[j].id, sessions_[i].id);
    if (!kept) addChange(Change::Removed, sessions_[i], sessions_[i].state, sessions_[i].state);
  }
  // Sort by slot (insertion sort; n <= 6).
  for (int i = 1; i < n; i++)
    for (int j = i; j > 0 && next[j].slot < next[j - 1].slot; j--) {
      static Session tmp;
      tmp = next[j];
      next[j] = next[j - 1];
      next[j - 1] = tmp;
    }
  for (int i = 0; i < n; i++) sessions_[i] = next[i];
  count_ = n;

  for (JsonObjectConst jm : doc["messages"].as<JsonArrayConst>()) {
    const uint32_t seq = jm["seq"] | 0u;
    if (seq <= after) continue;
    const int i = find(jm["sid"] | "");
    if (i >= 0) sessions_[i].push(seq, jm["text"] | "");
  }
  seq_ = serverSeq;
  return true;
}
