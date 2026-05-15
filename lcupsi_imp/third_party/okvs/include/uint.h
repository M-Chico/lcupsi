#pragma once

#include <array>
#include <cstdint>

#include "types.h"

namespace band_okvs {

template <int Size>
class uint {
 public:
  std::array<uint128_t, Size> values_;

  uint() : values_{} {}

  template <typename... Args>
  explicit uint(Args... words) : values_{static_cast<uint128_t>(words)...} {}

  void SetBlocks(const uint128_t* blocks, int len, uint128_t mask) {
    for (int i = 0; i < Size; ++i) {
      values_[i] = (i < len) ? blocks[i] : uint128_t(0);
    }
    values_[0] |= uint128_t(1);
    values_[Size - 1] &= mask;
  }

  int GetBit(int bit_pos) const {
    int int_index = bit_pos / 128;
    int bit_index = bit_pos % 128;
    uint128_t bit = (values_[int_index] >> bit_index) & uint128_t(1);
    return static_cast<int>(static_cast<uint64_t>(bit));
  }

  uint& operator=(int v) {
    values_[0] = uint128_t(static_cast<uint64_t>(v));
    for (int i = 1; i < Size; ++i) {
      values_[i] = uint128_t(0);
    }
    return *this;
  }

  uint& operator^=(const uint& rhs) {
    *this = *this ^ rhs;
    return *this;
  }

  uint& operator>>=(int amount) {
    *this = *this >> amount;
    return *this;
  }
};

template <int Size>
inline uint<Size> operator|(uint<Size> lhs, uint<Size> rhs) {
  uint<Size> out;
  for (int i = 0; i < Size; ++i) {
    out.values_[i] = lhs.values_[i] | rhs.values_[i];
  }
  return out;
}

template <int Size>
inline uint<Size> operator&(uint<Size> lhs, uint<Size> rhs) {
  uint<Size> out;
  for (int i = 0; i < Size; ++i) {
    out.values_[i] = lhs.values_[i] & rhs.values_[i];
  }
  return out;
}

template <int Size>
inline uint128_t operator&(uint<Size> lhs, int rhs) {
  return lhs.values_[0] & uint128_t(static_cast<uint64_t>(rhs));
}

template <int Size>
inline uint<Size> operator^(uint<Size> lhs, uint<Size> rhs) {
  uint<Size> out;
  for (int i = 0; i < Size; ++i) {
    out.values_[i] = lhs.values_[i] ^ rhs.values_[i];
  }
  return out;
}

template <int Size>
inline uint<Size> operator>>(uint<Size> lhs, int amount) {
  if (amount == 0) {
    return lhs;
  }
  uint<Size> out;
  for (int i = 0; i < Size; ++i) {
    uint128_t shifted = lhs.values_[i] >> amount;
    if (i + 1 < Size) {
      shifted |= lhs.values_[i + 1] << (128 - amount);
    }
    out.values_[i] = shifted;
  }
  return out;
}

template <int Size>
inline bool operator==(uint<Size> lhs, uint<Size> rhs) {
  for (int i = 0; i < Size; ++i) {
    if (lhs.values_[i] != rhs.values_[i]) {
      return false;
    }
  }
  return true;
}

template <int Size>
inline bool operator!=(uint<Size> lhs, uint<Size> rhs) {
  return !(lhs == rhs);
}

template <int Size>
inline bool operator==(uint<Size> lhs, int rhs) {
  if (lhs.values_[0] != uint128_t(static_cast<uint64_t>(rhs))) {
    return false;
  }
  for (int i = 1; i < Size; ++i) {
    if (lhs.values_[i] != uint128_t(0)) {
      return false;
    }
  }
  return true;
}

template <int Size>
inline bool operator!=(uint<Size> lhs, int rhs) {
  return !(lhs == rhs);
}

}  // namespace band_okvs
