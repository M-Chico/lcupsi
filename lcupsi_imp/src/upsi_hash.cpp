#include "upsi_hash.h"

namespace upsi {

uint64_t SplitMix64(uint64_t x) {
  x += 0x9E3779B97F4A7C15ULL;
  x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
  x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
  return x ^ (x >> 31);
}

uint64_t Fnv1a64(const Item& item) {
  uint64_t hash = 1469598103934665603ULL;
  for (size_t i = 0; i < item.size(); ++i) {
    hash ^= static_cast<uint64_t>(item[i]);
    hash *= 1099511628211ULL;
  }
  return hash;
}

uint128_t H2ToUint128(const Item& item, uint64_t seed1, uint64_t seed2) {
  const uint64_t base = Fnv1a64(item);
  const uint64_t hi = SplitMix64(base ^ seed1);
  const uint64_t lo = SplitMix64(base ^ seed2);
  return MakeUint128(hi, lo);
}

uint64_t ItemToScalar64(const Item& item, uint64_t seed) {
  // Keep non-zero to reduce trivial corner cases.
  return SplitMix64(Fnv1a64(item) ^ seed) | 1ULL;
}

}  // namespace upsi

