#include "upsi_protocol_initial.h"

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
  if (bs.nx > 0 || bs.ny > 0) {
    ++stats.changed_bucket_count;
  }
}

void SetOnlineStatFromBucketWindows(const std::vector<BucketTaskResult>& results,
                                    RoundEvalStats& stats) {
  bool has = false;
  double first_ci_ms = 0.0;
  double last_end_ms = 0.0;
  for (size_t i = 0; i < results.size(); ++i) {
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

bool RunInitialRound(const std::vector<Item>& x_set,
                     const std::vector<Item>& y_set,
                     const InitialRoundConfig& cfg,
                     InitialRoundResult& out) {
  RoundEvalStats stats;
  return RunInitialRoundWithStats(x_set, y_set, cfg, out, stats);
}

bool RunInitialRoundWithStats(const std::vector<Item>& x_set,
                              const std::vector<Item>& y_set,
                              const InitialRoundConfig& cfg,
                              InitialRoundResult& out, RoundEvalStats& stats) {
  stats = RoundEvalStats{};
  const double t_begin_ms = HighResNowMs();

  out.intersection.clear();

  const double t_bucketize_begin_ms = HighResNowMs();
  bool ok = true;
  const Buckets xb = BuildBuckets(x_set, cfg.bucket_cfg);
  const Buckets yb = BuildBuckets(y_set, cfg.bucket_cfg);
  const double t_bucketize_end_ms = HighResNowMs();
  stats.bucketize_ms = t_bucketize_end_ms - t_bucketize_begin_ms;

  const size_t b = cfg.bucket_cfg.bucket_count;
  const size_t worker_threads = std::max<size_t>(1, cfg.bucket_parallel_threads);
  if (worker_threads == 1 || b <= 1) {
    const double t_relic_init_begin_ms = HighResNowMs();
    if (!upsi_relic_global_init()) {
      return false;
    }
    const double t_relic_init_end_ms = HighResNowMs();
    stats.relic_init_ms = t_relic_init_end_ms - t_relic_init_begin_ms;

    bool p1_online_started = false;
    double first_ci_create_ms = 0.0;
    const double t_eval_begin_ms = HighResNowMs();
    for (size_t i = 0; i < b && ok; ++i) {
      BucketEvalStats bs;
      ok = EvalBucketIntersectionNoInit(xb[i], yb[i], cfg, out.intersection, &bs);
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
    stats.p1_online_ms =
        p1_online_started ? std::max(0.0, stats.eval_total_ms - first_ci_create_ms)
                          : 0.0;

    const double t_relic_clean_begin_ms = HighResNowMs();
    upsi_relic_global_clean();
    const double t_relic_clean_end_ms = HighResNowMs();
    stats.relic_clean_ms = t_relic_clean_end_ms - t_relic_clean_begin_ms;
  } else {
    std::vector<BucketTaskResult> results(b);
    std::atomic<size_t> next{0};
    std::atomic<bool> failed{false};

    const size_t th =
        std::min(worker_threads, static_cast<size_t>(std::max<size_t>(1, b)));
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
          const size_t i = next.fetch_add(1, std::memory_order_relaxed);
          if (i >= b) {
            break;
          }
          BucketTaskResult local;
          local.attempted = true;
          local.call_begin_ms = HighResNowMs();
          local.ok =
              EvalBucketIntersectionNoInit(xb[i], yb[i], cfg, local.intersection,
                                           &local.stats);
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

    for (size_t i = 0; i < b; ++i) {
      if (!results[i].attempted || !results[i].ok) {
        return false;
      }
      AccumulateBucketStats(results[i].stats, stats);
      if (!results[i].intersection.empty()) {
        out.intersection.insert(out.intersection.end(),
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
    SetOnlineStatFromBucketWindows(results, stats);
  }

  const double t_sort_begin_ms = HighResNowMs();
  SortUnique(out.intersection);
  const double t_sort_end_ms = HighResNowMs();
  stats.sort_unique_ms = t_sort_end_ms - t_sort_begin_ms;

  stats.total_ms = HighResNowMs() - t_begin_ms;
  return ok;
}

std::vector<Item> NaiveIntersection(const std::vector<Item>& x_set,
                                    const std::vector<Item>& y_set) {
  std::vector<Item> xs = x_set;
  std::vector<Item> ys = y_set;
  SortUnique(xs);
  SortUnique(ys);

  std::vector<Item> out;
  size_t i = 0;
  size_t j = 0;
  while (i < xs.size() && j < ys.size()) {
    if (xs[i] == ys[j]) {
      out.push_back(xs[i]);
      ++i;
      ++j;
    } else if (ItemLess(xs[i], ys[j])) {
      ++i;
    } else {
      ++j;
    }
  }
  return out;
}

}  // namespace upsi
