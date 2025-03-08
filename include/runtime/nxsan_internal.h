#pragma once
#ifndef __NXSAN_RUNTIME_INTERNAL_H
#define __NXSAN_RUNTIME_INTERNAL_H

// nxsan runtime-internal definitions
// Author: c272

#include <cstddef>
#include <stdint.h>

/*********************
 * Internal defines. *
 *********************/

// Number of bits used for the nxsan pointer tag.
#define __NXSAN_TAG_SIZE_BITS 8

// Alignment (in bits) of allocated tracked memory.
#define __NXSAN_TAG_GRANULARITY_BYTES 16
static_assert(__NXSAN_TAG_GRANULARITY_BYTES >= alignof(std::max_align_t),
              "Tag granularity must be greater or equal than the largest required alignment for scalar types.");

/**************************
 * Global shadow storage. *
 **************************/

// Static pointer to the nxsan shadow memory.
extern uint8_t* __nxsan_shadow;

// Size of nxsan shadow memory store.
extern size_t __nxsan_shadow_size;

/***************************
 * Internal use utilities. *
 ***************************/

// Checks whether the nxasan shadow memory has been allocated.
inline __attribute__((always_inline)) bool __nxsan_check_init() {
  return __nxsan_shadow_size > 0;
}

#endif
