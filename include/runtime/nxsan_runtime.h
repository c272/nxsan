#pragma once
#ifndef __NXSAN_RUNTIME_H
#define __NXSAN_RUNTIME_H

// nxsan runtime library definitions
// Author: c272

#include <cstddef>

/***************************
 * Application interface.  *
 ***************************/

// Initialises nxsan shadow memory for a given heap base address and size.
// Returns whether nxsan was initialised successfully from the method call.
extern "C" bool __nxsan_init(void* hBase, size_t hSize);

// Cleans up the nxsan shadow memory.
// If any memory regions are still marked as allocated, an error is generated.
// Returns whether nxsan was terminated successfully from the method call.
extern "C" bool __nxsan_terminate();

// Allocates size bytes of uninitialized shadow-memory tracked storage.
// If allocation succeeds, returns a pointer to the lowest (first) byte in the allocated
// memory block that is suitably aligned for any scalar type (at least as strictly as std::max_align_t)
// If size is zero, the call will be treated as an illegal operation.
//   * If the allocated storage has not been freed with __nxsan_free(void*) when __nxsan_terminate
//     is called, an error will be generated.
//   * Shadow-memory tracking will not function if the top byte of the returned pointer is cleared.
extern "C" void* __nxsan_malloc(size_t size);

// Deallocates the space previously allocated by __nxsan_malloc(size_t).
//   * If ptr is a null pointer, it will be treated as an illegal operation.
//   * The behavior is undefined if the value of ptr does not equal a value returned
//     earlier by __nxsan_malloc(size_t).
extern "C" void __nxsan_free(void* ptr);

/***************************************
 * Reporting functions for sizes 8-64. *
 ***************************************/

// Don't mark reporting calls as inline if requested by including source.
#ifdef __NXSAN_OUTLINE_REPORTING
#define __NXSAN_LD_STR_REPORT_FOR_SIZE(x) \
  extern "C" void __nxsan_report_load##x(void* p); \
  extern "C" void __nxsan_report_store##x(void* p);
#else
#define __NXSAN_LD_STR_REPORT_FOR_SIZE(x) \
  extern "C" __attribute__((always_inline)) void __nxsan_report_load##x(void* p); \
  extern "C" __attribute__((always_inline)) void __nxsan_report_store##x(void* p);
#endif

__NXSAN_LD_STR_REPORT_FOR_SIZE(8)
__NXSAN_LD_STR_REPORT_FOR_SIZE(16)
__NXSAN_LD_STR_REPORT_FOR_SIZE(32)
__NXSAN_LD_STR_REPORT_FOR_SIZE(64)

#endif
