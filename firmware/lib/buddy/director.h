#pragma once
#include <stdint.h>
#include "player.h"
#include "session_model.h"

// Drives one character's Player from its session state:
//   your_turn -> idle, working -> coding (via to_coding / to_idle poofs), needs_you -> needs_you,
//   ended -> poof_out -> gone, new session -> poof_in.
// Transitions always finish before the next one starts.
class Director {
 public:
  Director() { player_.setLoopForever(true); }

  // A newly seen session: poof in (an already-ended one just stays gone).
  void appear(SState s, uint32_t nowMs) {
    state_ = s;
    player_.play(s == SState::Ended ? ANIM_GONE : ANIM_POOF_IN, nowMs);
  }

  void setState(SState s) { state_ = s; }
  SState state() const { return state_; }

  uint16_t tick(uint32_t nowMs) {
    uint16_t f = player_.tick(nowMs);
    const uint8_t cur = player_.anim();
    const uint8_t want = goal();
    if (ANIMS[cur].loop && cur != want) {
      player_.play(route(cur, want), nowMs);
      f = player_.tick(nowMs);
    }
    return f;
  }

  uint8_t anim() const { return player_.anim(); }
  bool gone() const { return player_.anim() == ANIM_GONE; }

 private:
  uint8_t goal() const {
    switch (state_) {
      case SState::Working: return ANIM_CODING;
      case SState::NeedsYou: return ANIM_NEEDS_YOU;
      case SState::Ended: return ANIM_GONE;
      default: return ANIM_IDLE;
    }
  }

  // Next animation to play when resting in `cur` but the state wants `want`.
  static uint8_t route(uint8_t cur, uint8_t want) {
    if (cur == ANIM_GONE) return ANIM_POOF_IN;      // lands on idle
    if (cur == ANIM_CODING) return ANIM_TO_IDLE;    // always put the keyboard away first
    if (want == ANIM_CODING) return ANIM_TO_CODING;
    if (want == ANIM_GONE) return ANIM_POOF_OUT;
    return want;                                    // idle <-> needs_you: straight cut
  }

  Player player_;
  SState state_ = SState::YourTurn;
};
