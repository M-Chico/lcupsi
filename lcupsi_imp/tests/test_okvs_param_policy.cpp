#include <iostream>

#include "upsi_okvs_param_policy.h"

int main() {
  const size_t ns[] = {1, 8, 16, 31, 32, 33, 64, 65, 128, 129, 256, 257,
                       512, 1024, 2048, 4096, 8192};

  for (size_t i = 0; i < sizeof(ns) / sizeof(ns[0]); ++i) {
    const size_t n_raw = ns[i];
    const size_t n_pad = upsi::PaddedOkvsN(n_raw);
    const upsi::OkvsConfig cfg = upsi::SelectOkvsConfigByN(n_pad);

    if (!upsi::IsOkvsParamShapeValid(n_pad, cfg)) {
      std::cerr << "Invalid param shape for n_raw=" << n_raw
                << " n_pad=" << n_pad << " w=" << cfg.w << " e=" << cfg.e
                << '\n';
      return 1;
    }
  }

  std::cout << "OKVS parameter policy test passed\n";
  return 0;
}

