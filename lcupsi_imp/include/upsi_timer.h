#pragma once

#ifdef _WIN32
#include <windows.h>
#endif

#include <chrono>

namespace upsi {

inline double HighResNowMs() {
#ifdef _WIN32
  static LARGE_INTEGER freq = [] {
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    return f;
  }();
  LARGE_INTEGER c;
  QueryPerformanceCounter(&c);
  return 1000.0 * static_cast<double>(c.QuadPart) /
         static_cast<double>(freq.QuadPart);
#else
  using clock = std::chrono::steady_clock;
  return std::chrono::duration<double, std::milli>(clock::now().time_since_epoch())
      .count();
#endif
}

}  // namespace upsi
