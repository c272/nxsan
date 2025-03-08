#include "runtime/nxsan_internal.h"
#include "runtime/nxsan_runtime.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>

// Tag generator logic.
static std::random_device __nxsan_rd;
static std::mt19937 __nxsan_mt_gen;
std::uniform_int_distribution<short> __nxsan_tag_dist;

void __nxsan_init_tag_gen() {
  __nxsan_mt_gen = std::mt19937(__nxsan_rd());
  __nxsan_tag_dist =
      std::uniform_int_distribution<short>(1, __NXSAN_TAG_MAX_VAL);
}

// Generates an N-bit tag for use as a pointer tag.
//   - Tag bits are stored in the bottom N bits of the returned value.
//   - Possible values are between 1-255.
static inline __attribute__((always_inline)) uint8_t
__nxsan_generate_tag(void) {
  return ((uint8_t)__nxsan_tag_dist(__nxsan_mt_gen));
}

// Updates shadow memory to reflect the given tagged allocation for a set size.
// The given allocation must be verified to be within tracked heap bounds before
// calling. Behavior when out-of-bounds allocations are passed is undefined.
static inline __attribute__((always_inline)) void
__nxsan_set_shadow_tag(void *ptr, size_t size, size_t allocated) {
  uint8_t *shadowAddr = __nxsan_get_shadow_address(ptr);

  // Set *up to* the final shadow byte to the tag.
  size_t shadowSize = std::max(allocated / __NXSAN_TAG_GRANULARITY_BYTES, 1ul);
  uint8_t tag = __NXSAN_EXTRACT_TAG(ptr);
  for (int i = 0; i < shadowSize - 1; ++i) {
    *(shadowAddr + i) = tag;
  }

  // If the allocation is not a multiple of the tag granularity, then we need to
  // use a short granule to track the partial allocation in the final shadow
  // byte. See:
  // https://clang.llvm.org/docs/HardwareAssistedAddressSanitizerDesign.html
  uint8_t *lastShadowAddr = shadowAddr + (shadowSize - 1);
  if (size % __NXSAN_TAG_GRANULARITY_BYTES > 0) {
    // Set short granule.
    uint8_t lastShadowVal = (uint8_t)(size % __NXSAN_TAG_GRANULARITY_BYTES);
    *lastShadowAddr = lastShadowVal;

    // Store tag in final byte of real allocation granule.
    uint8_t *finalByte = (uint8_t *)ptr + (allocated - 1);
    *finalByte = tag;
  } else {
    // Allocation is perfectly aligned with tag granularity.
    // Set final tag byte directly to the tag.
    *lastShadowAddr = tag;
  }
}

extern "C" void *__nxsan_malloc(size_t size) {
  if (!__nxsan_check_init()) {
    // Not initialised, cannot malloc.
    __nxsan_abort_with_err("nxsan is not initialised, cannot allocate memory "
                           "(nxsan-noinit-alloc).");
    return nullptr;
  }

  // If the size is zero, treat it as an error.
  if (size == 0) {
    __nxsan_abort_with_err("Attempted to allocate size 0 (nxsan-alloc-zero).");
    return nullptr;
  }

  // Allocate memory of the given size.
  // The memory location and size must both be aligned to the tag granularity
  // to:
  // - Ensure no collision of allocations in shadow memory.
  // - Ensure the short granule can always be stored in the last byte of an
  // allocated granule.
  size_t alignedSize = size + __NXSAN_TAG_GRANULARITY_BYTES -
                       (size % __NXSAN_TAG_GRANULARITY_BYTES);
  void *ptr = std::aligned_alloc(__NXSAN_TAG_GRANULARITY_BYTES, alignedSize);
  if (!ptr) {
    // Failed to allocate memory.
    __nxsan_abort_with_err("Failed to allocate memory of size %zu (real "
                           "allocate size %zu) (nxsan-alloc-fail).",
                           size, alignedSize);
    return nullptr;
  }

  // If the returned allocation falls outside of tracked memory, we can't tag
  // it.
  if (!__nxsan_alloc_in_heap_bounds(ptr, size)) {
    __nxsan_abort_with_err(
        "Allocation fell outside of tracked heap bounds: [%p, %p) outside of "
        "range [%p, %p) (nxsan-alloc-oob).",
        ptr, (uint8_t *)ptr + size, __nxsan_shadow,
        __nxsan_shadow + __nxsan_shadow_size);
    return nullptr;
  }

  // Generate a random tag for the pointer, update shadow memory.
  uint8_t tag = __nxsan_generate_tag();
  ptr = __NXSAN_EMPLACE_TAG(ptr, tag);

  // Update shadow memory for the given tag.
  __nxsan_set_shadow_tag(ptr, size, alignedSize);

  return ptr;
}

extern "C" void __nxsan_free(void *ptr) {
  if (!__nxsan_check_init()) {
    // Not initialised, cannot malloc.
    __nxsan_abort_with_access_err(ptr,
                                  "nxsan is not initialised, but attempted to "
                                  "free memory (nxsan-noinit-free).");
    return;
  }

  // Is the given pointer within the heap bounds?
  void *ptrNoTag = __NXSAN_REMOVE_TAG(ptr);
  if (!__nxsan_ptr_in_heap_bounds(ptr)) {
    __nxsan_abort_with_access_err(ptr,
                                  "Attempted to free pointer outside of heap "
                                  "bounds [%p, %p) (nxsan-oob-free).",
                                  __nxsan_shadow,
                                  __nxsan_shadow + __nxsan_shadow_size);
    return;
  }

  // If the pointer is unaligned, something has gone horribly wrong.
  // Someone is trying to free memory from halfway through the allocation...
  if ((uint64_t)ptrNoTag % __NXSAN_TAG_GRANULARITY_BYTES > 0) {
    __nxsan_abort_with_access_err(
        ptr, "Attempted to free unaligned pointer (nxsan-unaligned-free).");
    return;
  }

  // Stop someone trying to free the shadow memory (WTF?).
  if (ptrNoTag == __nxsan_shadow) {
    __nxsan_abort_with_access_err(
        ptr, "Attempted to free nxsan shadow memory (seriously?).",
        __nxsan_shadow);
    return;
  }

  // Verify the tag within the pointer to free.
  uint8_t tag = __NXSAN_EXTRACT_TAG(ptr);
  uint8_t result = __nxsan_verify_ptr(ptr);
  if (result != __NXSAN_PTR_OK) {
    switch (result) {
    case __NXSAN_PTR_NOTAG:
      return;

    case __NXSAN_PTR_BADTAG:
      __nxsan_abort_with_access_err(
          ptr, "Attempted to free with stale pointer (nxsan-double-free).");
      return;

    // Attempted to free from the null page.
    case __NXSAN_PTR_NULLPAGE:
      __nxsan_abort_with_access_err(
          ptr, "Attempted to free from the null page (nxsan-nullpage-free).");
      return;

    // We already check for these results beforehand, so they should be
    // unreachable.
    case __NXSAN_PTR_OUT_OF_HEAP:
    case __NXSAN_PTR_OVERRUN:
      __nxsan_abort_with_access_err(
          ptr, "Unreachable internal error (nxsan-unreachable-free).");
      return;

    default:
      __nxsan_abort_with_access_err(
          ptr, "Unimplemented tag error (nxsan-unimpl-tag).");
      return;
    }
  }

  // Free the underlying heap memory.
  std::free(ptrNoTag);
}
