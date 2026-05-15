#include <cstdint>
#include <iomanip>
#include <iostream>
#include <vector>

#include "types.h"
#include "upsi_okvs_adapter.h"

namespace {

uint64_t SplitMix64(uint64_t x) {
  x += 0x9E3779B97F4A7C15ULL;
  x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
  x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
  return x ^ (x >> 31);
}

size_t RunOne(int64_t n, int64_t w, double e, size_t payload_bytes,
              uint64_t seed_bias) {
  upsi::BlockSplitOkvs okvs({n, w, e, payload_bytes});
  std::vector<uint128_t> keys(static_cast<size_t>(n));
  std::vector<std::vector<uint8_t>> payloads(
      static_cast<size_t>(n), std::vector<uint8_t>(payload_bytes, 0));

  // Distinct keys are required by OKVS.
  for (int64_t i = 0; i < n; ++i) {
    keys[static_cast<size_t>(i)] = MakeUint128(0, static_cast<uint64_t>(i + 1));
    for (size_t j = 0; j < payload_bytes; ++j) {
      payloads[static_cast<size_t>(i)][j] = static_cast<uint8_t>(
          SplitMix64(seed_bias + static_cast<uint64_t>(i) * 0x10000ULL + j) &
          0xFF);
    }
  }

  if (!okvs.Encode(keys, payloads)) {
    return static_cast<size_t>(-1);
  }

  std::vector<std::vector<uint8_t>> decoded;
  if (!okvs.Decode(keys, decoded)) {
    return static_cast<size_t>(-1);
  }
  if (decoded.size() != payloads.size()) {
    return static_cast<size_t>(-1);
  }

  size_t mismatch = 0;
  for (size_t i = 0; i < payloads.size(); ++i) {
    if (decoded[i] != payloads[i]) {
      ++mismatch;
    }
  }
  return mismatch;
}

}  // namespace

int main() {
  const size_t payload_bytes = 81;
  const int trials = 3;
  const int64_t ns[] = {32, 64, 128, 256, 512, 1024, 2048, 4096};
  const int64_t ws[] = {4,  8,  12, 16, 24, 32, 48, 64,
                        96, 120, 180, 240, 300, 360, 420};
  const double es[] = {1.12, 1.18, 1.25, 1.35, 1.50, 1.75, 2.00, 2.50, 3.00};

  std::cout << "OKVS small-n calibration (payload_bytes=" << payload_bytes
            << ", trials=" << trials << ")\n";

  bool all_found = true;
  for (size_t ni = 0; ni < sizeof(ns) / sizeof(ns[0]); ++ni) {
    const int64_t n = ns[ni];
    bool found = false;

    std::cout << "\n[n=" << n << "]\n";
    for (size_t ei = 0; ei < sizeof(es) / sizeof(es[0]) && !found; ++ei) {
      for (size_t wi = 0; wi < sizeof(ws) / sizeof(ws[0]) && !found; ++wi) {
        const double e = es[ei];
        const int64_t w = ws[wi];
        const int64_t m_est = static_cast<int64_t>(n * e + 0.999999);
        if (m_est - 2 * w + 1 <= 0) {
          continue;
        }
        size_t worst = 0;
        bool fail = false;
        for (int t = 0; t < trials; ++t) {
          size_t mismatch = static_cast<size_t>(-1);
          try {
            mismatch = RunOne(n, w, e, payload_bytes,
                              0xABCDEFULL + static_cast<uint64_t>(t));
          } catch (...) {
            fail = true;
            break;
          }
          if (mismatch == static_cast<size_t>(-1)) {
            fail = true;
            break;
          }
          if (mismatch > worst) {
            worst = mismatch;
          }
        }

        std::cout << "  try e=" << std::fixed << std::setprecision(2) << e
                  << ", w=" << w << " -> ";
        if (fail) {
          std::cout << "encode/decode failure\n";
          continue;
        }
        std::cout << "worst_mismatch=" << worst << "\n";
        if (worst == 0) {
          std::cout << "  selected: e=" << std::fixed << std::setprecision(2) << e
                    << ", w=" << w << "\n";
          found = true;
        }
      }
    }

    if (!found) {
      all_found = false;
      std::cout << "  no zero-mismatch config found in current candidate grid\n";
    }
  }

  if (!all_found) {
    std::cerr << "Calibration incomplete: expand candidate grid.\n";
    return 1;
  }

  std::cout << "\nCalibration completed with zero-mismatch configs for all n.\n";
  return 0;
}
