#pragma once

#include <vector>

#include "upsi_bucket.h"
#include "upsi_metrics.h"
#include "upsi_protocol_initial.h"

namespace upsi {

// Process one (x_bucket, y_bucket) pair and append matches to out_intersection.
// Requires RELIC global context already initialized by caller.
bool EvalBucketIntersectionNoInit(const Bucket& x_bucket, const Bucket& y_bucket,
                                  const InitialRoundConfig& cfg,
                                  std::vector<Item>& out_intersection,
                                  BucketEvalStats* stats = nullptr);

}  // namespace upsi
