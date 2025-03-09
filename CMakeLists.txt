cmake_minimum_required(VERSION 3.20.0)
project(nxsan)

# If a local install of zlib/llvm-devel exists, try to use that.
find_package(ZLIB)
find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# Generate compile commands for clangd.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

# Configure target for instrumentation tool.
set(NXSAN_INS_TARGET nxsan-instrumentation-cxx)
add_executable(${NXSAN_INS_TARGET}
    src/instrumentation/main.cpp
    src/instrumentation/AccessInstrumenter.cpp
    src/instrumentation/CliArguments.cpp
)
target_include_directories(${NXSAN_INS_TARGET} PRIVATE ${PROJECT_SOURCE_DIR}/include)
set_property(TARGET ${NXSAN_INS_TARGET} PROPERTY CXX_STANDARD 17)

# Find the libraries that correspond to the LLVM components
# that we wish to use
llvm_map_components_to_libnames(llvm_libs support core irreader)

# Link against LLVM libraries
target_link_libraries(${NXSAN_INS_TARGET} ${llvm_libs})

# Configure target for runtime library.
set(NXSAN_RT_TARGET nxsan-rt)
add_library(${NXSAN_RT_TARGET}
  src/runtime/nxsan_bt.cpp
  src/runtime/nxsan_init.cpp
  src/runtime/nxsan_malloc.cpp
  src/runtime/nxsan_report.cpp
  src/runtime/nxsan_utils.cpp
)
target_include_directories(${NXSAN_RT_TARGET} PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_compile_options(${NXSAN_RT_TARGET} PRIVATE -Wno-attributes)

# Configure tests.
option(BUILD_NXSAN_TESTS "Builds tests for verifying nxsan." OFF)
if (BUILD_NXSAN_TESTS)
  # Turn off useless googletest elements.
  set(BUILD_GMOCK OFF)
  set(INSTALL_GTEST OFF)
  set(GTEST_HAS_ABSL OFF)

  # For Windows: Prevent overriding the parent project's compiler/linker settings
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  add_subdirectory(thirdparty/googletest)

  # Testing on.
  enable_testing()

  # Configure test target.
  set(NXSAN_TESTS nxsan-tests)
  add_executable(${NXSAN_TESTS}
      tests/runtime/init_tests.cpp
      tests/runtime/malloc_tests.cpp
      tests/runtime/report_tests.cpp
  )
  target_include_directories(${NXSAN_TESTS} PRIVATE ${PROJECT_SOURCE_DIR}/include)
  target_compile_options(${NXSAN_TESTS} PRIVATE -Wno-attributes)
  target_link_libraries(${NXSAN_TESTS} ${NXSAN_RT_TARGET} GTest::gtest_main)

  # Discover tests.
  include(GoogleTest)
  gtest_discover_tests(${NXSAN_TESTS})
endif()
