#include "runtime/nxsan_internal.h"
#include "runtime/nxsan_runtime.h"

// Access type codes for reporting.
#define NXSAN_ACCESS_TYPE_UNK 0
#define NXSAN_ACCESS_TYPE_LOAD 1
#define NXSAN_ACCESS_TYPE_STORE 2

// Verifies a multi-byte access to a given pointer.
static inline __attribute__((always_inline)) uint8_t
__nxsan_verify_access(void *ptr, uint8_t len) {
  uint8_t tag = __NXSAN_EXTRACT_TAG(ptr);

  // Strip tag.
  ptr = __NXSAN_REMOVE_TAG(ptr);

  // Any pointers (even those with no tags) are not allowed
  // to access the null page of memory (0x0-PAGE_SIZE).
  if ((uint64_t)ptr < __NXSAN_PAGE_SIZE_BYTES) {
    return __NXSAN_PTR_NULLPAGE;
  }

  // Exclude pointers with no tags.
  if (tag == 0) {
    return __NXSAN_PTR_NOTAG;
  }

  // Check that tagged pointer is within heap region.
  if (!__nxsan_ptr_in_heap_bounds(ptr)) {
    return __NXSAN_PTR_OUT_OF_HEAP;
  }

  // Is tag the same as shadow heap tag?
  uint8_t shadowTag = __nxsan_get_shadow_tag(ptr);
  if (shadowTag == tag) {
    return __NXSAN_PTR_OK;
  }

  // If the shadow tag is zero, this is unreserved memory.
  // Possible use-after-free.
  if (shadowTag == 0) {
    return __NXSAN_PTR_FREED;
  }

  // If the shadow tag cannot be a short granule, the pointer is out of
  // bounds for the original tag granule.
  if (shadowTag >= __NXSAN_TAG_GRANULARITY_BYTES) {
    return __NXSAN_PTR_BADTAG;
  }

  // Tag may be a short granule.
  // In this case, the tag is stored in the last byte of the real granule's
  // memory allocation. Attempt to retrieve the granule tag & compare.
  uint8_t *granuleStart =
      (uint8_t *)ptr - ((uint64_t)ptr % __NXSAN_TAG_GRANULARITY_BYTES);
  uint8_t *finalByte = granuleStart + (__NXSAN_TAG_GRANULARITY_BYTES - 1);
  uint8_t shortGranTag = *finalByte;

  if (shortGranTag != tag) {
    return __NXSAN_PTR_BADTAG;
  } else if (len <= 1) {
    return __NXSAN_PTR_OK;
  }

  // Access is more than one byte, which means we also need to verify the short
  // tag.
  bool bounded =
      len + ((uint64_t)ptr % __NXSAN_TAG_GRANULARITY_BYTES) <= shadowTag;
  return bounded ? __NXSAN_PTR_OK : __NXSAN_PTR_OVERRUN;
}

uint8_t __nxsan_verify_ptr(void *ptr) { return __nxsan_verify_access(ptr, 1); }

// Returns the given access type as a string.
static inline __attribute__((always_inline)) const char *
__nxsan_get_access_type_name(uint8_t accessType) {
  switch (accessType) {
  case NXSAN_ACCESS_TYPE_LOAD:
    return "load";
  case NXSAN_ACCESS_TYPE_STORE:
    return "store";
  default:
    return "(unk)";
  }
}

// Verifies the result returned by verify_ptr or verify_access.
// If an error is discovered, aborts with the appropriate error message.
static inline __attribute__((always_inline)) void
__nxsan_report_access(void *ptr, uint8_t size, uint8_t accessType) {
  // Don't check if not initialised yet.'
  if (!__nxsan_check_init()) {
    return;
  }

  // Verify access, handle errors.
  switch (__nxsan_verify_access(ptr, size)) {
  // Allow untagged accesses.
  case __NXSAN_PTR_OK:
  case __NXSAN_PTR_NOTAG:
    return;

  case __NXSAN_PTR_BADTAG:
    __nxsan_abort_with_access_err(
        ptr,
        "Tag mismatch for heap memory access (attempted %s of %u bytes) "
        "(nxsan-tag-mismatch).",
        __nxsan_get_access_type_name(accessType), size);
    return;

  case __NXSAN_PTR_FREED:
    __nxsan_abort_with_access_err(
        ptr,
        "Access to unallocated memory (attempted %s of %u bytes) "
        "(nxsan-use-after-free).",
        __nxsan_get_access_type_name(accessType), size);
    return;

  case __NXSAN_PTR_OUT_OF_HEAP:
    __nxsan_abort_with_access_err(ptr,
                                  "Access outside of heap (attempted %s of %u "
                                  "bytes) (nxsan-not-in-heap).",
                                  __nxsan_get_access_type_name(accessType),
                                  size);
    return;

  case __NXSAN_PTR_OVERRUN:
    __nxsan_abort_with_access_err(ptr,
                                  "Heap buffer overrun (attempted %s of %u "
                                  "bytes) (nxsan-heap-buffer-overflow).",
                                  __nxsan_get_access_type_name(accessType),
                                  size);
    return;

  case __NXSAN_PTR_NULLPAGE:
    __nxsan_abort_with_access_err(ptr,
                                  "Access at nullpage (attempted %s of %u "
                                  "bytes) (nxsan-heap-buffer-overflow).",
                                  __nxsan_get_access_type_name(accessType),
                                  size);
    return;

  default:
    __nxsan_abort_with_access_err(ptr,
                                  "Unimplemented access error (attempted %s of "
                                  "%u bytes) (nxsan-unimpl-err).",
                                  __nxsan_get_access_type_name(accessType),
                                  size);
    return;
  }
}

// External-facing instruments.
// clang-format off
extern "C" void __nxsan_report_load8  (void *p) { __nxsan_report_access(p, 1, NXSAN_ACCESS_TYPE_LOAD ); }
extern "C" void __nxsan_report_load16 (void *p) { __nxsan_report_access(p, 2, NXSAN_ACCESS_TYPE_LOAD ); }
extern "C" void __nxsan_report_load32 (void *p) { __nxsan_report_access(p, 4, NXSAN_ACCESS_TYPE_LOAD ); }
extern "C" void __nxsan_report_load64 (void *p) { __nxsan_report_access(p, 8, NXSAN_ACCESS_TYPE_LOAD ); }
extern "C" void __nxsan_report_store8 (void *p) { __nxsan_report_access(p, 1, NXSAN_ACCESS_TYPE_STORE); }
extern "C" void __nxsan_report_store16(void *p) { __nxsan_report_access(p, 2, NXSAN_ACCESS_TYPE_STORE); }
extern "C" void __nxsan_report_store32(void *p) { __nxsan_report_access(p, 4, NXSAN_ACCESS_TYPE_STORE); }
extern "C" void __nxsan_report_store64(void *p) { __nxsan_report_access(p, 8, NXSAN_ACCESS_TYPE_STORE); }
// clang-format on
