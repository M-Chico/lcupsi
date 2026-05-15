#include <cstdint>
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

}  // namespace

int main() {
  const int64_t n = 1 << 12;
  const int64_t w = 360;
  const double e = 1.12;
  const size_t payload_bytes = 81;  // representative c_y || V_y length

  upsi::BlockSplitOkvs okvs({n, w, e, payload_bytes});

  std::vector<uint128_t> keys(static_cast<size_t>(n));
  std::vector<std::vector<uint8_t>> payloads(static_cast<size_t>(n),
                                             std::vector<uint8_t>(payload_bytes, 0));

  for (int64_t i = 0; i < n; ++i) {
    // Use canonical unique keys to satisfy OKVS distinct-key requirement.
    keys[static_cast<size_t>(i)] = MakeUint128(0, static_cast<uint64_t>(i + 1));

    for (size_t j = 0; j < payload_bytes; ++j) {
      payloads[static_cast<size_t>(i)][j] =
          static_cast<uint8_t>(SplitMix64(static_cast<uint64_t>(i) * 0x1000ULL + j) &
                               0xFF);
    }
  }

  if (!okvs.Encode(keys, payloads)) {
    std::cerr << "Encode failed\n";
    return 1;
  }

  std::vector<std::vector<uint8_t>> decoded;
  if (!okvs.Decode(keys, decoded)) {
    std::cerr << "Decode failed\n";
    return 2;
  }

  if (decoded.size() != payloads.size()) {
    std::cerr << "Decode size mismatch\n";
    return 3;
  }

  size_t mismatch = 0;
  for (size_t i = 0; i < payloads.size(); ++i) {
    if (decoded[i] != payloads[i]) {
      ++mismatch;
    }
  }

  std::cout << "n=" << n << ", payload_bytes=" << payload_bytes
            << ", block_count=" << okvs.block_count() << '\n';
  std::cout << "mismatch=" << mismatch << '\n';

  if (mismatch != 0) {
    std::cerr << "Block-split OKVS test failed\n";
    return 4;
  }

  std::cout << "Block-split OKVS test passed\n";
  return 0;
}
