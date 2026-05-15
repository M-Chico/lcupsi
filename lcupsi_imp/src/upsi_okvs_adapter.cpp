#include "upsi_okvs_adapter.h"

#include <algorithm>

#include "bokvsv2.h"

namespace upsi {

namespace {

constexpr size_t kBlockBytes = 16;

}  // namespace

BlockSplitOkvs::BlockSplitOkvs(const BlockSplitOkvsParams& params)
    : n_(params.n),
      w_(params.w),
      m_(0),
      e_(params.e),
      payload_bytes_(params.payload_bytes),
      block_count_((params.payload_bytes + kBlockBytes - 1) / kBlockBytes),
      tables_(block_count_) {}

uint128_t BlockSplitOkvs::BytesToBlock(const uint8_t* src, size_t len) {
  uint64_t lo = 0;
  uint64_t hi = 0;
  const size_t lo_len = std::min<size_t>(len, 8);
  for (size_t i = 0; i < lo_len; ++i) {
    lo |= static_cast<uint64_t>(src[i]) << (8 * i);
  }
  if (len > 8) {
    const size_t hi_len = std::min<size_t>(len - 8, 8);
    for (size_t i = 0; i < hi_len; ++i) {
      hi |= static_cast<uint64_t>(src[8 + i]) << (8 * i);
    }
  }
  return MakeUint128(hi, lo);
}

void BlockSplitOkvs::BlockToBytes(uint128_t block, uint8_t* dst, size_t len) {
  const uint64_t lo = Uint128Lo(block);
  const uint64_t hi = Uint128Hi(block);
  const size_t lo_len = std::min<size_t>(len, 8);
  for (size_t i = 0; i < lo_len; ++i) {
    dst[i] = static_cast<uint8_t>((lo >> (8 * i)) & 0xFF);
  }
  if (len > 8) {
    const size_t hi_len = std::min<size_t>(len - 8, 8);
    for (size_t i = 0; i < hi_len; ++i) {
      dst[8 + i] = static_cast<uint8_t>((hi >> (8 * i)) & 0xFF);
    }
  }
}

bool BlockSplitOkvs::Encode(
    const std::vector<uint128_t>& keys,
    const std::vector<std::vector<uint8_t>>& payloads) {
  if (keys.size() != static_cast<size_t>(n_) ||
      payloads.size() != static_cast<size_t>(n_)) {
    return false;
  }
  for (size_t i = 0; i < payloads.size(); ++i) {
    if (payloads[i].size() != payload_bytes_) {
      return false;
    }
  }

  for (size_t b = 0; b < block_count_; ++b) {
    const size_t offset = b * kBlockBytes;
    const size_t block_len = std::min(kBlockBytes, payload_bytes_ - offset);
    std::vector<uint128_t> values(static_cast<size_t>(n_));
    for (size_t i = 0; i < payloads.size(); ++i) {
      values[i] = BytesToBlock(payloads[i].data() + offset, block_len);
    }

    OKVSBKV2 okvs(n_, w_, e_);
    if (!okvs.Encode(keys, values)) {
      return false;
    }
    if (m_ == 0) {
      m_ = okvs.getM();
    }
    tables_[b] = okvs.p_;
  }
  return true;
}

bool BlockSplitOkvs::Decode(const std::vector<uint128_t>& keys,
                            std::vector<std::vector<uint8_t>>& payloads_out) const {
  if (block_count_ == 0) {
    payloads_out.clear();
    return true;
  }
  if (tables_.empty() || tables_.size() != block_count_) {
    return false;
  }
  for (size_t b = 0; b < block_count_; ++b) {
    if (tables_[b].empty()) {
      return false;
    }
  }

  payloads_out.assign(keys.size(), std::vector<uint8_t>(payload_bytes_, 0));

  for (size_t b = 0; b < block_count_; ++b) {
    OKVSBKV2 okvs(n_, w_, e_);
    std::vector<uint128_t> decoded_blocks;
    okvs.DecodeOtherP(keys, decoded_blocks, tables_[b]);
    if (decoded_blocks.size() != keys.size()) {
      return false;
    }

    const size_t offset = b * kBlockBytes;
    const size_t block_len = std::min(kBlockBytes, payload_bytes_ - offset);
    for (size_t i = 0; i < decoded_blocks.size(); ++i) {
      BlockToBytes(decoded_blocks[i], payloads_out[i].data() + offset, block_len);
    }
  }
  return true;
}

void BlockSplitOkvs::SetTables(const std::vector<std::vector<uint128_t>>& tables) {
  tables_ = tables;
}

}  // namespace upsi

