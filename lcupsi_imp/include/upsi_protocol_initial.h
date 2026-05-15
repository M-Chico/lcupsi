#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "upsi_bucket.h"
#include "upsi_metrics.h"
#include "upsi_types.h"

namespace upsi {

struct InitialRoundConfig {
  BucketConfig bucket_cfg;
  PayloadConfig payload_cfg;
  bool pad_small_buckets;
  uint64_t scalar_seed;
  uint64_t h2_seed1;
  uint64_t h2_seed2;
  // Number of worker threads for bucket-level parallel evaluation.
  // 1 means sequential (default).
  size_t bucket_parallel_threads = 1;
  // Reserve additional logical slots per bucket (e.g. +128) for update tolerance.
  size_t okvs_bucket_slack = 0;
  // Strict robust mode: allows multi-level fallback with per-param retries.
  // Even when disabled, implementation still runs a lightweight self-check
  // for correctness and permits a single stronger-parameter fallback.
  bool enable_okvs_encode_robust_mode = false;
  // Valid only when robust mode is enabled.
  size_t okvs_robust_retries_per_param = 4;
  size_t okvs_robust_param_levels = 4;
  // Robust self-check optimization:
  // For large n_enc, full decode-check can be expensive. In that case,
  // switch to sampled decode-check when n_enc > okvs_robust_full_check_max_n.
  bool okvs_robust_enable_sampled_self_check = true;
  size_t okvs_robust_self_check_sample_keys = 64;
  size_t okvs_robust_full_check_max_n = 192;
};

struct InitialRoundResult {
  std::vector<Item> intersection;
};

bool RunInitialRound(const std::vector<Item>& x_set,
                     const std::vector<Item>& y_set,
                     const InitialRoundConfig& cfg,
                     InitialRoundResult& out);

bool RunInitialRoundWithStats(const std::vector<Item>& x_set,
                              const std::vector<Item>& y_set,
                              const InitialRoundConfig& cfg,
                              InitialRoundResult& out,
                              RoundEvalStats& stats);

std::vector<Item> NaiveIntersection(const std::vector<Item>& x_set,
                                    const std::vector<Item>& y_set);

}  // namespace upsi
