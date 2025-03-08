#include "runtime/nxsan_internal.h"
#include "runtime/nxsan_runtime.h"

#include <cstdlib>

// nxsan shadow memory store, size
uint8_t* __nxsan_shadow = nullptr;
size_t __nxsan_shadow_size = 0;

void __nxsan_init(size_t hSize) {
  if (__nxsan_check_init()) { return; }
  // todo: mmap NX heap region
  __nxsan_shadow_size = hSize / __NXSAN_TAG_GRANULARITY_BYTES;
  __nxsan_shadow = (uint8_t*)std::calloc(1, __nxsan_shadow_size);
}

void __nxsan_terminate() {
  if (!__nxsan_check_init()) { return; }
  std::free(__nxsan_shadow);
}
