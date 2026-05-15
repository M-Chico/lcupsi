#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace upsi {

using Item = std::vector<uint8_t>;
using Bucket = std::vector<Item>;
using Buckets = std::vector<Bucket>;

struct BucketConfig {
  size_t bucket_count;
  uint64_t seed;
};

size_t BucketOf(const Item& item, const BucketConfig& cfg);
Buckets BuildBuckets(const std::vector<Item>& items, const BucketConfig& cfg);

struct BucketUpdateResult {
  std::vector<uint8_t> flags;  // 1 means this bucket changed in this round.
};

BucketUpdateResult ApplyBucketDelta(Buckets& buckets, const std::vector<Item>& add_set,
                                    const std::vector<Item>& del_set,
                                    const BucketConfig& cfg);

}  // namespace upsi

