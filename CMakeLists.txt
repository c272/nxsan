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
