#include <cstdint>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "upsi_relic_bridge.h"

namespace {

uint64_t SplitMix64(uint64_t x) {
  x += 0x9E3779B97F4A7C15ULL;
  x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
  x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
  return x ^ (x >> 31);
}

bool RunCase(size_t n) {
  const size_t cy_bytes = upsi_relic_default_cy_bytes();
  const size_t vy_bytes = upsi_relic_default_v_bytes();
  const size_t payload_bytes = cy_bytes + vy_bytes;

  std::vector<uint64_t> x(n), y(n);
  for (size_t i = 0; i < n; ++i) {
    x[i] = SplitMix64(0xABCDEF1234567890ULL + i) | 1ULL;
    y[i] = x[i];
  }

  upsi_relic_bucket_ctx* ctx = upsi_relic_bucket_create(x.data(), x.size());
  if (ctx == nullptr) {
    std::cerr << "bridge create failed\n";
    return false;
  }

  std::vector<uint8_t> payloads(n * payload_bytes, 0);
  if (!upsi_relic_bucket_sign(ctx, y.data(), y.size(), payloads.data(),
                              payload_bytes, cy_bytes, vy_bytes)) {
    std::cerr << "bridge sign failed\n";
    upsi_relic_bucket_destroy(ctx);
    return false;
  }

  size_t bad = 0;
  for (size_t i = 0; i < n; ++i) {
    int is_match = 0;
    if (!upsi_relic_bucket_verify_x(ctx, i, payloads.data() + i * payload_bytes,
                                    cy_bytes, vy_bytes, &is_match)) {
      std::cerr << "bridge verify call failed at i=" << i << '\n';
      upsi_relic_bucket_destroy(ctx);
      return false;
    }
    if (!is_match) {
      ++bad;
    }
  }

  upsi_relic_bucket_destroy(ctx);
  if (bad != 0) {
    std::cerr << "bridge false negatives: " << bad << "/" << n << '\n';
    return false;
  }
  std::cout << "bridge consistency n=" << n << " ok\n";
  return true;
}

}  // namespace

int main() {
  if (!upsi_relic_global_init()) {
    std::cerr << "relic init failed\n";
    return 1;
  }

  const size_t cases[] = {2, 4, 8, 16, 32, 64, 128};
  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    if (!RunCase(cases[i])) {
      upsi_relic_global_clean();
      return 1;
    }
  }

  upsi_relic_global_clean();
  std::cout << "relic bridge consistency test passed\n";
  return 0;
}
