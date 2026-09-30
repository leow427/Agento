#pragma once
#include <stdint.h>

// Connection health: disconnected while Wi-Fi is down or after 3 failed polls in a row.
class Link {
 public:
  static constexpr int MAX_FAILS = 3;
  void setWifi(bool up) { wifi_ = up; }
  void pollOk() { fails_ = 0; everOk_ = true; }
  void pollFailed() { if (fails_ < MAX_FAILS) fails_++; }
  bool disconnected() const { return !wifi_ || !everOk_ || fails_ >= MAX_FAILS; }

 private:
  bool wifi_ = false;
  bool everOk_ = false;
  int fails_ = 0;
};

// Doubling delay for reconnect attempts.
class Backoff {
 public:
  Backoff(uint32_t firstMs, uint32_t maxMs) : first_(firstMs), max_(maxMs), cur_(firstMs) {}
  uint32_t next() {
    const uint32_t d = cur_;
    cur_ = cur_ >= max_ / 2 ? max_ : cur_ * 2;
    return d;
  }
  void reset() { cur_ = first_; }

 private:
  uint32_t first_, max_, cur_;
};
