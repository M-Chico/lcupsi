#include <stdio.h>

#include "relic.h"

int main(void) {
  bn_t q, r, x[1], y[1], z[1];
  g1_t ss, u[1];
  g2_t d[2], s[2];
  gt_t t[1];
  size_t inter_len = 0;
  int ok = 0;

  bn_null(q);
  bn_null(r);
  bn_null(x[0]);
  bn_null(y[0]);
  bn_null(z[0]);
  g1_null(ss);
  g1_null(u[0]);
  g2_null(d[0]);
  g2_null(d[1]);
  g2_null(s[0]);
  g2_null(s[1]);
  gt_null(t[0]);

  if (core_init() != RLC_OK) {
    printf("core_init fail\n");
    return 1;
  }
  if (pc_param_set_any() != RLC_OK) {
    printf("pc_param_set_any fail\n");
    core_clean();
    return 1;
  }

  bn_new(q);
  bn_new(r);
  bn_new(x[0]);
  bn_new(y[0]);
  bn_new(z[0]);
  g1_new(ss);
  g1_new(u[0]);
  g2_new(d[0]);
  g2_new(d[1]);
  g2_new(s[0]);
  g2_new(s[1]);
  gt_new(t[0]);

  pc_get_ord(q);
  bn_set_dig(x[0], 12345);
  bn_copy(y[0], x[0]);

  if (cp_pbpsi_gen(q, ss, s, 1) != RLC_OK) {
    printf("cp_pbpsi_gen fail\n");
    goto cleanup;
  }
  if (cp_pbpsi_ask(d, r, x, s, 1) != RLC_OK) {
    printf("cp_pbpsi_ask fail\n");
    goto cleanup;
  }
  if (cp_pbpsi_ans(t, u, ss, d[0], y, 1) != RLC_OK) {
    printf("cp_pbpsi_ans fail\n");
    goto cleanup;
  }
  if (cp_pbpsi_int(z, &inter_len, d, x, 1, t, u, 1) != RLC_OK) {
    printf("cp_pbpsi_int fail\n");
    goto cleanup;
  }

  printf("m1 inter_len=%llu\n", (unsigned long long)inter_len);
  // Reproduces RELIC PBPSI edge case at m=1.
  ok = (inter_len == 0);

cleanup:
  bn_free(q);
  bn_free(r);
  bn_free(x[0]);
  bn_free(y[0]);
  bn_free(z[0]);
  g1_free(ss);
  g1_free(u[0]);
  g2_free(d[0]);
  g2_free(d[1]);
  g2_free(s[0]);
  g2_free(s[1]);
  gt_free(t[0]);
  core_clean();
  return ok ? 0 : 1;
}
