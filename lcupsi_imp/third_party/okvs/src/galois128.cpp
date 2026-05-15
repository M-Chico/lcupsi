#include "galois128.h"

#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace okvs {

uint128_t cc_gf128Mul(uint128_t a, uint128_t b) {
  uint128_t z = 0;
  uint128_t v = a;
  const uint128_t mask1 = 1;
  const uint128_t mask127 = static_cast<uint128_t>(1) << 127;
  const uint128_t r = static_cast<uint128_t>(0x87);

  for (size_t i = 0; i < 128; ++i) {
    if ((b >> i) & mask1) {
      z ^= v;
    }
    if (v & mask127) {
      v <<= 1;
      v ^= r;
    } else {
      v <<= 1;
    }
  }
  return z;
}

Galois128 Galois128::Mul(const Galois128& rhs) const {
  return Galois128(cc_gf128Mul(value_, rhs.value_));
}

Galois128 Galois128::Pow(std::uint64_t i) const {
  Galois128 pow2(*this);
  Galois128 s(0, 1);
  while (i) {
    if (i & 1) {
      s = s.Mul(pow2);
    }
    pow2 = pow2.Mul(pow2);
    i >>= 1;
  }
  return s;
}

Galois128 Galois128::Inv() const {
  Galois128 a = *this;
  Galois128 result(0, 0);
  for (int64_t i = 0; i <= 6; ++i) {
    Galois128 b(a);
    for (int64_t j = 0; j < (1 << i); ++j) {
      b = b * b;
    }
    a = a * b;
    if (i == 0) {
      result = b;
    } else {
      result = result * b;
    }
  }
  if ((Mul(result).get<uint128_t>(0)) != MakeUint128(0, 1)) {
    throw std::runtime_error("Galois128::Inv verification failed");
  }
  return result;
}

}  // namespace okvs

namespace std {

std::ostream& operator<<(std::ostream& os, okvs::Galois128 x) {
  uint128_t v = x.get<uint128_t>(0);
  uint64_t hi = Uint128Hi(v);
  uint64_t lo = Uint128Lo(v);
  std::ostringstream ss;
  ss << std::hex << std::setfill('0') << std::setw(16) << hi << std::setw(16)
     << lo;
  os << ss.str();
  return os;
}

}  // namespace std
