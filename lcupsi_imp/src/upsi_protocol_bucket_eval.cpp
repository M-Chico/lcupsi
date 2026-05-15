#include "upsi_protocol_bucket_eval.h"

#include <algorithm>
#include <cstring>
#include <vector>

#include "upsi_hash.h"
#include "upsi_okvs_adapter.h"
#include "upsi_okvs_param_policy.h"
#include "upsi_relic_bridge.h"
#include "upsi_timer.h"

namespace upsi {

namespace {

bool ContainsKeyPrefix(const std::vector<uint128_t>& keys, size_t count, uint128_t k) {
  for (size_t i = 0; i < count; ++i) {
    if (keys[i] == k) {
      return true;
    }
  }
  return false;
}

bool ContainsIndexPrefix(const std::vector<size_t>& xs, size_t count, size_t v) {
  for (size_t i = 0; i < count; ++i) {
    if (xs[i] == v) {
      return true;
    }
  }
  return false;
}

std::vector<size_t> BuildSelfCheckSampleIndices(size_t n_enc, size_t ny,
                                                size_t sample_cap,
                                                uint64_t seed) {
  std::vector<size_t> idx;
  if (n_enc == 0) {
    return idx;
  }
  const size_t target = std::max<size_t>(1, std::min(n_enc, sample_cap));
  idx.reserve(target);

  const size_t take_real = std::min(ny, target);
  for (size_t i = 0; i < take_real; ++i) {
    idx.push_back(i);
  }

  uint64_t ctr = seed ^ 0x9E3779B97F4A7C15ULL;
  while (idx.size() < target) {
    ++ctr;
    size_t cand = 0;
    if (ny < n_enc) {
      const size_t tail = n_enc - ny;
      cand = ny + static_cast<size_t>(SplitMix64(ctr) % tail);
    } else {
      cand = static_cast<size_t>(SplitMix64(ctr) % n_enc);
    }
    if (!ContainsIndexPrefix(idx, idx.size(), cand)) {
      idx.push_back(cand);
    }
  }
  return idx;
}

}  // namespace

bool EvalBucketIntersectionNoInit(const Bucket& x_bucket, const Bucket& y_bucket,
                                  const InitialRoundConfig& cfg,
                                  std::vector<Item>& out_intersection,
                                  BucketEvalStats* stats) {
  const double t_bucket_begin_ms = HighResNowMs();

  const size_t nx = x_bucket.size();
  const size_t ny = y_bucket.size();
  if (stats != nullptr) {
    *stats = BucketEvalStats{};
    stats->nx = nx;
    stats->ny = ny;
  }
  if (nx == 0 || ny == 0) {
    if (stats != nullptr) {
      stats->bucket_total_ms = HighResNowMs() - t_bucket_begin_ms;
    }
    return true;
  }

  const size_t cy_bytes =
      cfg.payload_cfg.cy_bytes == 0 ? upsi_relic_default_cy_bytes()
                                    : cfg.payload_cfg.cy_bytes;
  const size_t vy_bytes =
      cfg.payload_cfg.vy_bytes == 0 ? upsi_relic_default_v_bytes()
                                    : cfg.payload_cfg.vy_bytes;
  const size_t payload_bytes = cy_bytes + vy_bytes;
  if (stats != nullptr) {
    stats->payload_bytes = payload_bytes;
  }

  size_t logical_n = ny;
  if (cfg.okvs_bucket_slack > 0) {
    logical_n = ny + cfg.okvs_bucket_slack;
    if (logical_n < ny) {
      return false;
    }
  }
  const size_t n_enc = cfg.pad_small_buckets ? PaddedOkvsN(logical_n) : logical_n;
  if (stats != nullptr) {
    stats->n_enc = n_enc;
  }
  const OkvsConfig base_okvs_cfg = SelectOkvsConfigByN(n_enc);
  if (!IsOkvsParamShapeValid(n_enc, base_okvs_cfg)) {
    return false;
  }

  std::vector<uint64_t> x_scalars(nx), y_scalars(ny);
  for (size_t i = 0; i < nx; ++i) {
    x_scalars[i] = ItemToScalar64(x_bucket[i], cfg.scalar_seed);
  }
  for (size_t i = 0; i < ny; ++i) {
    y_scalars[i] = ItemToScalar64(y_bucket[i], cfg.scalar_seed);
  }

  // RELIC cp_pbpsi_int shows a correctness edge case at m=1.
  // Pad X-side scalars to at least 2 entries for cryptographic processing only.
  std::vector<uint64_t> x_scalars_pbpsi = x_scalars;
  if (x_scalars_pbpsi.size() == 1) {
    uint64_t dummy = SplitMix64(x_scalars_pbpsi[0] ^ 0xA5A5A5A5ULL) | 1ULL;
    if (dummy == x_scalars_pbpsi[0]) {
      dummy ^= 0x9E3779B97F4A7C15ULL;
      dummy |= 1ULL;
    }
    x_scalars_pbpsi.push_back(dummy);
  }

  const double t_create_begin_ms = HighResNowMs();
  upsi_relic_bucket_ctx* bridge =
      upsi_relic_bucket_create(x_scalars_pbpsi.data(), x_scalars_pbpsi.size());
  const double t_create_end_ms = HighResNowMs();
  if (bridge == nullptr) {
    return false;
  }
  if (stats != nullptr) {
    stats->p0_bucket_create_ms = t_create_end_ms - t_create_begin_ms;
    stats->p0_to_p1_ci_bytes = upsi_relic_default_ci_bytes();
    stats->p0_to_p1_bytes = stats->p0_to_p1_ci_bytes;
  }

  std::vector<uint128_t> keys_enc(n_enc);
  std::vector<std::vector<uint8_t>> payloads_enc(
      n_enc, std::vector<uint8_t>(payload_bytes, 0));

  const double t_p1_window_begin_ms = t_create_end_ms;
  const double t_sign_begin_ms = t_p1_window_begin_ms;
  std::vector<uint8_t> real_payload_bytes(ny * payload_bytes, 0);
  if (!upsi_relic_bucket_sign(bridge, y_scalars.data(), y_scalars.size(),
                              real_payload_bytes.data(), payload_bytes, cy_bytes,
                              vy_bytes)) {
    upsi_relic_bucket_destroy(bridge);
    return false;
  }
  const double t_sign_end_ms = HighResNowMs();

  for (size_t i = 0; i < ny; ++i) {
    keys_enc[i] = H2ToUint128(y_bucket[i], cfg.h2_seed1, cfg.h2_seed2);
    std::memcpy(payloads_enc[i].data(), real_payload_bytes.data() + i * payload_bytes,
                payload_bytes);
  }

  const double t_okvs_begin_ms = HighResNowMs();
  auto fill_dummy_tail = [&](uint64_t salt) {
    uint64_t dummy_ctr = 0x13579BDFULL ^ salt;
    for (size_t i = ny; i < n_enc; ++i) {
      uint128_t k = 0;
      do {
        ++dummy_ctr;
        const uint64_t hi = SplitMix64(dummy_ctr ^ cfg.h2_seed1);
        const uint64_t lo = SplitMix64(dummy_ctr ^ cfg.h2_seed2);
        k = MakeUint128(hi, lo);
      } while (ContainsKeyPrefix(keys_enc, i, k));
      keys_enc[i] = k;

      for (size_t j = 0; j < payload_bytes; ++j) {
        payloads_enc[i][j] =
            (uint8_t)(SplitMix64(dummy_ctr + (uint64_t)j * 0x9E37ULL) & 0xFF);
      }
    }
  };

  auto max_i64 = [](int64_t a, int64_t b) { return a > b ? a : b; };
  auto max_d = [](double a, double b) { return a > b ? a : b; };

  const bool strict_robust = cfg.enable_okvs_encode_robust_mode;
  std::vector<OkvsConfig> cfg_candidates;
  cfg_candidates.push_back(base_okvs_cfg);
  const OkvsConfig stronger[] = {
      {max_i64(base_okvs_cfg.w, 12), max_d(base_okvs_cfg.e, 1.50)},
      {max_i64(base_okvs_cfg.w, 16), max_d(base_okvs_cfg.e, 1.75)},
      {max_i64(base_okvs_cfg.w, 24), max_d(base_okvs_cfg.e, 2.00)},
  };
  const size_t strong_cap = sizeof(stronger) / sizeof(stronger[0]);
  const size_t max_extra = strict_robust
                               ? std::min<size_t>(cfg.okvs_robust_param_levels > 0
                                                      ? (cfg.okvs_robust_param_levels - 1)
                                                      : 0,
                                                  strong_cap)
                               : std::min<size_t>(1, strong_cap);
  for (size_t i = 0; i < max_extra; ++i) {
    cfg_candidates.push_back(stronger[i]);
  }

  const size_t retries =
      strict_robust ? std::max<size_t>(1, cfg.okvs_robust_retries_per_param) : 1;

  bool okvs_ready = false;
  OkvsConfig chosen_okvs_cfg = base_okvs_cfg;
  std::vector<std::vector<uint128_t>> chosen_tables;
  int64_t chosen_okvs_m = 0;
  size_t local_okvs_attempts = 0;
  size_t local_self_checks = 0;
  size_t local_self_checks_full = 0;
  size_t local_self_checks_sampled = 0;

  for (size_t ci = 0; ci < cfg_candidates.size() && !okvs_ready; ++ci) {
    const OkvsConfig cur_cfg = cfg_candidates[ci];
    if (!IsOkvsParamShapeValid(n_enc, cur_cfg)) {
      continue;
    }
    for (size_t attempt = 0; attempt < retries && !okvs_ready; ++attempt) {
      ++local_okvs_attempts;
      fill_dummy_tail(static_cast<uint64_t>(attempt + ci * 17));

      BlockSplitOkvs candidate(
          {(int64_t)n_enc, cur_cfg.w, cur_cfg.e, payload_bytes});
      if (!candidate.Encode(keys_enc, payloads_enc)) {
        continue;
      }

      ++local_self_checks;
      const bool use_full_self_check =
          strict_robust &&
          (!cfg.okvs_robust_enable_sampled_self_check ||
           n_enc <= cfg.okvs_robust_full_check_max_n);
      if (use_full_self_check) {
        ++local_self_checks_full;
      } else {
        ++local_self_checks_sampled;
      }

      std::vector<uint128_t> check_keys;
      std::vector<std::vector<uint8_t>> expected_payloads;
      if (use_full_self_check) {
        check_keys = keys_enc;
        expected_payloads = payloads_enc;
      } else if (strict_robust) {
        const auto sample_idx = BuildSelfCheckSampleIndices(
            n_enc, ny, cfg.okvs_robust_self_check_sample_keys,
            static_cast<uint64_t>((ci + 1) * 131 + (attempt + 1) * 977));
        check_keys.reserve(sample_idx.size());
        expected_payloads.reserve(sample_idx.size());
        for (size_t si = 0; si < sample_idx.size(); ++si) {
          const size_t kidx = sample_idx[si];
          check_keys.push_back(keys_enc[kidx]);
          expected_payloads.push_back(payloads_enc[kidx]);
        }
      } else {
        // Non-robust mode: lightweight consistency check on real keys only.
        check_keys.assign(keys_enc.begin(), keys_enc.begin() + ny);
        expected_payloads.assign(payloads_enc.begin(), payloads_enc.begin() + ny);
      }

      std::vector<std::vector<uint8_t>> self_decoded;
      if (!candidate.Decode(check_keys, self_decoded)) {
        continue;
      }
      if (self_decoded != expected_payloads) {
        continue;
      }

      okvs_ready = true;
      chosen_okvs_cfg = cur_cfg;
      chosen_tables = candidate.tables();
      chosen_okvs_m = candidate.table_m();
    }
  }

  if (!okvs_ready) {
    upsi_relic_bucket_destroy(bridge);
    return false;
  }
  const double t_okvs_end_ms = HighResNowMs();

  BlockSplitOkvs okvs(
      {(int64_t)n_enc, chosen_okvs_cfg.w, chosen_okvs_cfg.e, payload_bytes});
  okvs.SetTables(chosen_tables);

  if (stats != nullptr) {
    stats->okvs_encode_attempts = local_okvs_attempts;
    stats->okvs_self_checks = local_self_checks;
    stats->okvs_self_checks_full = local_self_checks_full;
    stats->okvs_self_checks_sampled = local_self_checks_sampled;
    stats->okvs_block_count = okvs.block_count();
    stats->okvs_table_m = static_cast<size_t>(chosen_okvs_m);
    stats->okvs_table_bytes =
        stats->okvs_block_count * stats->okvs_table_m * static_cast<size_t>(16);
    stats->p1_to_p0_okvs_bytes = stats->okvs_table_bytes;
    stats->p1_to_p0_bytes = stats->p1_to_p0_okvs_bytes;
    stats->p1_sign_ms = t_sign_end_ms - t_sign_begin_ms;
    stats->p1_okvs_encode_ms = t_okvs_end_ms - t_okvs_begin_ms;
  }

  std::vector<uint128_t> x_keys(nx);
  for (size_t i = 0; i < nx; ++i) {
    x_keys[i] = H2ToUint128(x_bucket[i], cfg.h2_seed1, cfg.h2_seed2);
  }

  const double t_decode_begin_ms = HighResNowMs();
  std::vector<std::vector<uint8_t>> decoded;
  if (!okvs.Decode(x_keys, decoded) || decoded.size() != nx) {
    upsi_relic_bucket_destroy(bridge);
    return false;
  }
  const double t_decode_end_ms = HighResNowMs();

  const double t_verify_begin_ms = HighResNowMs();
  for (size_t i = 0; i < nx; ++i) {
    int is_match = 0;
    if (!upsi_relic_bucket_verify_x(bridge, i, decoded[i].data(), cy_bytes,
                                    vy_bytes, &is_match)) {
      upsi_relic_bucket_destroy(bridge);
      return false;
    }
    if (is_match) {
      out_intersection.push_back(x_bucket[i]);
      if (stats != nullptr) {
        ++stats->match_count;
      }
    }
  }
  const double t_verify_end_ms = HighResNowMs();

  if (stats != nullptr) {
    // P0 returns matched hashes/ids to P1 (approx 16 bytes per key).
    stats->p0_to_p1_result_bytes = stats->match_count * static_cast<size_t>(16);
    stats->p0_to_p1_bytes = stats->p0_to_p1_ci_bytes + stats->p0_to_p1_result_bytes;
    stats->total_comm_bytes = stats->p0_to_p1_bytes + stats->p1_to_p0_bytes;
    stats->p0_okvs_decode_ms = t_decode_end_ms - t_decode_begin_ms;
    stats->p0_verify_ms = t_verify_end_ms - t_verify_begin_ms;
    stats->p1_online_window_ms = t_verify_end_ms - t_p1_window_begin_ms;
    stats->p1_online_ms = stats->p1_online_window_ms;
    stats->bucket_total_ms = HighResNowMs() - t_bucket_begin_ms;
  }

  upsi_relic_bucket_destroy(bridge);
  return true;
}

}  // namespace upsi
