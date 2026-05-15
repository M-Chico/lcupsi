#pragma once

#include <cstdint>
#include <vector>

#include "types.h"
#include "upsi_bucket.h"

namespace upsi {

uint64_t SplitMix64(uint64_t x);
uint64_t Fnv1a64(const Item& item);

uint128_t H2ToUint128(const Item& item, uint64_t seed1, uint64_t seed2);

// Deterministic mapping to RELIC scalar domain input surrogate.
uint64_t ItemToScalar64(const Item& item, uint64_t seed);

}  // namespace upsi

