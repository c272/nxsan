#pragma once
#ifndef __NXSAN_RUNTIME_INTERNAL_H
#define __NXSAN_RUNTIME_INTERNAL_H

// nxsan runtime-internal definitions
// Author: c272

#include <cstddef>
#include <random>
#include <stdint.h>
#include <string>

/*********************
 * Internal defines. *
 *********************/

// Number of bits used for the nxsan pointer tag.
#define __NXSAN_TAG_SIZE_BITS 8
static_assert(__NXSAN_TAG_SIZE_BITS == 4 || __NXSAN_TAG_SIZE_BITS == 8,
              "Tag size must be either four or eight bits.");
#define __NXSAN_TAG_MAX_VAL (0xFF >> (8 - __NXSAN_TAG_SIZE_BITS))

// Masks for extracting the tag from a 64-bit value.
#define __NXSAN_TAG_MASK (0xFFFFFFFFFFFFFFFF << (64 - __NXSAN_TAG_SIZE_BITS))
#define __NXSAN_INVERSE_TAG_MASK (0xFFFFFFFFFFFFFFFF >> __NXSAN_TAG_SIZE_BITS)

// Macro for manipulating the tag with a 64-bit pointer.
#define __NXSAN_EXTRACT_TAG(x)                                                 \
  (uint8_t)(((uint64_t)x & __NXSAN_TAG_MASK) >> (64 - __NXSAN_TAG_SIZE_BITS))
#define __NXSAN_EMPLACE_TAG(x, tag)                                            \
  (void *)(((uint64_t)x & __NXSAN_INVERSE_TAG_MASK) |                          \
           ((uint64_t)tag << (64 - __NXSAN_TAG_SIZE_BITS)))
#define __NXSAN_REMOVE_TAG(x) (void *)((uint64_t)x & __NXSAN_INVERSE_TAG_MASK)

// Alignment (in bits) of allocated tracked memory.
#define __NXSAN_TAG_GRANULARITY_BYTES 16
static_assert(__NXSAN_TAG_GRANULARITY_BYTES >= alignof(std::max_align_t),
              "Tag granularity must be greater or equal than the largest "
              "required alignment for scalar types.");

// Size of pages to be tracked by nxsan.
#define __NXSAN_PAGE_SIZE_BYTES 4096

// Return codes from a pointer verification.
// clang-format off
#define __NXSAN_PTR_OK          0
#define __NXSAN_PTR_NOTAG       1
#define __NXSAN_PTR_BADTAG      2
#define __NXSAN_PTR_OUT_OF_HEAP 3
#define __NXSAN_PTR_OVERRUN     4
#define __NXSAN_PTR_NULLPAGE    5
#define __NXSAN_PTR_FREED       6
// clang-format on

// Internal allocation functions.
// Note: Allocation functions set for the below defines should be guaranteed to
// allocate within the tracked heap memory region, otherwise undefined behavior will occur.
#ifndef __NXSAN_INTERNAL_ALIGNED_ALLOC
#define __NXSAN_INTERNAL_ALIGNED_ALLOC std::aligned_alloc
#endif
#ifndef __NXSAN_INTERNAL_CALLOC
#define __NXSAN_INTERNAL_CALLOC std::calloc
#endif
#ifndef __NXSAN_INTERNAL_FREE
#define __NXSAN_INTERNAL_FREE std::free
#endif

/**************************
 * Global shadow storage. *
 **************************/

// Static pointer to the nxsan shadow memory.
extern uint8_t *__nxsan_shadow;

// Size of nxsan shadow memory store.
extern size_t __nxsan_shadow_size;

// Base of the heap.
extern uint8_t *__nxsan_heap_base;

// Random generator for tags.
extern std::uniform_int_distribution<short> __nxsan_tag_gen;

/***************************
 * Internal use utilities. *
 ***************************/

// Checks whether the nxasan shadow memory has been allocated.
inline __attribute__((always_inline)) bool __nxsan_check_init() {
  return __nxsan_shadow_size > 0;
}

// Fetches the address of the end of tracked heap memory.
inline __attribute__((always_inline)) uint8_t *__nxsan_get_heap_tail() {
  return __nxsan_heap_base +
         (__nxsan_shadow_size * __NXSAN_TAG_GRANULARITY_BYTES);
}

// Verifies whether the given pointer is within the tracked memory bounds.
inline __attribute__((always_inline)) bool
__nxsan_ptr_in_heap_bounds(void *ptr) {
  return (uint64_t)ptr >= (uint64_t)__nxsan_heap_base &&
         (uint64_t)ptr < (uint64_t)__nxsan_get_heap_tail();
}

// Verifies whether the given allocation is within the tracked heap bounds.
inline __attribute__((always_inline)) bool
__nxsan_alloc_in_heap_bounds(void *ptr, size_t size) {
  return __nxsan_ptr_in_heap_bounds(ptr) &&
         __nxsan_ptr_in_heap_bounds((uint8_t *)ptr + size);
}

// Returns the shadow address for a given memory location.
// Behavior is undefined when the provided pointer is outside of the tracked
// memory region.
inline __attribute__((always_inline)) uint8_t *
__nxsan_get_shadow_address(void *ptr) {
  // Remove tag.
  uint8_t *ptrNoTag = (uint8_t *)((uint64_t)ptr & __NXSAN_INVERSE_TAG_MASK);

  // Get distance from shadow heap base.
  size_t shadowDist =
      (uint64_t)(ptrNoTag - __nxsan_heap_base) / __NXSAN_TAG_GRANULARITY_BYTES;

  return __nxsan_shadow + shadowDist;
}

// Fetches the tag value for the given pointer.
inline __attribute__((always_inline)) uint8_t
__nxsan_get_shadow_tag(void *ptr) {
  uint8_t *shadowAddr = __nxsan_get_shadow_address(ptr);
  return *shadowAddr;
}

// Verifies that the given pointer:
//   - Is within the tracked heap range.
//   - Has a valid tag value that matches the shadow heap.
// If the pointer does not have a valid tag (>0), the method will return
// PTR_NOTAG. Accesses to addresses with no tag may emit a warning, but should
// not be treated as an error. This is to avoid accesses to clean (untagged)
// pointers causing an nxsan abort.
uint8_t __nxsan_verify_ptr(void *ptr);

// Initialises the tag generator for use.
void __nxsan_init_tag_gen();

/********************
 * Error utilities. *
 ********************/

// Aborts the running application with an error stemming from a bad pointer
// access.
void __nxsan_abort_with_access_err(void *ptr, const char *fmt, ...);

// Aborts the running application with a generic error.
void __nxsan_abort_with_err(const char *fmt, ...);

// Creates a backtrace of the current call stack.
std::string __nxsan_bt();

#endif
