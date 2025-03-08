#include "runtime/nxsan_internal.h"
#include "runtime/nxsan_runtime.h"

void* __nxsan_malloc(size_t size) {
  // todo
 (void)__nxsan_shadow_size;
 return nullptr;
}

void __nxsan_free(void* ptr) {
  // todo
}
