#include <cstdint>
#include <iostream>
#include <vector>

#include "upsi_protocol_initial.h"

namespace {

upsi::Item U64ToItem(uint64_t x) {
  upsi::Item out(8, 0);
  for (size_t i = 0; i < 8; ++i) {
    out[i] = static_cast<uint8_t>((x >> (8 * i)) & 0xFF);
  }
  return out;
}

bool RunCase(size_t n) {
  std::vector<upsi::Item> x_set;
  std::vector<upsi::Item> y_set;
  x_set.reserve(n);
  y_set.reserve(n);

  for (size_t i = 0; i < n; ++i) {
    x_set.push_back(U64ToItem(static_cast<uint64_t>(i + 1)));
  }
  for (size_t i = 0; i < n; ++i) {
    y_set.push_back(U64ToItem(static_cast<uint64_t>(i + 1 + n / 2)));
  }

  upsi::InitialRoundConfig cfg;
  cfg.bucket_cfg = {n / 4, 0xBADC0FFEE0DDF00DULL};  // force small bucket sizes
  cfg.payload_cfg = {49, 32};
  cfg.pad_small_buckets = true;
  cfg.scalar_seed = 0x31415926ULL;
  cfg.h2_seed1 = 0x1234567890ABCDEFULL;
  cfg.h2_seed2 = 0x0FEDCBA098765432ULL;

  upsi::InitialRoundResult protocol;
  if (!upsi::RunInitialRound(x_set, y_set, cfg, protocol)) {
    std::cerr << "RunInitialRound failed for n=" << n << '\n';
    return false;
  }

  const auto naive = upsi::NaiveIntersection(x_set, y_set);
  if (naive != protocol.intersection) {
    std::cerr << "Mismatch for n=" << n << ", naive=" << naive.size()
              << ", protocol=" << protocol.intersection.size() << '\n';
    return false;
  }

  std::cout << "n=" << n << ", intersection=" << protocol.intersection.size()
            << " (ok)\n";
  return true;
}

}  // namespace

int main() {
  const size_t cases[] = {64, 128, 256};
  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    if (!RunCase(cases[i])) {
      return 1;
    }
  }
  std::cout << "Initial round small-set test passed\n";
  return 0;
}

