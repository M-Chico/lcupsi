#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "types.h"

namespace upsi {

struct BlockSplitOkvsParams {
  int64_t n;
  int64_t w;
  double e;
  size_t payload_bytes;
};

// Logical single OKVS value with payload_bytes width, implemented by
// splitting payload into 128-bit blocks and encoding each block in one table.
class BlockSplitOkvs {
 public:
  explicit BlockSplitOkvs(const BlockSplitOkvsParams& params);

  bool Encode(const std::vector<uint128_t>& keys,
              const std::vector<std::vector<uint8_t>>& payloads);

  bool Decode(const std::vector<uint128_t>& keys,
              std::vector<std::vector<uint8_t>>& payloads_out) const;

  size_t block_count() const { return block_count_; }
  int64_t table_n() const { return n_; }
  int64_t table_w() const { return w_; }
  int64_t table_m() const { return m_; }
  double table_e() const { return e_; }
  size_t payload_bytes() const { return payload_bytes_; }

  const std::vector<std::vector<uint128_t>>& tables() const { return tables_; }
  void SetTables(const std::vector<std::vector<uint128_t>>& tables);

 private:
  static uint128_t BytesToBlock(const uint8_t* src, size_t len);
  static void BlockToBytes(uint128_t block, uint8_t* dst, size_t len);

  int64_t n_;
  int64_t w_;
  int64_t m_;
  double e_;
  size_t payload_bytes_;
  size_t block_count_;
  std::vector<std::vector<uint128_t>> tables_;
};

}  // namespace upsi

