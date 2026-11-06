#include "../../src/spatial_audio/mat_pacing.h"
#include <iostream>
#include <stdexcept>
int main() {
  try {
    constexpr int64_t hz = 10000003, origin = 12345;
    spatial_stream::ProcessClock clock;
    int64_t legacy = origin, worst = 0;
    // Every 100th wake is 3ms late. The previous 2ms rebasing rule adds
    // 10.8 seconds/hour; the production clock must keep its original epoch.
    constexpr uint64_t rounds = 24ULL * 3600 * 100;
    for (uint64_t n = 0; n < rounds; ++n) {
      const auto expected = origin + int64_t(n / 100) * hz + int64_t(n % 100) * hz / 100;
      const auto now = expected + (n % 100 == 99 ? hz * 3 / 1000 : 0);
      if (clock.deadline(now, hz) != expected) throw std::runtime_error("process clock drift");
      clock.advance();
      // Legacy wake times follow its own shifted deadline.
      const auto legacyNow = legacy + (n % 100 == 99 ? hz * 3 / 1000 : 0);
      if (legacyNow - legacy > hz / 500) legacy = legacyNow;
      legacy += hz / 100;
      worst = legacy - (expected + hz / 100);
    }
    if (worst < hz * 200) throw std::runtime_error("legacy reproducer failed");
    std::cout << "PASS 24 simulated hours, 8640000 deadlines, fractional QPC ticks; old clock drift=" << double(worst)/hz << "s, fixed drift=0; nativeCapture=0\n";
    return 0;
  } catch(const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
