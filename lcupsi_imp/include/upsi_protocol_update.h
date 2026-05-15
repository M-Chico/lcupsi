#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "upsi_bucket.h"
#include "upsi_protocol_initial.h"

namespace upsi {

struct UpdateProtocolState {
  InitialRoundConfig cfg;
  Buckets x_buckets;
  Buckets y_buckets;
  std::vector<Item> intersection;
};

struct UpdateRoundResult {
  std::vector<uint8_t> changed_bucket_flags;
  size_t changed_bucket_count = 0;
  std::vector<Item> intersection;
};

bool InitializeUpdateState(const std::vector<Item>& x_set,
                           const std::vector<Item>& y_set,
                           const InitialRoundConfig& cfg,
                           UpdateProtocolState& out_state);

bool RunUpdateRound(UpdateProtocolState& state, const std::vector<Item>& add_set,
                    const std::vector<Item>& del_set, UpdateRoundResult& out);

bool RunUpdateRoundWithStats(UpdateProtocolState& state,
                             const std::vector<Item>& add_set,
                             const std::vector<Item>& del_set,
                             UpdateRoundResult& out, RoundEvalStats& stats);

}  // namespace upsi
