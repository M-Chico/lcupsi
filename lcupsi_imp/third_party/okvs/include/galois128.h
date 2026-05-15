#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ostream>

#include "types.h"

namespace okvs {

uint128_t cc_gf128Mul(uint128_t a, uint128_t b);

class Galois128 {
 public:
  Galois128(uint64_t hi, uint64_t lo) : value_(MakeUint128(hi, lo)) {}
  explicit Galois128(uint64_t v) : value_(MakeUint128(0, v)) {}
  explicit Galois128(uint128_t v) : value_(v) {}

  Galois128 Mul(const Galois128& rhs) const;
  Galois128 Pow(std::uint64_t i) const;
  Galois128 Inv() const;

  inline Galois128 operator*(const Galois128& rhs) const { return Mul(rhs); }

  inline const uint8_t* data() const {
    return reinterpret_cast<const uint8_t*>(&value_);
  }

  template <typename T>
  T get(size_t index) const {
    T output;
    std::memcpy(&output, data() + sizeof(T) * index, sizeof(T));
    return output;
  }

 private:
  uint128_t value_;
};

}  // namespace okvs

namespace std {

std::ostream& operator<<(std::ostream& os, okvs::Galois128 x);

}  // namespace std
