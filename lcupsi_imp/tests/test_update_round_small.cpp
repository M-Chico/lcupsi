#include <cstdint>
#include <iostream>
#include <vector>

#include "upsi_protocol_initial.h"
#include "upsi_protocol_update.h"

namespace {

upsi::Item U64ToItem(uint64_t x) {
  upsi::Item out(8, 0);
  for (size_t i = 0; i < 8; ++i) {
    out[i] = static_cast<uint8_t>((x >> (8 * i)) & 0xFF);
  }
  return out;
}

bool RemoveOne(std::vector<upsi::Item>& xs, const upsi::Item& v) {
  for (auto it = xs.begin(); it != xs.end(); ++it) {
    if (*it == v) {
      xs.erase(it);
      return true;
    }
  }
  return false;
}

bool RunCase() {
  const size_t n = 128;
  std::vector<upsi::Item> x_set;
  std::vector<upsi::Item> y_set;
  x_set.reserve(n);
  y_set.reserve(n);

  for (size_t i = 0; i < n; ++i) {
    x_set.push_back(U64ToItem(static_cast<uint64_t>(i + 1)));
  }
  for (size_t i = 0; i < n; ++i) {
    y_set.push_back(U64ToItem(static_cast<uint64_t>(i + 1 + n / 2)));
  }

  upsi::InitialRoundConfig cfg;
  cfg.bucket_cfg = {n / 4, 0xBADC0FFEE0DDF00DULL};
  cfg.payload_cfg = {49, 32};
  cfg.pad_small_buckets = true;
  cfg.scalar_seed = 0x31415926ULL;
  cfg.h2_seed1 = 0x1234567890ABCDEFULL;
  cfg.h2_seed2 = 0x0FEDCBA098765432ULL;
  cfg.enable_okvs_encode_robust_mode = false;

  upsi::UpdateProtocolState st;
  if (!upsi::InitializeUpdateState(x_set, y_set, cfg, st)) {
    std::cerr << "InitializeUpdateState failed\n";
    return false;
  }

  std::vector<upsi::Item> add_set;
  std::vector<upsi::Item> del_set;
  for (uint64_t v = 20; v < 30; ++v) {
    add_set.push_back(U64ToItem(v));
  }
  for (uint64_t v = 100; v < 110; ++v) {
    del_set.push_back(U64ToItem(v));
  }

  upsi::UpdateRoundResult upd;
  if (!upsi::RunUpdateRound(st, add_set, del_set, upd)) {
    std::cerr << "RunUpdateRound failed\n";
    return false;
  }

  std::vector<upsi::Item> y_live = y_set;
  for (size_t i = 0; i < del_set.size(); ++i) {
    RemoveOne(y_live, del_set[i]);
  }
  for (size_t i = 0; i < add_set.size(); ++i) {
    y_live.push_back(add_set[i]);
  }

  const auto naive = upsi::NaiveIntersection(x_set, y_live);
  if (naive != upd.intersection) {
    std::cerr << "update mismatch: naive=" << naive.size()
              << " protocol=" << upd.intersection.size() << '\n';
    return false;
  }

  upsi::UpdateRoundResult noop;
  const std::vector<upsi::Item> empty;
  if (!upsi::RunUpdateRound(st, empty, empty, noop)) {
    std::cerr << "RunUpdateRound noop failed\n";
    return false;
  }
  if (noop.changed_bucket_count != 0) {
    std::cerr << "noop changed bucket count should be 0\n";
    return false;
  }
  if (noop.intersection != upd.intersection) {
    std::cerr << "noop intersection changed unexpectedly\n";
    return false;
  }

  std::cout << "update small test passed, changed_buckets="
            << upd.changed_bucket_count
            << ", intersection=" << upd.intersection.size() << '\n';
  return true;
}

}  // namespace

int main() {
  if (!RunCase()) {
    return 1;
  }
  return 0;
}

