#include "upsi_relic_bridge.h"

#include <stdlib.h>
#include <string.h>

#include "relic.h"

struct upsi_relic_bucket_ctx {
  size_t nx;
  bn_t q;
  bn_t sk;
  bn_t r;
  g1_t ss;
  g2_t* s;
  g2_t* d;
};

static void upsi_bn_set_u64(bn_t out, uint64_t v) {
  uint8_t be[8];
  for (size_t i = 0; i < 8; ++i) {
    be[7 - i] = (uint8_t)((v >> (8 * i)) & 0xFFU);
  }
  bn_read_bin(out, be, (int)sizeof(be));
}

int upsi_relic_global_init(void) {
  if (core_init() != RLC_OK) {
    return 0;
  }
  if (pc_param_set_any() != RLC_OK) {
    core_clean();
    return 0;
  }
  return 1;
}

void upsi_relic_global_clean(void) { core_clean(); }

size_t upsi_relic_default_cy_bytes(void) { return (size_t)(RLC_PC_BYTES + 1); }

size_t upsi_relic_default_v_bytes(void) { return (size_t)RLC_MD_LEN; }

size_t upsi_relic_default_ci_bytes(void) { return (size_t)(4 * RLC_PC_BYTES + 1); }

upsi_relic_bucket_ctx* upsi_relic_bucket_create(const uint64_t* x, size_t nx) {
  upsi_relic_bucket_ctx* ctx = NULL;
  bn_t* x_bn = NULL;

  ctx = (upsi_relic_bucket_ctx*)malloc(sizeof(upsi_relic_bucket_ctx));
  if (ctx == NULL) {
    return NULL;
  }
  memset(ctx, 0, sizeof(*ctx));
  ctx->nx = nx;

  bn_null(ctx->q);
  bn_null(ctx->sk);
  bn_null(ctx->r);
  g1_null(ctx->ss);

  bn_new(ctx->q);
  bn_new(ctx->sk);
  bn_new(ctx->r);
  g1_new(ctx->ss);

  ctx->s = (g2_t*)calloc(nx + 1, sizeof(g2_t));
  ctx->d = (g2_t*)calloc(nx + 1, sizeof(g2_t));
  if (ctx->s == NULL || ctx->d == NULL) {
    upsi_relic_bucket_destroy(ctx);
    return NULL;
  }
  for (size_t i = 0; i <= nx; ++i) {
    g2_null(ctx->s[i]);
    g2_null(ctx->d[i]);
    g2_new(ctx->s[i]);
    g2_new(ctx->d[i]);
  }

  x_bn = (bn_t*)calloc(nx, sizeof(bn_t));
  if (x_bn == NULL) {
    upsi_relic_bucket_destroy(ctx);
    return NULL;
  }
  for (size_t i = 0; i < nx; ++i) {
    bn_null(x_bn[i]);
    bn_new(x_bn[i]);
    upsi_bn_set_u64(x_bn[i], x[i]);
  }

  pc_get_ord(ctx->q);
  if (cp_pbpsi_gen(ctx->sk, ctx->ss, ctx->s, nx) != RLC_OK ||
      cp_pbpsi_ask(ctx->d, ctx->r, x_bn, ctx->s, nx) != RLC_OK) {
    for (size_t i = 0; i < nx; ++i) {
      bn_free(x_bn[i]);
    }
    free(x_bn);
    upsi_relic_bucket_destroy(ctx);
    return NULL;
  }

  for (size_t i = 0; i < nx; ++i) {
    bn_free(x_bn[i]);
  }
  free(x_bn);
  return ctx;
}

void upsi_relic_bucket_destroy(upsi_relic_bucket_ctx* ctx) {
  if (ctx == NULL) {
    return;
  }
  bn_free(ctx->q);
  bn_free(ctx->sk);
  bn_free(ctx->r);
  g1_free(ctx->ss);
  if (ctx->s != NULL) {
    for (size_t i = 0; i <= ctx->nx; ++i) {
      g2_free(ctx->s[i]);
    }
    free(ctx->s);
  }
  if (ctx->d != NULL) {
    for (size_t i = 0; i <= ctx->nx; ++i) {
      g2_free(ctx->d[i]);
    }
    free(ctx->d);
  }
  free(ctx);
}

int upsi_relic_bucket_sign(const upsi_relic_bucket_ctx* ctx, const uint64_t* y,
                           size_t ny, uint8_t* out_payload,
                           size_t payload_stride, size_t cy_bytes,
                           size_t vy_bytes) {
  bn_t tj, y_bn;
  g1_t g1_tmp, y_gen, cy;
  gt_t tpair;
  int ok = 1;

  if (ctx == NULL || y == NULL || out_payload == NULL) {
    return 0;
  }
  if (payload_stride < cy_bytes + vy_bytes) {
    return 0;
  }

  bn_null(tj);
  bn_null(y_bn);
  g1_null(g1_tmp);
  g1_null(y_gen);
  g1_null(cy);
  gt_null(tpair);

  bn_new(tj);
  bn_new(y_bn);
  g1_new(g1_tmp);
  g1_new(y_gen);
  g1_new(cy);
  gt_new(tpair);

  for (size_t i = 0; i < ny && ok; ++i) {
    upsi_bn_set_u64(y_bn, y[i]);
    bn_rand_mod(tj, ctx->q);

    g1_mul_gen(g1_tmp, tj);
    pc_map(tpair, g1_tmp, ctx->d[0]);

    g1_mul_gen(y_gen, y_bn);
    g1_sub(cy, ctx->ss, y_gen);
    g1_mul(cy, cy, tj);

    uint8_t* dst = out_payload + i * payload_stride;
    g1_write_bin(dst, (int)cy_bytes, cy, 1);

    uint8_t e_bytes[12 * RLC_PC_BYTES];
    uint8_t v_full[RLC_MD_LEN];
    gt_write_bin(e_bytes, sizeof(e_bytes), tpair, 0);
    md_map(v_full, e_bytes, sizeof(e_bytes));
    memcpy(dst + cy_bytes, v_full, vy_bytes);
  }

  bn_free(tj);
  bn_free(y_bn);
  g1_free(g1_tmp);
  g1_free(y_gen);
  g1_free(cy);
  gt_free(tpair);
  return ok;
}

int upsi_relic_bucket_verify_x(const upsi_relic_bucket_ctx* ctx, size_t x_index,
                               const uint8_t* payload, size_t cy_bytes,
                               size_t vy_bytes, int* is_match) {
  g1_t cy;
  gt_t echeck;
  int parse_ok = 1;

  if (ctx == NULL || payload == NULL || is_match == NULL) {
    return 0;
  }
  if (x_index >= ctx->nx) {
    return 0;
  }
  *is_match = 0;
  // Fast reject for random bytes from non-keys to avoid frequent RELIC parser errors.
  if (cy_bytes == 0) {
    return 1;
  }
  {
    const uint8_t tag = payload[0];
    // We serialize G1 in compressed form (g1_write_bin(..., 1)),
    // so valid tag bytes are expected to be 0x02 or 0x03.
    if (!(tag == 0x02 || tag == 0x03)) {
      return 1;
    }
  }

  g1_null(cy);
  gt_null(echeck);
  g1_new(cy);
  gt_new(echeck);

  RLC_TRY {
    g1_read_bin(cy, payload, (int)cy_bytes);
  }
  RLC_CATCH_ANY {
    parse_ok = 0;
  }
  RLC_FINALLY {
  }

  if (!parse_ok) {
    g1_free(cy);
    gt_free(echeck);
    return 1;
  }

  if (!g1_is_valid(cy)) {
    g1_free(cy);
    gt_free(echeck);
    return 1;
  }

  pc_map(echeck, cy, ctx->d[x_index + 1]);
  if (gt_is_unity(echeck)) {
    g1_free(cy);
    gt_free(echeck);
    return 1;
  }

  uint8_t e_bytes[12 * RLC_PC_BYTES];
  uint8_t v_full[RLC_MD_LEN];
  gt_write_bin(e_bytes, sizeof(e_bytes), echeck, 0);
  md_map(v_full, e_bytes, sizeof(e_bytes));
  if (memcmp(payload + cy_bytes, v_full, vy_bytes) == 0) {
    *is_match = 1;
  }

  g1_free(cy);
  gt_free(echeck);
  return 1;
}
