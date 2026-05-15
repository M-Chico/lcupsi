#pragma once

#include <cstddef>
#include <cstdint>

#include "upsi_types.h"

namespace upsi {

// Empirically calibrated minimum stable encoded cardinality.
// For bucket sizes below this threshold, pad with random dummies before OKVS.
constexpr size_t kOkvsMinStableN = 32;

inline size_t PaddedOkvsN(size_t bucket_n) {
  return bucket_n < kOkvsMinStableN ? kOkvsMinStableN : bucket_n;
}

inline OkvsConfig SelectOkvsConfigByN(size_t padded_n) {
  if (padded_n <= 32) {
    return {8, 3.00};
  }
  if (padded_n <= 64) {
    return {12, 1.35};
  }
  if (padded_n <= 128) {
    return {16, 1.25};
  }
  if (padded_n <= 256) {
    return {24, 1.18};
  }
  if (padded_n <= 2048) {
    return {24, 1.12};
  }
  if (padded_n <= 4096) {
    return {48, 1.12};
  }
  return {64, 1.12};
}

inline bool IsOkvsParamShapeValid(size_t n, const OkvsConfig& cfg) {
  // Matches current OKVS constructor validity check:
  // m = ceil(n * e), requires m - 2w + 1 > 0.
  const int64_t m = static_cast<int64_t>(n * cfg.e + 0.999999);
  return (m - 2 * cfg.w + 1) > 0;
}

}  // namespace upsi
