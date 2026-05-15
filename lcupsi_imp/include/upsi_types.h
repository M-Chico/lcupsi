#pragma once

#include <cstddef>
#include <cstdint>

namespace upsi {

struct PayloadConfig {
  // compressed c_y bytes (RELIC g1_write_bin(..., 1) in current setup)
  size_t cy_bytes;
  // V_y tag bytes (32 for SHA-256 full tag; 16 for 128-bit truncated tag)
  size_t vy_bytes;
};

inline size_t PayloadBytes(const PayloadConfig& cfg) {
  return cfg.cy_bytes + cfg.vy_bytes;
}

struct OkvsConfig {
  int64_t w;
  double e;
};

}  // namespace upsi

