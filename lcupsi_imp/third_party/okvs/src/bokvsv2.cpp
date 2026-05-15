#include "bokvsv2.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "uint.h"

namespace {

inline uint64_t SplitMix64(uint64_t x) {
  x += 0x9E3779B97F4A7C15ULL;
  x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
  x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
  return x ^ (x >> 31);
}

template <typename BandWordT>
inline void GenerateBandWord(uint128_t key, int64_t r, int64_t band_length,
                             BandWordT& out_word, int64_t& band_start) {
  const int num_blocks = static_cast<int>((band_length + 127) / 128);
  const int rem = static_cast<int>(band_length % 128);
  uint128_t high_mask;
  if (rem == 0) {
    high_mask = Uint128AllOnes();
  } else if (rem < 64) {
    high_mask = MakeUint128(0, (static_cast<uint64_t>(1) << rem) - 1);
  } else if (rem == 64) {
    high_mask = MakeUint128(0, UINT64_MAX);
  } else {
    high_mask = MakeUint128((static_cast<uint64_t>(1) << (rem - 64)) - 1,
                            UINT64_MAX);
  }

  const int64_t divisor = r - band_length + 1;
  if (divisor <= 0) {
    throw std::runtime_error("invalid OKVS params: r - w + 1 must be > 0");
  }

  const uint64_t lo = Uint128Lo(key);
  const uint64_t hi = Uint128Hi(key);
  const uint64_t selector = SplitMix64(lo ^ SplitMix64(hi));
  band_start = static_cast<int64_t>(selector % static_cast<uint64_t>(divisor));

  uint128_t blocks[6] = {0, 0, 0, 0, 0, 0};
  for (int i = 0; i < num_blocks; ++i) {
    const uint64_t h = SplitMix64(hi + static_cast<uint64_t>(2 * i + 1));
    const uint64_t l = SplitMix64(lo + static_cast<uint64_t>(2 * i + 2));
    blocks[i] = MakeUint128(h, l);
  }
  out_word.SetBlocks(blocks, num_blocks, high_mask);
}

template <typename BandWordT, typename BandAndValueT>
inline void GenerateBandAndValueDirect(uint128_t key, uint128_t value,
                                       int64_t r, int64_t band_length,
                                       BandAndValueT& out) {
  GenerateBandWord(key, r, band_length, out.band_, out.band_start_);
  out.value_ = value;
}

template <typename BandWordT>
struct BandAndValueT {
  int64_t band_start_;
  uint128_t value_;
  BandWordT band_;

  BandAndValueT() : band_start_(0), value_(0), band_() {}

  int64_t BandStart() const { return band_start_; }
  const BandWordT& RawBand() const { return band_; }
  uint128_t RawValue() const { return value_; }
};

template <typename BandWordT>
using CompactBandT = BandWordT;

template <typename BandWordT, typename BandAndValue>
bool ReduceToRowEchelon(const std::vector<BandAndValue>& bands, int64_t n,
                        std::vector<CompactBandT<BandWordT> >& reduced_matrix,
                        std::vector<uint128_t>& reduced_values, int64_t m) {
  const CompactBandT<BandWordT> kZero = CompactBandT<BandWordT>(0);

  for (int64_t i = 0; i < n; ++i) {
    int64_t offset = bands[static_cast<size_t>(i)].BandStart();
    CompactBandT<BandWordT> raw_band = bands[static_cast<size_t>(i)].RawBand();
    uint128_t value = bands[static_cast<size_t>(i)].RawValue();

    while (offset < m && reduced_matrix[static_cast<size_t>(offset)] != kZero) {
      raw_band ^= reduced_matrix[static_cast<size_t>(offset)];
      value ^= reduced_values[static_cast<size_t>(offset)];

      while (offset < m && (raw_band & 1) == 0) {
        raw_band >>= 1;
        ++offset;
      }

      if (offset >= m) {
        break;
      }
    }

    if (raw_band == kZero) {
      continue;
    }

    if (offset < m) {
      reduced_matrix[static_cast<size_t>(offset)] = raw_band;
      reduced_values[static_cast<size_t>(offset)] = value;
    }
  }

  return true;
}

template <typename BandWordT>
void Solve(const std::vector<CompactBandT<BandWordT> >& reduced_matrix,
           std::vector<uint128_t>& reduced_values, int64_t m,
           int64_t band_length) {
  const CompactBandT<BandWordT> kZero = CompactBandT<BandWordT>(0);
  for (int64_t i = m - 1; i >= 0; --i) {
    if (reduced_matrix[static_cast<size_t>(i)] == kZero) {
      continue;
    }

    uint128_t res = 0;
    const CompactBandT<BandWordT>& band = reduced_matrix[static_cast<size_t>(i)];
    for (int64_t j = 0; j < band_length && (i + j) < m; ++j) {
      if (band.GetBit(static_cast<int>(j))) {
        res ^= reduced_values[static_cast<size_t>(i + j)];
      }
    }
    reduced_values[static_cast<size_t>(i)] = res;
  }
}

template <typename BandWordT>
bool EncodeImpl(const std::vector<uint128_t>& keys,
                const std::vector<uint128_t>& values, int64_t n, int64_t m,
                int64_t r, int64_t band_length,
                std::vector<uint128_t>& p_out) {
  p_out.assign(static_cast<size_t>(m), 0);

  typedef BandAndValueT<BandWordT> BandAndValue;
  std::vector<BandAndValue> bands(static_cast<size_t>(n));
  for (int64_t i = 0; i < n; ++i) {
    GenerateBandAndValueDirect<BandWordT, BandAndValue>(
        keys[static_cast<size_t>(i)], values[static_cast<size_t>(i)], r,
        band_length, bands[static_cast<size_t>(i)]);
  }

  std::sort(bands.begin(), bands.end(),
            [](const BandAndValue& a, const BandAndValue& b) {
              return a.BandStart() < b.BandStart();
            });

  std::vector<CompactBandT<BandWordT> > reduced_matrix(static_cast<size_t>(m));
  std::vector<uint128_t> reduced_values(static_cast<size_t>(m), 0);
  if (!ReduceToRowEchelon<BandWordT>(bands, n, reduced_matrix, reduced_values,
                                     m)) {
    return false;
  }

  Solve<BandWordT>(reduced_matrix, reduced_values, m, band_length);

  for (int64_t i = 0; i < m; ++i) {
    p_out[static_cast<size_t>(i)] = reduced_values[static_cast<size_t>(i)];
  }
  return true;
}

template <typename BandWordT>
void DecodeImpl(const std::vector<uint128_t>& keys,
                const std::vector<uint128_t>& p,
                std::vector<uint128_t>& values, int64_t n, int64_t r,
                int64_t band_length) {
  values.assign(static_cast<size_t>(n), 0);
  for (int64_t i = 0; i < n; ++i) {
    BandWordT band;
    int64_t band_start = 0;
    GenerateBandWord(keys[static_cast<size_t>(i)], r, band_length, band,
                     band_start);

    uint128_t res = 0;
    for (int64_t j = 0; j < band_length; ++j) {
      if (band.GetBit(static_cast<int>(j))) {
        res ^= p[static_cast<size_t>(band_start + j)];
      }
    }
    values[static_cast<size_t>(i)] = res;
  }
}

}  // namespace

OKVSBKV2::OKVSBKV2(int64_t n, int64_t w, double e)
    : n_(n),
      m_(static_cast<int64_t>(std::ceil(n * e))),
      w_(w),
      r_(m_ - w),
      band_length_(w),
      e_(e),
      p_(static_cast<size_t>(m_), 0) {
  if (w_ > kMaxBandBits) {
    throw std::runtime_error("band length exceeds max capacity");
  }
  if (n_ <= 0 || w_ <= 0 || m_ <= 0) {
    throw std::runtime_error("invalid OKVS params");
  }
  if (r_ - band_length_ + 1 <= 0) {
    throw std::runtime_error("invalid OKVS params: m and w combination");
  }
}

bool OKVSBKV2::Encode(const std::vector<uint128_t>& keys,
                      const std::vector<uint128_t>& values) {
  if (static_cast<int64_t>(keys.size()) != n_ ||
      static_cast<int64_t>(values.size()) != n_) {
    return false;
  }

  const int num_blocks = static_cast<int>((band_length_ + 127) / 128);
  switch (num_blocks) {
    case 1:
      return EncodeImpl<band_okvs::uint<1> >(keys, values, n_, m_, r_,
                                             band_length_, p_);
    case 2:
      return EncodeImpl<band_okvs::uint<2> >(keys, values, n_, m_, r_,
                                             band_length_, p_);
    case 3:
      return EncodeImpl<band_okvs::uint<3> >(keys, values, n_, m_, r_,
                                             band_length_, p_);
    case 4:
      return EncodeImpl<band_okvs::uint<4> >(keys, values, n_, m_, r_,
                                             band_length_, p_);
    case 5:
      return EncodeImpl<band_okvs::uint<5> >(keys, values, n_, m_, r_,
                                             band_length_, p_);
    case 6:
      return EncodeImpl<band_okvs::uint<6> >(keys, values, n_, m_, r_,
                                             band_length_, p_);
    default:
      return false;
  }
}

void OKVSBKV2::Decode(const std::vector<uint128_t>& keys,
                      std::vector<uint128_t>& values) const {
  DecodeOtherP(keys, values, p_);
}

void OKVSBKV2::DecodeOtherP(const std::vector<uint128_t>& keys,
                            std::vector<uint128_t>& values,
                            const std::vector<uint128_t>& p) const {
  const int64_t n = static_cast<int64_t>(keys.size());
  const int num_blocks = static_cast<int>((band_length_ + 127) / 128);
  switch (num_blocks) {
    case 1:
      DecodeImpl<band_okvs::uint<1> >(keys, p, values, n, r_, band_length_);
      return;
    case 2:
      DecodeImpl<band_okvs::uint<2> >(keys, p, values, n, r_, band_length_);
      return;
    case 3:
      DecodeImpl<band_okvs::uint<3> >(keys, p, values, n, r_, band_length_);
      return;
    case 4:
      DecodeImpl<band_okvs::uint<4> >(keys, p, values, n, r_, band_length_);
      return;
    case 5:
      DecodeImpl<band_okvs::uint<5> >(keys, p, values, n, r_, band_length_);
      return;
    case 6:
      DecodeImpl<band_okvs::uint<6> >(keys, p, values, n, r_, band_length_);
      return;
    default:
      values.clear();
      return;
  }
}

void OKVSBKV2::DecodeDifflenP(const std::vector<uint128_t>& keys,
                              std::vector<uint128_t>& values,
                              const std::vector<uint128_t>& p) const {
  DecodeOtherP(keys, values, p);
}

void OKVSBKV2::Mul(okvs::Galois128 delta_gf128) {
  for (size_t i = 0; i < p_.size(); ++i) {
    p_[i] = (delta_gf128 * okvs::Galois128(p_[i])).get<uint128_t>(0);
  }
}
