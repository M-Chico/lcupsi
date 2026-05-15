#pragma once

#include <cstddef>

namespace upsi {

struct BucketEvalStats {
  size_t nx = 0;
  size_t ny = 0;
  size_t n_enc = 0;
  size_t payload_bytes = 0;
  size_t okvs_block_count = 0;
  size_t okvs_table_m = 0;
  size_t okvs_table_bytes = 0;
  size_t match_count = 0;
  size_t okvs_encode_attempts = 0;
  size_t okvs_self_checks = 0;
  size_t okvs_self_checks_full = 0;
  size_t okvs_self_checks_sampled = 0;

  // Communication decomposition for this bucket.
  size_t p0_to_p1_ci_bytes = 0;
  size_t p1_to_p0_okvs_bytes = 0;
  size_t p0_to_p1_result_bytes = 0;

  // Legacy totals.
  size_t p0_to_p1_bytes = 0;
  size_t p1_to_p0_bytes = 0;
  size_t total_comm_bytes = 0;

  // Timing decomposition for this bucket (local simulation).
  double p0_bucket_create_ms = 0.0;
  double p1_sign_ms = 0.0;
  double p1_okvs_encode_ms = 0.0;
  double p0_okvs_decode_ms = 0.0;
  double p0_verify_ms = 0.0;
  // P1 online window: from receiving first P0 ciphertext (Ci)
  // to receiving final P0 result message for this bucket.
  double p1_online_window_ms = 0.0;

  // Legacy online value, equals p1_online_window_ms.
  double p1_online_ms = 0.0;
  double bucket_total_ms = 0.0;
};

struct RoundEvalStats {
  // Total round wall-clock.
  double total_ms = 0.0;
  // Stage decomposition.
  double bucketize_ms = 0.0;
  double delta_apply_ms = 0.0;
  double unchanged_filter_ms = 0.0;
  double sort_unique_ms = 0.0;
  double relic_init_ms = 0.0;
  double relic_clean_ms = 0.0;
  double eval_total_ms = 0.0;

  // Per-bucket internal decomposition aggregated over processed buckets.
  double p0_bucket_create_ms = 0.0;
  double p1_sign_ms = 0.0;
  double p1_okvs_encode_ms = 0.0;
  double p0_okvs_decode_ms = 0.0;
  double p0_verify_ms = 0.0;
  // Workload counters for per-item normalization.
  size_t p1_sign_item_count = 0;
  size_t p0_verify_item_count = 0;

  // P1 online window over the round:
  // from first received Ci in this round to last P0 message in this round.
  double p1_online_ms = 0.0;

  // Communication decomposition.
  size_t p0_to_p1_ci_bytes = 0;
  size_t p1_to_p0_okvs_bytes = 0;
  size_t p0_to_p1_result_bytes = 0;
  size_t p0_to_p1_flag_bytes = 0;

  // Legacy totals.
  size_t p0_to_p1_bytes = 0;
  size_t p1_to_p0_bytes = 0;
  size_t total_comm_bytes = 0;

  // Robust OKVS behavior counters.
  size_t okvs_encode_attempts = 0;
  size_t okvs_self_checks = 0;
  size_t okvs_self_checks_full = 0;
  size_t okvs_self_checks_sampled = 0;

  size_t changed_bucket_count = 0;
};

}  // namespace upsi
