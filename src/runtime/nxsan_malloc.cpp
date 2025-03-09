#include "runtime/nxsan_internal.h"
#include "runtime/nxsan_runtime.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>

// Allocation byte size threshold for avoiding tag values of <TG.
// When tag values are <TG, detection of use-after-free becomes very difficult
// when application code accesses past the first granule of the freed
// allocation. We can somewhat mitigate the effects of this by avoiding small
// tag values for large allocations.
#define __NXSAN_AVOID_SMALL_TAG_THRESH 256

// Tag generator logic.
static std::random_device __nxsan_rd;
static std::mt19937 __nxsan_mt_gen;
std::uniform_int_distribution<short> __nxsan_tag_dist;

void __nxsan_init_tag_gen() {
  __nxsan_mt_gen = std::mt19937(__nxsan_rd());
  __nxsan_tag_dist =
      std::uniform_int_distribution<short>(1, __NXSAN_TAG_MAX_VAL);
}

// Generates an N-bit pointer tag for the given allocation.
//   - Tag bits are stored in the bottom N bits of the returned value.
//   - Possible values are between 1-255.
// Guaranteed to generate a tag which is different to the preceeding and
// proceeding shadow memory regions.
static inline __attribute__((always_inline)) uint8_t
__nxsan_generate_tag(void *ptr, size_t size) {
  // Fetch the shadow tag preceding this alloc.
  uint8_t *prevShadowPtr = __nxsan_get_shadow_address(ptr) - 1;
  uint8_t prevShadowTag = 0;
  if (prevShadowPtr >= __nxsan_shadow) {
    prevShadowTag = *prevShadowPtr;
  }

  // Fetch the shadow tag following this alloc.
  uint8_t *allocTail = (uint8_t *)ptr + size;
  uint8_t *nextShadowPtr = __nxsan_get_shadow_address(allocTail) + 1;
  uint8_t nextShadowTag = 0;
  if (nextShadowPtr < __nxsan_shadow + __nxsan_shadow_size) {
    nextShadowTag = *nextShadowPtr;
  }

  // Determine whether we must avoid small tag values for this alloc.
  bool avoidSmallTag = size >= __NXSAN_AVOID_SMALL_TAG_THRESH;

  // Generate tag, ensuring it is not the same as the prior/next tag.
  // If the tag is <TG and we must avoid small tags, also re-generate.
  uint8_t tag;
  do {
    tag = ((uint8_t)__nxsan_tag_dist(__nxsan_mt_gen));
  } while (tag == prevShadowTag || tag == nextShadowTag ||
           (avoidSmallTag && tag < __NXSAN_TAG_GRANULARITY_BYTES));

  return tag;
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

// Clears the shadow tag in memory for the given pointer.
// Since we don't know the size of the allocation at the point of free, we must
// make some concessions on how accurate we can be about clearing tagged shadow
// memory.
//  * We cannot clear trailing short granules in >1 granule allocations, as this
//    is just as likely to be the next tag in allocated memory.
//  * We cannot clear past the first granule if the tag value is <TG, as it is
//    possible the next value is a short granule which equals the current tag.
//  * We *can* clear up until the tag value changes if the tag is >=TG, as
//    consecutive tags are guaranteed to differ meaning two identical tags will
//    never line up in shadow memory.
// To combat the above limitations, for larger allocations we deliberately avoid
// tag values between 1 and TG-1 (threshold configurable above).
static inline __attribute__((always_inline)) void
__nxsan_clear_shadow_tag(void *ptr, uint8_t tag) {
  // Clear the tag value of the first granule.
  uint8_t *shadowAddr = __nxsan_get_shadow_address(ptr);
  uint8_t origTag = *shadowAddr;
  *shadowAddr = 0x0;

  // If the original tag was a short tag, the allocation was <TG bytes.
  // Thus, we have cleared all of the relevant shadow bytes.
  if (origTag != tag) {
    return;
  }

  // If the tag is <TG, we cannot do anything more (see above).
  if (tag < __NXSAN_TAG_GRANULARITY_BYTES) {
    return;
  }

  // Clear up until the tag value differs. We also can't clear our own final
  // short granule, so don't bother checking for that.
  ++shadowAddr;
  while (shadowAddr < __nxsan_shadow + __nxsan_shadow_size &&
         *shadowAddr == tag) {
    *shadowAddr = 0x0;
    ++shadowAddr;
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
  void *ptr = __NXSAN_INTERNAL_ALIGNED_ALLOC(__NXSAN_TAG_GRANULARITY_BYTES,
                                             alignedSize);
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
  uint8_t tag = __nxsan_generate_tag(ptr, size);
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
  if (!__nxsan_ptr_in_heap_bounds(ptrNoTag)) {
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
      __nxsan_abort_with_access_err(
          ptr, "Attempted to free memory with no tag (nxsan-notag-free).");
      return;

    case __NXSAN_PTR_BADTAG:
      __nxsan_abort_with_access_err(
          ptr, "Attempted to free memory with bad tag (nxsan-badtag-free).");
      return;

    case __NXSAN_PTR_FREED:
      __nxsan_abort_with_access_err(
          ptr, "Attempted to free unallocated memory (nxsan-double-free).");
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
  __NXSAN_INTERNAL_FREE(ptrNoTag);

  // Remove the tag in shadow memory (set to 0x0).
  // This isn't perfect, see function comment for details.
  __nxsan_clear_shadow_tag(ptrNoTag, tag);
}
