#pragma once

#include <cstdint>

struct uint128_t {
  uint64_t hi;
  uint64_t lo;

  constexpr uint128_t() : hi(0), lo(0) {}
  constexpr uint128_t(uint64_t low) : hi(0), lo(low) {}
  constexpr uint128_t(uint64_t high, uint64_t low) : hi(high), lo(low) {}

  explicit constexpr operator bool() const { return hi != 0 || lo != 0; }
  explicit constexpr operator uint64_t() const { return lo; }

  uint128_t& operator^=(const uint128_t& rhs) {
    hi ^= rhs.hi;
    lo ^= rhs.lo;
    return *this;
  }

  uint128_t& operator|=(const uint128_t& rhs) {
    hi |= rhs.hi;
    lo |= rhs.lo;
    return *this;
  }

  uint128_t& operator&=(const uint128_t& rhs) {
    hi &= rhs.hi;
    lo &= rhs.lo;
    return *this;
  }

  uint128_t& operator<<=(int s);
  uint128_t& operator>>=(int s);
};

inline constexpr bool operator==(const uint128_t& a, const uint128_t& b) {
  return a.hi == b.hi && a.lo == b.lo;
}

inline constexpr bool operator!=(const uint128_t& a, const uint128_t& b) {
  return !(a == b);
}

inline constexpr uint128_t operator^(const uint128_t& a, const uint128_t& b) {
  return uint128_t(a.hi ^ b.hi, a.lo ^ b.lo);
}

inline constexpr uint128_t operator|(const uint128_t& a, const uint128_t& b) {
  return uint128_t(a.hi | b.hi, a.lo | b.lo);
}

inline constexpr uint128_t operator&(const uint128_t& a, const uint128_t& b) {
  return uint128_t(a.hi & b.hi, a.lo & b.lo);
}

inline constexpr uint128_t operator<<(const uint128_t& v, int s) {
  if (s <= 0) {
    return v;
  }
  if (s >= 128) {
    return uint128_t(0, 0);
  }
  if (s >= 64) {
    return uint128_t(v.lo << (s - 64), 0);
  }
  return uint128_t((v.hi << s) | (v.lo >> (64 - s)), v.lo << s);
}

inline constexpr uint128_t operator>>(const uint128_t& v, int s) {
  if (s <= 0) {
    return v;
  }
  if (s >= 128) {
    return uint128_t(0, 0);
  }
  if (s >= 64) {
    return uint128_t(0, v.hi >> (s - 64));
  }
  return uint128_t(v.hi >> s, (v.lo >> s) | (v.hi << (64 - s)));
}

inline uint128_t& uint128_t::operator<<=(int s) {
  *this = *this << s;
  return *this;
}

inline uint128_t& uint128_t::operator>>=(int s) {
  *this = *this >> s;
  return *this;
}

inline uint128_t MakeUint128(uint64_t hi, uint64_t lo) {
  return uint128_t(hi, lo);
}

inline uint64_t Uint128Hi(uint128_t v) {
  return v.hi;
}

inline uint64_t Uint128Lo(uint128_t v) {
  return v.lo;
}

inline uint128_t Uint128AllOnes() {
  return uint128_t(UINT64_MAX, UINT64_MAX);
}
