#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
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

template <typename T>
T Median(std::vector<T> xs) {
  if (xs.empty()) {
    return T{};
  }
  std::sort(xs.begin(), xs.end());
  return xs[xs.size() / 2];
}

struct StatPoint {
  double setup_ms = 0.0;
  double total_ms = 0.0;
  double bucketize_ms = 0.0;
  double delta_apply_ms = 0.0;
  double unchanged_filter_ms = 0.0;
  double sort_unique_ms = 0.0;
  double relic_init_ms = 0.0;
  double relic_clean_ms = 0.0;
  double eval_total_ms = 0.0;
  double p0_bucket_create_ms = 0.0;
  double p1_sign_ms = 0.0;
  double p1_okvs_encode_ms = 0.0;
  double p0_okvs_decode_ms = 0.0;
  double p0_verify_ms = 0.0;
  double p1_online_ms = 0.0;
  size_t p1_sign_items = 0;
  size_t p0_verify_items = 0;
  double p1_sign_us_per_item = 0.0;
  double p0_verify_us_per_item = 0.0;
  size_t x_bucket_min = 0;
  size_t x_bucket_max = 0;
  size_t y_bucket_min = 0;
  size_t y_bucket_max = 0;
  size_t p0_to_p1_ci_bytes = 0;
  size_t p1_to_p0_okvs_bytes = 0;
  size_t p0_to_p1_result_bytes = 0;
  size_t p0_to_p1_flag_bytes = 0;
  size_t p0_to_p1_bytes = 0;
  size_t p1_to_p0_bytes = 0;
  size_t total_comm_bytes = 0;
  size_t okvs_encode_attempts = 0;
  size_t okvs_self_checks = 0;
  size_t okvs_self_checks_full = 0;
  size_t okvs_self_checks_sampled = 0;
  size_t changed_bucket_count = 0;
  size_t intersection_size = 0;
};

StatPoint MedianStat(const std::vector<StatPoint>& xs) {
  StatPoint out;
  std::vector<double> s, t, b0, d0, uf, su, ri, rc, et, c0, s1, oe, d1, v0, p1, s1u,
      v0u;
  std::vector<size_t> ci, ok, rs, fg, a, b, c, oa, os, osf, oss, d, z, s1n, v0n, xmin,
      xmax, ymin, ymax;
  s.reserve(xs.size());
  t.reserve(xs.size());
  b0.reserve(xs.size());
  d0.reserve(xs.size());
  uf.reserve(xs.size());
  su.reserve(xs.size());
  ri.reserve(xs.size());
  rc.reserve(xs.size());
  et.reserve(xs.size());
  c0.reserve(xs.size());
  s1.reserve(xs.size());
  oe.reserve(xs.size());
  d1.reserve(xs.size());
  v0.reserve(xs.size());
  p1.reserve(xs.size());
  s1u.reserve(xs.size());
  v0u.reserve(xs.size());
  ci.reserve(xs.size());
  ok.reserve(xs.size());
  rs.reserve(xs.size());
  fg.reserve(xs.size());
  a.reserve(xs.size());
  b.reserve(xs.size());
  c.reserve(xs.size());
  oa.reserve(xs.size());
  os.reserve(xs.size());
  osf.reserve(xs.size());
  oss.reserve(xs.size());
  d.reserve(xs.size());
  z.reserve(xs.size());
  s1n.reserve(xs.size());
  v0n.reserve(xs.size());
  xmin.reserve(xs.size());
  xmax.reserve(xs.size());
  ymin.reserve(xs.size());
  ymax.reserve(xs.size());
  for (size_t i = 0; i < xs.size(); ++i) {
    s.push_back(xs[i].setup_ms);
    t.push_back(xs[i].total_ms);
    b0.push_back(xs[i].bucketize_ms);
    d0.push_back(xs[i].delta_apply_ms);
    uf.push_back(xs[i].unchanged_filter_ms);
    su.push_back(xs[i].sort_unique_ms);
    ri.push_back(xs[i].relic_init_ms);
    rc.push_back(xs[i].relic_clean_ms);
    et.push_back(xs[i].eval_total_ms);
    c0.push_back(xs[i].p0_bucket_create_ms);
    s1.push_back(xs[i].p1_sign_ms);
    oe.push_back(xs[i].p1_okvs_encode_ms);
    d1.push_back(xs[i].p0_okvs_decode_ms);
    v0.push_back(xs[i].p0_verify_ms);
    p1.push_back(xs[i].p1_online_ms);
    s1u.push_back(xs[i].p1_sign_us_per_item);
    v0u.push_back(xs[i].p0_verify_us_per_item);
    ci.push_back(xs[i].p0_to_p1_ci_bytes);
    ok.push_back(xs[i].p1_to_p0_okvs_bytes);
    rs.push_back(xs[i].p0_to_p1_result_bytes);
    fg.push_back(xs[i].p0_to_p1_flag_bytes);
    a.push_back(xs[i].p0_to_p1_bytes);
    b.push_back(xs[i].p1_to_p0_bytes);
    c.push_back(xs[i].total_comm_bytes);
    oa.push_back(xs[i].okvs_encode_attempts);
    os.push_back(xs[i].okvs_self_checks);
    osf.push_back(xs[i].okvs_self_checks_full);
    oss.push_back(xs[i].okvs_self_checks_sampled);
    d.push_back(xs[i].changed_bucket_count);
    z.push_back(xs[i].intersection_size);
    s1n.push_back(xs[i].p1_sign_items);
    v0n.push_back(xs[i].p0_verify_items);
    xmin.push_back(xs[i].x_bucket_min);
    xmax.push_back(xs[i].x_bucket_max);
    ymin.push_back(xs[i].y_bucket_min);
    ymax.push_back(xs[i].y_bucket_max);
  }
  out.setup_ms = Median(s);
  out.total_ms = Median(t);
  out.bucketize_ms = Median(b0);
  out.delta_apply_ms = Median(d0);
  out.unchanged_filter_ms = Median(uf);
  out.sort_unique_ms = Median(su);
  out.relic_init_ms = Median(ri);
  out.relic_clean_ms = Median(rc);
  out.eval_total_ms = Median(et);
  out.p0_bucket_create_ms = Median(c0);
  out.p1_sign_ms = Median(s1);
  out.p1_okvs_encode_ms = Median(oe);
  out.p0_okvs_decode_ms = Median(d1);
  out.p0_verify_ms = Median(v0);
  out.p1_online_ms = Median(p1);
  out.p1_sign_us_per_item = Median(s1u);
  out.p0_verify_us_per_item = Median(v0u);
  out.p0_to_p1_ci_bytes = Median(ci);
  out.p1_to_p0_okvs_bytes = Median(ok);
  out.p0_to_p1_result_bytes = Median(rs);
  out.p0_to_p1_flag_bytes = Median(fg);
  out.p0_to_p1_bytes = Median(a);
  out.p1_to_p0_bytes = Median(b);
  out.total_comm_bytes = Median(c);
  out.okvs_encode_attempts = Median(oa);
  out.okvs_self_checks = Median(os);
  out.okvs_self_checks_full = Median(osf);
  out.okvs_self_checks_sampled = Median(oss);
  out.changed_bucket_count = Median(d);
  out.intersection_size = Median(z);
  out.p1_sign_items = Median(s1n);
  out.p0_verify_items = Median(v0n);
  out.x_bucket_min = Median(xmin);
  out.x_bucket_max = Median(xmax);
  out.y_bucket_min = Median(ymin);
  out.y_bucket_max = Median(ymax);
  return out;
}

void FillPerItemTiming(StatPoint& p) {
  p.p1_sign_us_per_item =
      (p.p1_sign_items == 0)
          ? 0.0
          : (1000.0 * p.p1_sign_ms) / static_cast<double>(p.p1_sign_items);
  p.p0_verify_us_per_item =
      (p.p0_verify_items == 0)
          ? 0.0
          : (1000.0 * p.p0_verify_ms) / static_cast<double>(p.p0_verify_items);
}

void FillBucketMinMax(const upsi::Buckets& xb, const upsi::Buckets& yb, StatPoint& p) {
  if (xb.empty() || yb.empty()) {
    p.x_bucket_min = 0;
    p.x_bucket_max = 0;
    p.y_bucket_min = 0;
    p.y_bucket_max = 0;
    return;
  }
  size_t x_min = xb[0].size();
  size_t x_max = xb[0].size();
  for (size_t i = 1; i < xb.size(); ++i) {
    x_min = std::min(x_min, xb[i].size());
    x_max = std::max(x_max, xb[i].size());
  }

  size_t y_min = yb[0].size();
  size_t y_max = yb[0].size();
  for (size_t i = 1; i < yb.size(); ++i) {
    y_min = std::min(y_min, yb[i].size());
    y_max = std::max(y_max, yb[i].size());
  }

  p.x_bucket_min = x_min;
  p.x_bucket_max = x_max;
  p.y_bucket_min = y_min;
  p.y_bucket_max = y_max;
}

void BuildBaseSets(size_t n, std::vector<upsi::Item>& x_set,
                   std::vector<upsi::Item>& y_set) {
  x_set.clear();
  y_set.clear();
  x_set.reserve(n);
  y_set.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    x_set.push_back(U64ToItem(static_cast<uint64_t>(i + 1)));
  }
  for (size_t i = 0; i < n; ++i) {
    y_set.push_back(U64ToItem(static_cast<uint64_t>(i + 1 + n / 2)));
  }
}

void BuildDeltaSets(size_t n, size_t delta_sz, std::vector<upsi::Item>& add_set,
                    std::vector<upsi::Item>& del_set) {
  add_set.clear();
  del_set.clear();
  add_set.reserve(delta_sz);
  del_set.reserve(delta_sz);

  // Add small IDs to increase overlap with X.
  for (size_t i = 0; i < delta_sz; ++i) {
    add_set.push_back(U64ToItem(static_cast<uint64_t>(1 + i)));
  }
  // Delete from upper half of current Y.
  const uint64_t del_base = static_cast<uint64_t>(n / 2 + 1);
  for (size_t i = 0; i < delta_sz; ++i) {
    del_set.push_back(U64ToItem(del_base + static_cast<uint64_t>(i)));
  }
}

upsi::InitialRoundConfig BuildCfg(size_t n, size_t bucket_count, size_t bucket_slack,
                                  bool robust_mode, size_t bucket_threads) {
  upsi::InitialRoundConfig cfg;
  cfg.bucket_cfg = {bucket_count, 0xBADC0FFEE0DDF00DULL};
  cfg.payload_cfg = {49, 32};
  cfg.pad_small_buckets = true;
  cfg.scalar_seed = 0x31415926ULL;
  cfg.h2_seed1 = 0x1234567890ABCDEFULL;
  cfg.h2_seed2 = 0x0FEDCBA098765432ULL;
  cfg.bucket_parallel_threads = std::max<size_t>(1, bucket_threads);
  cfg.okvs_bucket_slack = bucket_slack;
  cfg.enable_okvs_encode_robust_mode = robust_mode;
  cfg.okvs_robust_retries_per_param = robust_mode ? 4 : 1;
  cfg.okvs_robust_param_levels = robust_mode ? 4 : 1;
  // Keep strict robust defaults for experiment correctness.
  // Optional sampled self-check remains available in config but disabled by default.
  cfg.okvs_robust_enable_sampled_self_check = false;
  cfg.okvs_robust_self_check_sample_keys = 64;
  cfg.okvs_robust_full_check_max_n = static_cast<size_t>(-1);
  return cfg;
}

void WriteCsvHeader(std::ofstream& ofs) {
  ofs << "stage,n,bucket_count,bucket_threads,trials,median_total_ms,median_setup_ms,"
         "median_bucketize_ms,median_delta_apply_ms,median_unchanged_filter_ms,"
         "median_sort_unique_ms,median_relic_init_ms,median_relic_clean_ms,"
         "median_eval_total_ms,median_p0_bucket_create_ms,median_p1_sign_ms,"
         "median_p1_okvs_encode_ms,median_p0_okvs_decode_ms,median_p0_verify_ms,"
         "median_p1_online_ms,median_p1_sign_items,median_p0_verify_items,"
         "median_p1_sign_us_per_item,median_p0_verify_us_per_item,"
         "median_x_bucket_min,median_x_bucket_max,median_y_bucket_min,"
         "median_y_bucket_max,median_p0_to_p1_ci_bytes,median_p1_to_p0_okvs_bytes,"
         "median_p0_to_p1_result_bytes,median_p0_to_p1_flag_bytes,"
         "median_p0_to_p1_bytes,median_p1_to_p0_bytes,median_total_comm_bytes,"
         "median_okvs_encode_attempts,median_okvs_self_checks,"
         "median_okvs_self_checks_full,median_okvs_self_checks_sampled,"
         "median_changed_buckets,median_intersection,delta_size,bucket_slack,"
         "robust_mode\n";
}

void WriteCsvRow(std::ofstream& ofs, const std::string& stage, size_t n,
                 size_t bucket_count, size_t bucket_threads, size_t trials,
                 const StatPoint& m,
                 size_t delta_size, size_t bucket_slack, bool robust_mode) {
  ofs << stage << "," << n << "," << bucket_count << "," << bucket_threads << ","
      << trials << ","
      << std::fixed << std::setprecision(6) << m.total_ms << "," << m.setup_ms << ","
      << m.bucketize_ms << "," << m.delta_apply_ms << "," << m.unchanged_filter_ms
      << "," << m.sort_unique_ms << "," << m.relic_init_ms << ","
      << m.relic_clean_ms << "," << m.eval_total_ms << "," << m.p0_bucket_create_ms
      << "," << m.p1_sign_ms << "," << m.p1_okvs_encode_ms << ","
      << m.p0_okvs_decode_ms << "," << m.p0_verify_ms << "," << m.p1_online_ms
      << "," << m.p1_sign_items << "," << m.p0_verify_items << ","
      << m.p1_sign_us_per_item << "," << m.p0_verify_us_per_item << ","
      << m.x_bucket_min << "," << m.x_bucket_max << "," << m.y_bucket_min << ","
      << m.y_bucket_max << "," << m.p0_to_p1_ci_bytes << ","
      << m.p1_to_p0_okvs_bytes << "," << m.p0_to_p1_result_bytes << ","
      << m.p0_to_p1_flag_bytes << "," << m.p0_to_p1_bytes << ","
      << m.p1_to_p0_bytes << "," << m.total_comm_bytes << ","
      << m.okvs_encode_attempts << "," << m.okvs_self_checks << ","
      << m.okvs_self_checks_full << "," << m.okvs_self_checks_sampled << ","
      << m.changed_bucket_count << "," << m.intersection_size << "," << delta_size
      << "," << bucket_slack << ","
      << (robust_mode ? 1 : 0) << "\n";
}

}  // namespace

int main(int argc, char** argv) {
  const std::string out_csv =
      (argc >= 2) ? std::string(argv[1])
                  : std::string("upsi_4_3_2_summary.csv");
  const std::string profile = (argc >= 3) ? std::string(argv[2]) : "target";
  const size_t trials =
      (argc >= 4) ? static_cast<size_t>(std::strtoull(argv[3], nullptr, 10)) : 1;
  const bool robust_mode = (argc >= 5) ? (std::string(argv[4]) == "1") : false;
  const size_t max_points =
      (argc >= 6) ? static_cast<size_t>(std::strtoull(argv[5], nullptr, 10)) : 0;
  const size_t force_n =
      (argc >= 7) ? static_cast<size_t>(std::strtoull(argv[6], nullptr, 10)) : 0;
  const size_t force_delta =
      (argc >= 8) ? static_cast<size_t>(std::strtoull(argv[7], nullptr, 10)) : 0;
  const size_t bucket_threads =
      (argc >= 9) ? static_cast<size_t>(std::strtoull(argv[8], nullptr, 10)) : 1;

  std::vector<size_t> n_list;
  if (profile == "quick") {
    n_list = {128, 256, 512, 1024};
  } else {
    // UPSI 4.3.2 requested scale.
    n_list = {1ULL << 14, 1ULL << 16, 1ULL << 18};
  }
  if (force_n > 0) {
    n_list = {force_n};
  }

  std::vector<size_t> delta_list;
  if (profile == "quick") {
    delta_list = {1ULL << 3};
  } else {
    // UPSI 4.3.2 figure points: Nd = 2^3, 2^5, 2^7.
    delta_list = {1ULL << 3, 1ULL << 5, 1ULL << 7};
  }
  if (force_delta > 0) {
    delta_list = {force_delta};
  }

  const size_t kBucketDiv = 1ULL << 7;
  const size_t kBucketSlack = 128;

  std::ofstream ofs(out_csv, std::ios::out | std::ios::trunc);
  if (!ofs.is_open()) {
    std::cerr << "failed to open output csv: " << out_csv << "\n";
    return 1;
  }
  WriteCsvHeader(ofs);

  std::cout << "UPSI 4.3.2 benchmark start\n";
  std::cout << "output: " << out_csv << "\n";
  std::cout << "profile: " << profile << "\n";
  std::cout << "trials per point: " << trials << "\n";
  std::cout << "robust_mode: " << (robust_mode ? "on" : "off") << "\n";
  std::cout << "bucket_threads: " << std::max<size_t>(1, bucket_threads) << "\n";

  const size_t point_count =
      (max_points == 0) ? n_list.size() : std::min(max_points, n_list.size());
  for (size_t ni = 0; ni < point_count; ++ni) {
    const size_t n = n_list[ni];
    const size_t bucket_count = std::max<size_t>(8, n / kBucketDiv);
    const upsi::InitialRoundConfig cfg =
        BuildCfg(n, bucket_count, kBucketSlack, robust_mode, bucket_threads);

    std::vector<upsi::Item> x_set, y_set;
    BuildBaseSets(n, x_set, y_set);
    const upsi::Buckets x_base_buckets = upsi::BuildBuckets(x_set, cfg.bucket_cfg);
    const upsi::Buckets y_base_buckets = upsi::BuildBuckets(y_set, cfg.bucket_cfg);

    for (size_t di = 0; di < delta_list.size(); ++di) {
      const size_t delta_sz =
          std::min(delta_list[di], std::max<size_t>(1, n / 2));
      std::vector<upsi::Item> add_set, del_set;
      BuildDeltaSets(n, delta_sz, add_set, del_set);

      std::vector<StatPoint> init_pts;
      init_pts.reserve(trials);
      for (size_t t = 0; t < trials; ++t) {
        upsi::InitialRoundResult out;
        upsi::RoundEvalStats stats;
        if (!upsi::RunInitialRoundWithStats(x_set, y_set, cfg, out, stats)) {
          std::cerr << "initial round failed: n=" << n << ", delta=" << delta_sz
                    << ", trial=" << t << "\n";
          return 1;
        }
        const auto naive = upsi::NaiveIntersection(x_set, y_set);
        if (naive != out.intersection) {
          std::cerr << "initial correctness mismatch: n=" << n
                    << ", delta=" << delta_sz << ", trial=" << t << "\n";
          return 1;
        }

        StatPoint p;
        p.total_ms = stats.total_ms;
        p.bucketize_ms = stats.bucketize_ms;
        p.delta_apply_ms = stats.delta_apply_ms;
        p.unchanged_filter_ms = stats.unchanged_filter_ms;
        p.sort_unique_ms = stats.sort_unique_ms;
        p.relic_init_ms = stats.relic_init_ms;
        p.relic_clean_ms = stats.relic_clean_ms;
        p.eval_total_ms = stats.eval_total_ms;
        p.p0_bucket_create_ms = stats.p0_bucket_create_ms;
        p.p1_sign_ms = stats.p1_sign_ms;
        p.p1_okvs_encode_ms = stats.p1_okvs_encode_ms;
        p.p0_okvs_decode_ms = stats.p0_okvs_decode_ms;
        p.p0_verify_ms = stats.p0_verify_ms;
        p.p1_online_ms = stats.p1_online_ms;
        p.p1_sign_items = stats.p1_sign_item_count;
        p.p0_verify_items = stats.p0_verify_item_count;
        FillPerItemTiming(p);
        FillBucketMinMax(x_base_buckets, y_base_buckets, p);
        p.p0_to_p1_ci_bytes = stats.p0_to_p1_ci_bytes;
        p.p1_to_p0_okvs_bytes = stats.p1_to_p0_okvs_bytes;
        p.p0_to_p1_result_bytes = stats.p0_to_p1_result_bytes;
        p.p0_to_p1_flag_bytes = stats.p0_to_p1_flag_bytes;
        p.p0_to_p1_bytes = stats.p0_to_p1_bytes;
        p.p1_to_p0_bytes = stats.p1_to_p0_bytes;
        p.total_comm_bytes = stats.total_comm_bytes;
        p.okvs_encode_attempts = stats.okvs_encode_attempts;
        p.okvs_self_checks = stats.okvs_self_checks;
        p.okvs_self_checks_full = stats.okvs_self_checks_full;
        p.okvs_self_checks_sampled = stats.okvs_self_checks_sampled;
        p.changed_bucket_count = stats.changed_bucket_count;
        p.intersection_size = out.intersection.size();
        init_pts.push_back(p);
      }
      const StatPoint init_med = MedianStat(init_pts);
      WriteCsvRow(ofs, "initial", n, cfg.bucket_cfg.bucket_count,
                  cfg.bucket_parallel_threads, trials, init_med,
                  delta_sz, cfg.okvs_bucket_slack,
                  cfg.enable_okvs_encode_robust_mode);

      upsi::UpdateProtocolState base_state;
      const auto t_setup_begin = std::chrono::steady_clock::now();
      if (!upsi::InitializeUpdateState(x_set, y_set, cfg, base_state)) {
        std::cerr << "init update-state failed: n=" << n << ", delta=" << delta_sz
                  << "\n";
        return 1;
      }
      const auto t_setup_end = std::chrono::steady_clock::now();
      const double update_setup_ms =
          std::chrono::duration<double, std::milli>(t_setup_end - t_setup_begin)
              .count();

      const auto naive_base = upsi::NaiveIntersection(x_set, y_set);
      if (naive_base != base_state.intersection) {
        std::cerr << "update base-state correctness mismatch: n=" << n
                  << ", delta=" << delta_sz << "\n";
        return 1;
      }

      std::vector<StatPoint> upd_pts;
      upd_pts.reserve(trials);
      for (size_t t = 0; t < trials; ++t) {
        upsi::UpdateProtocolState state = base_state;
        upsi::UpdateRoundResult upd_out;
        upsi::RoundEvalStats upd_stats;
        if (!upsi::RunUpdateRoundWithStats(state, add_set, del_set, upd_out,
                                           upd_stats)) {
          std::cerr << "update round failed: n=" << n << ", delta=" << delta_sz
                    << ", trial=" << t << "\n";
          return 1;
        }

        std::vector<upsi::Item> y_live = y_set;
        for (size_t i = 0; i < del_set.size(); ++i) {
          RemoveOne(y_live, del_set[i]);
        }
        for (size_t i = 0; i < add_set.size(); ++i) {
          y_live.push_back(add_set[i]);
        }
        const auto naive_upd = upsi::NaiveIntersection(x_set, y_live);
        if (naive_upd != upd_out.intersection) {
          std::cerr << "update correctness mismatch: n=" << n
                    << ", delta=" << delta_sz << ", trial=" << t << "\n";
          return 1;
        }

        StatPoint p;
        p.setup_ms = update_setup_ms;
        p.total_ms = upd_stats.total_ms;
        p.bucketize_ms = upd_stats.bucketize_ms;
        p.delta_apply_ms = upd_stats.delta_apply_ms;
        p.unchanged_filter_ms = upd_stats.unchanged_filter_ms;
        p.sort_unique_ms = upd_stats.sort_unique_ms;
        p.relic_init_ms = upd_stats.relic_init_ms;
        p.relic_clean_ms = upd_stats.relic_clean_ms;
        p.eval_total_ms = upd_stats.eval_total_ms;
        p.p0_bucket_create_ms = upd_stats.p0_bucket_create_ms;
        p.p1_sign_ms = upd_stats.p1_sign_ms;
        p.p1_okvs_encode_ms = upd_stats.p1_okvs_encode_ms;
        p.p0_okvs_decode_ms = upd_stats.p0_okvs_decode_ms;
        p.p0_verify_ms = upd_stats.p0_verify_ms;
        p.p1_online_ms = upd_stats.p1_online_ms;
        p.p1_sign_items = upd_stats.p1_sign_item_count;
        p.p0_verify_items = upd_stats.p0_verify_item_count;
        FillPerItemTiming(p);
        FillBucketMinMax(state.x_buckets, state.y_buckets, p);
        p.p0_to_p1_ci_bytes = upd_stats.p0_to_p1_ci_bytes;
        p.p1_to_p0_okvs_bytes = upd_stats.p1_to_p0_okvs_bytes;
        p.p0_to_p1_result_bytes = upd_stats.p0_to_p1_result_bytes;
        p.p0_to_p1_flag_bytes = upd_stats.p0_to_p1_flag_bytes;
        p.p0_to_p1_bytes = upd_stats.p0_to_p1_bytes;
        p.p1_to_p0_bytes = upd_stats.p1_to_p0_bytes;
        p.total_comm_bytes = upd_stats.total_comm_bytes;
        p.okvs_encode_attempts = upd_stats.okvs_encode_attempts;
        p.okvs_self_checks = upd_stats.okvs_self_checks;
        p.okvs_self_checks_full = upd_stats.okvs_self_checks_full;
        p.okvs_self_checks_sampled = upd_stats.okvs_self_checks_sampled;
        p.changed_bucket_count = upd_stats.changed_bucket_count;
        p.intersection_size = upd_out.intersection.size();
        upd_pts.push_back(p);
      }
      const StatPoint upd_med = MedianStat(upd_pts);
      WriteCsvRow(ofs, "update", n, cfg.bucket_cfg.bucket_count,
                  cfg.bucket_parallel_threads, trials, upd_med,
                  delta_sz, cfg.okvs_bucket_slack,
                  cfg.enable_okvs_encode_robust_mode);

      std::cout << "n=" << n << ", delta=" << delta_sz << " done: init_total_ms="
                << std::fixed << std::setprecision(3) << init_med.total_ms
                << ", upd_setup_ms=" << upd_med.setup_ms
                << ", upd_total_ms=" << upd_med.total_ms
                << ", upd_changed_buckets=" << upd_med.changed_bucket_count << "\n";
    }
  }

  std::cout << "UPSI 4.3.2 benchmark completed\n";
  return 0;
}
