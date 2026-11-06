#pragma once
#include <cstdint>

namespace spatial_stream {
  // Absolute 100 Hz deadlines. A late wake never becomes the next epoch:
  // rebasing on every small delay silently makes the codec slower than input.
  class ProcessClock {
    int64_t origin_ = 0;
    uint64_t rounds_ = 0;
    bool started_ = false;

  public:
    int64_t deadline(int64_t now, int64_t frequency) {
      if (!started_) {
        origin_ = now;
        started_ = true;
      }
      // Split the product to retain fractional ticks without long-run overflow.
      return origin_ + int64_t(rounds_ / 100) * frequency +
             int64_t(rounds_ % 100) * frequency / 100;
    }
    void advance() { ++rounds_; }
  };
  // Real capture is paced by the Windows audio device, whose clock can drift
  // against QPC. Follow observed quantum timestamps with a small fixed lead.
  // Warmup must not become a permanent backlog of captured game audio.
  class EncoderClock {
    ProcessClock warm_, drain_;
    int64_t firstCapture_ = 0, lastCapture_ = 0, sourceStart_ = 0, last_ = 0;
    int64_t bootstrapBegan_ = 0;
    bool sourceSettled_ = false;
  public:
    bool deadline(int64_t now, int64_t frequency, uint32_t phase,
                  int64_t captured, int64_t &target) {
      if (frequency <= 0) return false;
      if (phase == 1) {
        target = warm_.deadline(now, frequency); warm_.advance();
      } else if (phase == 2) {
        if (captured <= 0 || captured < lastCapture_ || captured > now + frequency / 4) return false;
        if (!firstCapture_) {
          firstCapture_ = captured;
          sourceStart_ = captured + frequency / 50;
          bootstrapBegan_ = now;
        }
        target = sourceStart_ + (captured - firstCapture_);
        lastCapture_ = captured;
      } else if (phase == 3) {
        const auto start = now > last_ + frequency / 100 ? now : last_ + frequency / 100;
        target = drain_.deadline(start, frequency); drain_.advance();
      } else return false;
      last_ = target;
      return true;
    }
    bool source_settled() const { return sourceSettled_; }
    bool within_budget(int64_t now, int64_t frequency, uint32_t phase, int64_t target) {
      if (phase == 2 && !sourceSettled_) {
        if (now - target <= frequency / 100) sourceSettled_ = true;
        else {
          // Preserve and encode every startup input. Catch up only during this
          // bounded pre-roll; never keep the initial warmup delay forever.
          if (now - bootstrapBegan_ > frequency * 2) return false;
          return now - target <= frequency / 2;
        }
      }
      return now - target <= frequency / 4;
    }
  };
}  // namespace spatial_stream
