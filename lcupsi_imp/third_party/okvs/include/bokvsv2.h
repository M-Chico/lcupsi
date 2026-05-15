#pragma once

#include <cstdint>
#include <vector>

#include "galois128.h"
#include "types.h"

class OKVSBKV2 {
 public:
  OKVSBKV2(int64_t n, int64_t w, double e);

  int64_t getN() const { return n_; }
  int64_t getM() const { return m_; }
  int64_t getW() const { return w_; }
  int64_t getR() const { return r_; }
  double getE() const { return e_; }

  bool Encode(const std::vector<uint128_t>& keys,
              const std::vector<uint128_t>& values);
  void Decode(const std::vector<uint128_t>& keys,
              std::vector<uint128_t>& values) const;
  void DecodeOtherP(const std::vector<uint128_t>& keys,
                    std::vector<uint128_t>& values,
                    const std::vector<uint128_t>& p) const;
  void DecodeDifflenP(const std::vector<uint128_t>& keys,
                      std::vector<uint128_t>& values,
                      const std::vector<uint128_t>& p) const;
  void Mul(okvs::Galois128 delta_gf128);

 private:
  static constexpr int64_t kMaxBandBits = 128 * 6;

  int64_t n_;
  int64_t m_;
  int64_t w_;
  int64_t r_;
  int64_t band_length_;
  double e_;

 public:
  std::vector<uint128_t> p_;
};
