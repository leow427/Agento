#pragma once
// Mirror of AgentFunnel's GET /state: sessions (mirrored each poll) and their last messages.
// No Arduino dependencies, so it is unit-tested natively.
#include <stddef.h>
#include <stdint.h>

constexpr int MAX_SESSIONS = 6;
constexpr int MAX_MSGS = 20;
constexpr size_t ID_BYTES = 64;
constexpr size_t PROJECT_BYTES = 32;
constexpr size_t MSG_BYTES = 320;  // texts are <= 200 code points; longer UTF-8 is cut on a boundary

enum class SState : uint8_t { YourTurn, Working, NeedsYou, Ended };
const char* stateName(SState s);

struct Msg {
  uint32_t seq;
  char text[MSG_BYTES];
};

struct Session {
  char id[ID_BYTES];
  uint8_t slot;
  char project[PROJECT_BYTES];
  SState state;
  uint8_t msgCount;
  uint8_t msgHead;  // index of the oldest message in the ring
  Msg msgs[MAX_MSGS];
  const Msg& msg(int i) const { return msgs[(msgHead + i) % MAX_MSGS]; }  // 0 = oldest
  void push(uint32_t seq, const char* text);
};

struct Change {
  enum Kind : uint8_t { Appeared, Removed, StateChanged } kind;
  char id[ID_BYTES];
  uint8_t slot;
  SState from, to;
};

class Model {
 public:
  // Applies one /state response. Returns false (and changes nothing) on bad JSON or shape.
  bool apply(const char* json, size_t len);
  uint32_t seq() const { return seq_; }
  int count() const { return count_; }
  const Session& at(int i) const { return sessions_[i]; }  // sorted by slot
  int find(const char* id) const;
  int changeCount() const { return changeCount_; }
  const Change& change(int i) const { return changes_[i]; }

 private:
  Session sessions_[MAX_SESSIONS] = {};
  int count_ = 0;
  uint32_t seq_ = 0;
  Change changes_[2 * MAX_SESSIONS] = {};
  int changeCount_ = 0;
  void addChange(Change::Kind k, const Session& s, SState from, SState to);
};

// Copies src into dst (size n), cutting on a UTF-8 code point boundary.
void copyUtf8(char* dst, const char* src, size_t n);
