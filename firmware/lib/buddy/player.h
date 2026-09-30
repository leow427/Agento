#pragma once
#include <stdint.h>
#include "frames_data.h"

// Mirrors tools/player.py. Timestamps are uint32_t milliseconds; all comparisons use
// unsigned subtraction so millis() wraparound is harmless.
class Player {
 public:
  explicit Player(uint32_t nowMs = 0) { start(ANIM_START, nowMs); }

  // Advance to `nowMs` and return the frame index to show (an index into FRAMES).
  uint16_t tick(uint32_t nowMs) {
    for (;;) {
      const Anim& a = ANIMS[anim_];
      const uint32_t ms = a.steps[step_].ms;
      if (nowMs - stepStart_ < ms) break;
      stepStart_ += ms;
      if (++step_ < a.count) continue;
      step_ = 0;
      const bool holding = a.loop && (loopForever_ || (stepStart_ - animStart_) < a.holdMs);
      if (!holding) {
        anim_ = a.next;
        animStart_ = stepStart_;
      }
    }
    return ANIMS[anim_].steps[step_].frame;
  }

  void toggle(uint32_t nowMs) {
    if (anim_ == ANIM_IDLE) start(ANIM_TO_CODING, nowMs);
    else if (anim_ == ANIM_CODING) start(ANIM_TO_IDLE, nowMs);
  }

  void play(uint8_t anim, uint32_t nowMs) { start(anim, nowMs); }

  // Wearable mode: looping animations never hand off to `next` on their own.
  void setLoopForever(bool on) { loopForever_ = on; }

  uint8_t anim() const { return anim_; }

 private:
  void start(uint8_t anim, uint32_t nowMs) {
    anim_ = anim;
    step_ = 0;
    stepStart_ = nowMs;
    animStart_ = nowMs;
  }

  uint8_t anim_ = ANIM_START;
  uint8_t step_ = 0;
  uint32_t stepStart_ = 0;
  uint32_t animStart_ = 0;
  bool loopForever_ = false;
};
