#include "upsi_bucket.h"

#include <algorithm>

namespace upsi {

namespace {

uint64_t SplitMix64(uint64_t x) {
  x += 0x9E3779B97F4A7C15ULL;
  x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
  x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
  return x ^ (x >> 31);
}

uint64_t Fnv1a64(const Item& item) {
  uint64_t hash = 1469598103934665603ULL;
  for (size_t i = 0; i < item.size(); ++i) {
    hash ^= static_cast<uint64_t>(item[i]);
    hash *= 1099511628211ULL;
  }
  return hash;
}

bool RemoveOne(Bucket& bucket, const Item& item) {
  for (auto it = bucket.begin(); it != bucket.end(); ++it) {
    if (*it == item) {
      bucket.erase(it);
      return true;
    }
  }
  return false;
}

}  // namespace

size_t BucketOf(const Item& item, const BucketConfig& cfg) {
  if (cfg.bucket_count == 0) {
    return 0;
  }
  const uint64_t h = SplitMix64(Fnv1a64(item) ^ cfg.seed);
  return static_cast<size_t>(h % static_cast<uint64_t>(cfg.bucket_count));
}

Buckets BuildBuckets(const std::vector<Item>& items, const BucketConfig& cfg) {
  Buckets buckets(cfg.bucket_count);
  for (size_t i = 0; i < items.size(); ++i) {
    const size_t idx = BucketOf(items[i], cfg);
    buckets[idx].push_back(items[i]);
  }
  return buckets;
}

BucketUpdateResult ApplyBucketDelta(Buckets& buckets, const std::vector<Item>& add_set,
                                    const std::vector<Item>& del_set,
                                    const BucketConfig& cfg) {
  BucketUpdateResult out;
  out.flags.assign(cfg.bucket_count, 0);

  for (size_t i = 0; i < del_set.size(); ++i) {
    const size_t idx = BucketOf(del_set[i], cfg);
    if (idx < buckets.size() && RemoveOne(buckets[idx], del_set[i])) {
      out.flags[idx] = 1;
    }
  }

  for (size_t i = 0; i < add_set.size(); ++i) {
    const size_t idx = BucketOf(add_set[i], cfg);
    if (idx < buckets.size()) {
      buckets[idx].push_back(add_set[i]);
      out.flags[idx] = 1;
    }
  }

  return out;
}

}  // namespace upsi

