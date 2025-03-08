#include "runtime/nxsan_internal.h"
#include "runtime/nxsan_runtime.h"

#include <cstdlib>

// nxsan shadow memory store, size
uint8_t* __nxsan_shadow = nullptr;
size_t __nxsan_shadow_size = 0;

// nsan heap base
uint8_t* __nxsan_heap_base = nullptr;

extern "C" void __nxsan_init(void* hBase, size_t hSize) {
  if (__nxsan_check_init()) { return; }

  // Configure heap base & shadow region.
  __nxsan_shadow_size = hSize / __NXSAN_TAG_GRANULARITY_BYTES;
  __nxsan_shadow = (uint8_t*)std::calloc(1, __nxsan_shadow_size);
  __nxsan_heap_base = (uint8_t*)hBase;

  // Report an error if allocation fails.
  if (!__nxsan_shadow) {
    __nxsan_abort_with_err("Failed to allocate nxsan shadow memory of size %zu.", __nxsan_shadow_size);
    return;
  }

  // Initialise the tag generator.
  __nxsan_init_tag_gen();
}

extern "C" void __nxsan_terminate() {
  if (!__nxsan_check_init()) { return; }

  // Verify that all allocations have been de-allocated.
  // ...

  // Free shadow region.
  std::free(__nxsan_shadow);
  __nxsan_shadow_size = 0;
  __nxsan_heap_base = nullptr;
}
