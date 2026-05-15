#include <iostream>
#include <string>
#include <vector>

#include "upsi_bucket.h"

namespace {

upsi::Item ToItem(const std::string& s) {
  return upsi::Item(s.begin(), s.end());
}

bool BucketsEqual(const upsi::Buckets& a, const upsi::Buckets& b) {
  return a == b;
}

}  // namespace

int main() {
  const upsi::BucketConfig cfg{8, 0xBADC0FFEE0DDF00DULL};

  std::vector<upsi::Item> base = {
      ToItem("alice"), ToItem("bob"), ToItem("carol"), ToItem("dave"),
      ToItem("eve"),   ToItem("frank"), ToItem("grace"), ToItem("heidi")};

  upsi::Buckets buckets = upsi::BuildBuckets(base, cfg);
  upsi::Buckets expected = buckets;

  std::vector<upsi::Item> add_set = {ToItem("ivan"), ToItem("judy")};
  std::vector<upsi::Item> del_set = {ToItem("alice"), ToItem("frank")};

  const auto update = upsi::ApplyBucketDelta(buckets, add_set, del_set, cfg);

  // Build expected result with the same deterministic hash rules.
  for (size_t i = 0; i < del_set.size(); ++i) {
    const size_t idx = upsi::BucketOf(del_set[i], cfg);
    auto& bucket = expected[idx];
    for (auto it = bucket.begin(); it != bucket.end(); ++it) {
      if (*it == del_set[i]) {
        bucket.erase(it);
        break;
      }
    }
  }
  for (size_t i = 0; i < add_set.size(); ++i) {
    const size_t idx = upsi::BucketOf(add_set[i], cfg);
    expected[idx].push_back(add_set[i]);
  }

  if (!BucketsEqual(buckets, expected)) {
    std::cerr << "Bucket state mismatch after delta\n";
    return 1;
  }

  size_t changed = 0;
  for (size_t i = 0; i < update.flags.size(); ++i) {
    if (update.flags[i]) {
      ++changed;
    }
  }
  if (changed == 0) {
    std::cerr << "Expected at least one changed bucket\n";
    return 2;
  }

  std::cout << "bucket_count=" << cfg.bucket_count << ", changed=" << changed
            << '\n';
  std::cout << "Bucketization update test passed\n";
  return 0;
}

