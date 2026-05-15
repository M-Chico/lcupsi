#include "upsi_protocol_update.h"

#include <algorithm>
#include <atomic>
#include <thread>
#include <vector>

#include "upsi_protocol_bucket_eval.h"
#include "upsi_relic_bridge.h"
#include "upsi_timer.h"

namespace upsi {

namespace {

bool ItemLess(const Item& a, const Item& b) {
  return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end());
}

bool ItemEq(const Item& a, const Item& b) { return a == b; }

void SortUnique(std::vector<Item>& xs) {
  std::sort(xs.begin(), xs.end(), ItemLess);
  xs.erase(std::unique(xs.begin(), xs.end(), ItemEq), xs.end());
}

size_t CountChanged(const std::vector<uint8_t>& flags) {
  size_t c = 0;
  for (size_t i = 0; i < flags.size(); ++i) {
    if (flags[i]) {
      ++c;
    }
  }
  return c;
}

struct BucketTaskResult {
  bool attempted = false;
  bool ok = false;
  BucketEvalStats stats;
  std::vector<Item> intersection;
  double call_begin_ms = 0.0;
  double call_end_ms = 0.0;
};

struct ThreadEvalSums {
  double relic_init_ms = 0.0;
  double relic_clean_ms = 0.0;
  double eval_total_ms = 0.0;
  double p0_bucket_create_ms = 0.0;
  double p1_sign_ms = 0.0;
  double p1_okvs_encode_ms = 0.0;
  double p0_okvs_decode_ms = 0.0;
  double p0_verify_ms = 0.0;
};

void AccumulateBucketStats(const BucketEvalStats& bs, RoundEvalStats& stats) {
  stats.eval_total_ms += bs.bucket_total_ms;
  stats.p0_bucket_create_ms += bs.p0_bucket_create_ms;
  stats.p1_sign_ms += bs.p1_sign_ms;
  stats.p1_okvs_encode_ms += bs.p1_okvs_encode_ms;
  stats.p0_okvs_decode_ms += bs.p0_okvs_decode_ms;
  stats.p0_verify_ms += bs.p0_verify_ms;
  stats.p1_sign_item_count += bs.ny;
  stats.p0_verify_item_count += bs.nx;
  stats.p0_to_p1_ci_bytes += bs.p0_to_p1_ci_bytes;
  stats.p1_to_p0_okvs_bytes += bs.p1_to_p0_okvs_bytes;
  stats.p0_to_p1_result_bytes += bs.p0_to_p1_result_bytes;
  stats.okvs_encode_attempts += bs.okvs_encode_attempts;
  stats.okvs_self_checks += bs.okvs_self_checks;
  stats.okvs_self_checks_full += bs.okvs_self_checks_full;
  stats.okvs_self_checks_sampled += bs.okvs_self_checks_sampled;
  stats.p0_to_p1_bytes += bs.p0_to_p1_bytes;
  stats.p1_to_p0_bytes += bs.p1_to_p0_bytes;
  stats.total_comm_bytes += bs.total_comm_bytes;
}

void SetOnlineStatFromBucketWindows(const std::vector<BucketTaskResult>& results,
                                    const std::vector<size_t>& changed_idx,
                                    RoundEvalStats& stats) {
  bool has = false;
  double first_ci_ms = 0.0;
  double last_end_ms = 0.0;
  for (size_t k = 0; k < changed_idx.size(); ++k) {
    const size_t i = changed_idx[k];
    if (!results[i].attempted || !results[i].ok) {
      continue;
    }
    const BucketEvalStats& bs = results[i].stats;
    if (bs.p0_to_p1_ci_bytes == 0) {
      continue;
    }
    const double ci_ms = results[i].call_begin_ms + bs.p0_bucket_create_ms;
    const double end_ms = results[i].call_end_ms;
    if (!has) {
      has = true;
      first_ci_ms = ci_ms;
      last_end_ms = end_ms;
    } else {
      first_ci_ms = std::min(first_ci_ms, ci_ms);
      last_end_ms = std::max(last_end_ms, end_ms);
    }
  }
  stats.p1_online_ms = has ? std::max(0.0, last_end_ms - first_ci_ms) : 0.0;
}

}  // namespace

bool InitializeUpdateState(const std::vector<Item>& x_set,
                           const std::vector<Item>& y_set,
                           const InitialRoundConfig& cfg,
                           UpdateProtocolState& out_state) {
  out_state.cfg = cfg;
  out_state.x_buckets = BuildBuckets(x_set, cfg.bucket_cfg);
  out_state.y_buckets = BuildBuckets(y_set, cfg.bucket_cfg);

  InitialRoundResult initial;
  if (!RunInitialRound(x_set, y_set, cfg, initial)) {
    return false;
  }
  out_state.intersection = initial.intersection;
  return true;
}

bool RunUpdateRound(UpdateProtocolState& state, const std::vector<Item>& add_set,
                    const std::vector<Item>& del_set, UpdateRoundResult& out) {
  RoundEvalStats stats;
  return RunUpdateRoundWithStats(state, add_set, del_set, out, stats);
}

bool RunUpdateRoundWithStats(UpdateProtocolState& state,
                             const std::vector<Item>& add_set,
                             const std::vector<Item>& del_set,
                             UpdateRoundResult& out, RoundEvalStats& stats) {
  stats = RoundEvalStats{};
  const double t_begin_ms = HighResNowMs();

  const double t_apply_begin_ms = HighResNowMs();
  const BucketUpdateResult delta =
      ApplyBucketDelta(state.y_buckets, add_set, del_set, state.cfg.bucket_cfg);
  const double t_apply_end_ms = HighResNowMs();
  stats.delta_apply_ms = t_apply_end_ms - t_apply_begin_ms;

  out.changed_bucket_flags = delta.flags;
  out.changed_bucket_count = CountChanged(delta.flags);
  stats.changed_bucket_count = out.changed_bucket_count;

  const double t_filter_begin_ms = HighResNowMs();
  std::vector<Item> next_intersection;
  next_intersection.reserve(state.intersection.size());
  for (size_t i = 0; i < state.intersection.size(); ++i) {
    const size_t bidx = BucketOf(state.intersection[i], state.cfg.bucket_cfg);
    if (bidx < delta.flags.size() && delta.flags[bidx] == 0) {
      next_intersection.push_back(state.intersection[i]);
    }
  }
  const double t_filter_end_ms = HighResNowMs();
  stats.unchanged_filter_ms = t_filter_end_ms - t_filter_begin_ms;

  if (out.changed_bucket_count > 0) {
    std::vector<size_t> changed_idx;
    changed_idx.reserve(out.changed_bucket_count);
    for (size_t i = 0; i < delta.flags.size(); ++i) {
      if (delta.flags[i]) {
        changed_idx.push_back(i);
      }
    }

    const size_t worker_threads =
        std::max<size_t>(1, state.cfg.bucket_parallel_threads);
    if (worker_threads == 1 || changed_idx.size() <= 1) {
      const double t_relic_init_begin_ms = HighResNowMs();
      if (!upsi_relic_global_init()) {
        return false;
      }
      const double t_relic_init_end_ms = HighResNowMs();
      stats.relic_init_ms = t_relic_init_end_ms - t_relic_init_begin_ms;

      bool ok = true;
      bool p1_online_started = false;
      double first_ci_create_ms = 0.0;
      const double t_eval_begin_ms = HighResNowMs();
      for (size_t k = 0; k < changed_idx.size() && ok; ++k) {
        const size_t i = changed_idx[k];
        BucketEvalStats bs;
        ok = EvalBucketIntersectionNoInit(state.x_buckets[i], state.y_buckets[i],
                                          state.cfg, next_intersection, &bs);
        if (!ok) {
          break;
        }
        AccumulateBucketStats(bs, stats);
        if (!p1_online_started && bs.p0_to_p1_ci_bytes > 0) {
          p1_online_started = true;
          first_ci_create_ms = bs.p0_bucket_create_ms;
        }
      }
      const double t_eval_end_ms = HighResNowMs();
      stats.eval_total_ms = t_eval_end_ms - t_eval_begin_ms;
      const double t_relic_clean_begin_ms = HighResNowMs();
      upsi_relic_global_clean();
      const double t_relic_clean_end_ms = HighResNowMs();
      stats.relic_clean_ms = t_relic_clean_end_ms - t_relic_clean_begin_ms;
      if (!ok) {
        return false;
      }
      stats.p1_online_ms =
          p1_online_started
              ? std::max(0.0, stats.eval_total_ms - first_ci_create_ms)
              : 0.0;
    } else {
      std::vector<BucketTaskResult> results(delta.flags.size());
      std::atomic<size_t> next{0};
      std::atomic<bool> failed{false};
      const size_t th = std::min(worker_threads, changed_idx.size());
      std::vector<ThreadEvalSums> thread_sums(th);
      const double t_eval_begin_ms = HighResNowMs();
      std::vector<std::thread> workers;
      workers.reserve(th);
      for (size_t ti = 0; ti < th; ++ti) {
        workers.emplace_back([&, ti]() {
          ThreadEvalSums local_sums;
          const double t_init_begin_ms = HighResNowMs();
          if (!upsi_relic_global_init()) {
            failed.store(true, std::memory_order_relaxed);
            return;
          }
          const double t_init_end_ms = HighResNowMs();
          local_sums.relic_init_ms = t_init_end_ms - t_init_begin_ms;

          while (!failed.load(std::memory_order_relaxed)) {
            const size_t p = next.fetch_add(1, std::memory_order_relaxed);
            if (p >= changed_idx.size()) {
              break;
            }
            const size_t i = changed_idx[p];
            BucketTaskResult local;
            local.attempted = true;
            local.call_begin_ms = HighResNowMs();
            local.ok = EvalBucketIntersectionNoInit(
                state.x_buckets[i], state.y_buckets[i], state.cfg,
                local.intersection, &local.stats);
            local.call_end_ms = HighResNowMs();
            local_sums.eval_total_ms += local.stats.bucket_total_ms;
            local_sums.p0_bucket_create_ms += local.stats.p0_bucket_create_ms;
            local_sums.p1_sign_ms += local.stats.p1_sign_ms;
            local_sums.p1_okvs_encode_ms += local.stats.p1_okvs_encode_ms;
            local_sums.p0_okvs_decode_ms += local.stats.p0_okvs_decode_ms;
            local_sums.p0_verify_ms += local.stats.p0_verify_ms;
            results[i] = std::move(local);
            if (!results[i].ok) {
              failed.store(true, std::memory_order_relaxed);
            }
          }

          const double t_clean_begin_ms = HighResNowMs();
          upsi_relic_global_clean();
          const double t_clean_end_ms = HighResNowMs();
          local_sums.relic_clean_ms = t_clean_end_ms - t_clean_begin_ms;
          thread_sums[ti] = local_sums;
        });
      }
      for (size_t i = 0; i < workers.size(); ++i) {
        workers[i].join();
      }
      const double t_eval_end_ms = HighResNowMs();

      double max_p0_bucket_create_ms = 0.0;
      double max_p1_sign_ms = 0.0;
      double max_p1_okvs_encode_ms = 0.0;
      double max_p0_okvs_decode_ms = 0.0;
      double max_p0_verify_ms = 0.0;
      for (size_t i = 0; i < thread_sums.size(); ++i) {
        stats.relic_init_ms =
            std::max(stats.relic_init_ms, thread_sums[i].relic_init_ms);
        stats.relic_clean_ms =
            std::max(stats.relic_clean_ms, thread_sums[i].relic_clean_ms);
        max_p0_bucket_create_ms =
            std::max(max_p0_bucket_create_ms, thread_sums[i].p0_bucket_create_ms);
        max_p1_sign_ms = std::max(max_p1_sign_ms, thread_sums[i].p1_sign_ms);
        max_p1_okvs_encode_ms =
            std::max(max_p1_okvs_encode_ms, thread_sums[i].p1_okvs_encode_ms);
        max_p0_okvs_decode_ms =
            std::max(max_p0_okvs_decode_ms, thread_sums[i].p0_okvs_decode_ms);
        max_p0_verify_ms = std::max(max_p0_verify_ms, thread_sums[i].p0_verify_ms);
      }
      if (failed.load(std::memory_order_relaxed)) {
        return false;
      }

      for (size_t k = 0; k < changed_idx.size(); ++k) {
        const size_t i = changed_idx[k];
        if (!results[i].attempted || !results[i].ok) {
          return false;
        }
        AccumulateBucketStats(results[i].stats, stats);
        if (!results[i].intersection.empty()) {
          next_intersection.insert(next_intersection.end(),
                                   results[i].intersection.begin(),
                                   results[i].intersection.end());
        }
      }
      stats.eval_total_ms = t_eval_end_ms - t_eval_begin_ms;
      stats.p0_bucket_create_ms = max_p0_bucket_create_ms;
      stats.p1_sign_ms = max_p1_sign_ms;
      stats.p1_okvs_encode_ms = max_p1_okvs_encode_ms;
      stats.p0_okvs_decode_ms = max_p0_okvs_decode_ms;
      stats.p0_verify_ms = max_p0_verify_ms;
      SetOnlineStatFromBucketWindows(results, changed_idx, stats);
    }
  }

  // Flag vector is part of update round communication.
  stats.p0_to_p1_flag_bytes = out.changed_bucket_flags.size();
  stats.p0_to_p1_bytes += out.changed_bucket_flags.size();
  stats.total_comm_bytes += out.changed_bucket_flags.size();

  const double t_sort_begin_ms = HighResNowMs();
  SortUnique(next_intersection);
  const double t_sort_end_ms = HighResNowMs();
  stats.sort_unique_ms = t_sort_end_ms - t_sort_begin_ms;
  state.intersection = next_intersection;
  out.intersection = state.intersection;

  stats.total_ms = HighResNowMs() - t_begin_ms;
  return true;
}

}  // namespace upsi
