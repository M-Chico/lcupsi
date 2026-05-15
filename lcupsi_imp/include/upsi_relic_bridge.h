#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct upsi_relic_bucket_ctx upsi_relic_bucket_ctx;

int upsi_relic_global_init(void);
void upsi_relic_global_clean(void);

size_t upsi_relic_default_cy_bytes(void);
size_t upsi_relic_default_v_bytes(void);
size_t upsi_relic_default_ci_bytes(void);

upsi_relic_bucket_ctx* upsi_relic_bucket_create(const uint64_t* x, size_t nx);
void upsi_relic_bucket_destroy(upsi_relic_bucket_ctx* ctx);

int upsi_relic_bucket_sign(const upsi_relic_bucket_ctx* ctx, const uint64_t* y,
                           size_t ny, uint8_t* out_payload,
                           size_t payload_stride, size_t cy_bytes,
                           size_t vy_bytes);

int upsi_relic_bucket_verify_x(const upsi_relic_bucket_ctx* ctx, size_t x_index,
                               const uint8_t* payload, size_t cy_bytes,
                               size_t vy_bytes, int* is_match);

#ifdef __cplusplus
}
#endif
