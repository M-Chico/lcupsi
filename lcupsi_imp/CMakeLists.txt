cmake_minimum_required(VERSION 3.16)
project(upsi_imp LANGUAGES C CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(UPSI_BUILD_BENCHMARKS "Build UPSI benchmark executable when RELIC is available" ON)

# Use Release by default for single-config generators (e.g., MinGW Makefiles).
# Benchmarking cryptographic paths in non-optimized builds can be several times slower.
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(UPSI_RELIC_MAIN_DIR "" CACHE PATH "Path to RELIC source root")
set(UPSI_RELIC_TARGET_DIR "" CACHE PATH "Path to RELIC configured target/build directory")
set(UPSI_RELIC_STATIC_LIB "" CACHE FILEPATH "Path to RELIC static library, e.g. librelic_s.a")
set(UPSI_GMP_INCLUDE_DIR "" CACHE PATH "Optional path to GMP include directory")
set(UPSI_GMP_LIBRARY "" CACHE FILEPATH "Optional path to GMP library")

if(NOT UPSI_RELIC_STATIC_LIB AND UPSI_RELIC_TARGET_DIR)
  set(_upsi_relic_static_candidate "${UPSI_RELIC_TARGET_DIR}/lib/librelic_s.a")
  if(EXISTS "${_upsi_relic_static_candidate}")
    set(UPSI_RELIC_STATIC_LIB "${_upsi_relic_static_candidate}" CACHE FILEPATH
        "Path to RELIC static library, e.g. librelic_s.a" FORCE)
  endif()
endif()

set(UPSI_HAS_RELIC OFF)
if(TARGET relic_s)
  set(UPSI_HAS_RELIC ON)
elseif(UPSI_RELIC_STATIC_LIB AND EXISTS "${UPSI_RELIC_STATIC_LIB}")
  add_library(relic_s STATIC IMPORTED GLOBAL)
  set_target_properties(relic_s PROPERTIES IMPORTED_LOCATION "${UPSI_RELIC_STATIC_LIB}")
  set(UPSI_HAS_RELIC ON)
else()
  message(STATUS "RELIC is not configured; RELIC-dependent protocol targets are disabled.")
endif()

add_library(okvs STATIC
  third_party/okvs/src/bokvsv2.cpp
  third_party/okvs/src/galois128.cpp
)
target_include_directories(okvs PUBLIC third_party/okvs/include)

add_library(upsi_okvs_adapter STATIC
  src/upsi_okvs_adapter.cpp
)
target_include_directories(upsi_okvs_adapter PUBLIC
  include
  third_party/okvs/include
)
target_link_libraries(upsi_okvs_adapter PUBLIC okvs)

add_library(upsi_bucket STATIC
  src/upsi_bucket.cpp
)
target_include_directories(upsi_bucket PUBLIC include)

add_library(upsi_hash STATIC
  src/upsi_hash.cpp
)
target_include_directories(upsi_hash PUBLIC include third_party/okvs/include)

if(UPSI_HAS_RELIC)
  find_package(OpenMP QUIET)

  set(UPSI_RELIC_INCLUDE_DIRS include)
  if(UPSI_RELIC_MAIN_DIR)
    list(APPEND UPSI_RELIC_INCLUDE_DIRS "${UPSI_RELIC_MAIN_DIR}/include")
  endif()
  if(UPSI_RELIC_TARGET_DIR)
    list(APPEND UPSI_RELIC_INCLUDE_DIRS "${UPSI_RELIC_TARGET_DIR}/include")
  endif()

  set(UPSI_GMP_LINK_LIBRARY gmp)
  if(UPSI_GMP_LIBRARY)
    set(UPSI_GMP_LINK_LIBRARY "${UPSI_GMP_LIBRARY}")
  endif()

  add_library(upsi_relic_bridge STATIC
    src/upsi_relic_bridge.c
  )
  target_include_directories(upsi_relic_bridge PUBLIC ${UPSI_RELIC_INCLUDE_DIRS})
  if(UPSI_GMP_INCLUDE_DIR)
    target_include_directories(upsi_relic_bridge PUBLIC "${UPSI_GMP_INCLUDE_DIR}")
  endif()
  target_link_libraries(upsi_relic_bridge PUBLIC relic_s ${UPSI_GMP_LINK_LIBRARY})
  if(OpenMP_C_FOUND)
    target_link_libraries(upsi_relic_bridge PUBLIC OpenMP::OpenMP_C)
  endif()

  add_library(upsi_bucket_eval STATIC
    src/upsi_protocol_bucket_eval.cpp
  )
  target_include_directories(upsi_bucket_eval PUBLIC
    include
    third_party/okvs/include
    ${UPSI_RELIC_INCLUDE_DIRS}
  )
  target_link_libraries(upsi_bucket_eval PUBLIC
    upsi_hash
    upsi_okvs_adapter
    upsi_relic_bridge
  )

  add_library(upsi_protocol_initial STATIC
    src/upsi_protocol_initial.cpp
  )
  target_include_directories(upsi_protocol_initial PUBLIC
    include
    third_party/okvs/include
    ${UPSI_RELIC_INCLUDE_DIRS}
  )
  target_link_libraries(upsi_protocol_initial PUBLIC
    upsi_bucket
    upsi_bucket_eval
  )

  add_library(upsi_protocol_update STATIC
    src/upsi_protocol_update.cpp
  )
  target_include_directories(upsi_protocol_update PUBLIC
    include
    third_party/okvs/include
    ${UPSI_RELIC_INCLUDE_DIRS}
  )
  target_link_libraries(upsi_protocol_update PUBLIC
    upsi_bucket
    upsi_protocol_initial
    upsi_bucket_eval
    upsi_relic_bridge
  )
endif()

if(BUILD_TESTING)
  function(add_upsi_test target_name)
    add_executable(${target_name} ${ARGN})
    add_test(NAME ${target_name} COMMAND ${target_name})
  endfunction()

  add_upsi_test(test_okvs_blocksplit
    tests/test_okvs_blocksplit.cpp
  )
  target_link_libraries(test_okvs_blocksplit PRIVATE upsi_okvs_adapter)

  add_upsi_test(test_bucketization
    tests/test_bucketization.cpp
  )
  target_link_libraries(test_bucketization PRIVATE upsi_bucket)

  add_upsi_test(test_okvs_small_n_calibration
    tests/test_okvs_small_n_calibration.cpp
  )
  target_link_libraries(test_okvs_small_n_calibration PRIVATE upsi_okvs_adapter)

  add_upsi_test(test_okvs_param_policy
    tests/test_okvs_param_policy.cpp
  )
  target_link_libraries(test_okvs_param_policy PRIVATE upsi_okvs_adapter)

  if(UPSI_HAS_RELIC)
    add_upsi_test(test_pbpsi_m1
      tests/test_pbpsi_m1.c
    )
    target_include_directories(test_pbpsi_m1 PRIVATE ${UPSI_RELIC_INCLUDE_DIRS})
    if(UPSI_GMP_INCLUDE_DIR)
      target_include_directories(test_pbpsi_m1 PRIVATE "${UPSI_GMP_INCLUDE_DIR}")
    endif()
    target_link_libraries(test_pbpsi_m1 PRIVATE relic_s ${UPSI_GMP_LINK_LIBRARY})

    add_upsi_test(test_initial_round_small
      tests/test_initial_round_small.cpp
    )
    target_link_libraries(test_initial_round_small PRIVATE upsi_protocol_initial)

    add_upsi_test(test_delta_round_small
      tests/test_update_round_small.cpp
    )
    target_link_libraries(test_delta_round_small PRIVATE upsi_protocol_update)

    add_upsi_test(test_relic_bridge_consistency
      tests/test_relic_bridge_consistency.cpp
    )
    target_link_libraries(test_relic_bridge_consistency PRIVATE upsi_relic_bridge)
  endif()
endif()

if(UPSI_HAS_RELIC AND UPSI_BUILD_BENCHMARKS)
  add_executable(upsi_exp_4_3_2_bench
    src/upsi_exp_4_3_2_bench.cpp
  )
  target_link_libraries(upsi_exp_4_3_2_bench PRIVATE
    upsi_protocol_update
    upsi_protocol_initial
  )
endif()
