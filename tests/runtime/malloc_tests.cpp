#include <gtest/gtest.h>

#define __NXSAN_OUTLINE_REPORTING
#include "runtime/nxsan_runtime.h"
#include "runtime/nxsan_internal.h"

#define TRACK_REGION_BASE (void*)0x0
#define TRACK_REGION_SIZE 0xFFFFFFFF

// Small allocations available.
TEST(AllocFree, SmallAllocation) {
  EXPECT_TRUE(__nxsan_init(TRACK_REGION_BASE, TRACK_REGION_SIZE));

  uint8_t* pt = (uint8_t*)__nxsan_malloc(8);
  __nxsan_report_load64(pt);
  *pt = 4;
  __nxsan_free(pt);

  EXPECT_TRUE(__nxsan_terminate());
}

// Multi-granule allocations.
TEST(AllocFree, MultiGranuleAllocation) {
  EXPECT_TRUE(__nxsan_init(TRACK_REGION_BASE, TRACK_REGION_SIZE));

  uint8_t* pt = (uint8_t*)__nxsan_malloc(__NXSAN_TAG_GRANULARITY_BYTES + 6);
  __nxsan_report_load8(pt);
  __nxsan_report_load8(pt + (__NXSAN_TAG_GRANULARITY_BYTES + 5));
  *pt = 4;
  *(pt + __NXSAN_TAG_GRANULARITY_BYTES + 5) = 12;
  __nxsan_free(pt);

  EXPECT_TRUE(__nxsan_terminate());
}

// Do not allow allocations while uninitialized.
TEST(AllocFree, NoPreInitAlloc) {
  ASSERT_DEATH(__nxsan_malloc(__NXSAN_TAG_GRANULARITY_BYTES), "nxsan-noinit-alloc");
}

// Do not allow zero-size allocations.
TEST(AllocFree, NoZeroSizeAlloc) {
  if (__nxsan_check_init()) {
    __nxsan_terminate();
  }
  EXPECT_TRUE(__nxsan_init(TRACK_REGION_BASE, TRACK_REGION_SIZE));
  ASSERT_DEATH(__nxsan_malloc(0), "nxsan-alloc-zero");
}

// Do not allow stupidly large allocations.
TEST(AllocFree, NoHugeAlloc) {
  if (__nxsan_check_init()) {
    __nxsan_terminate();
  }
  EXPECT_TRUE(__nxsan_init(TRACK_REGION_BASE, TRACK_REGION_SIZE));
  ASSERT_DEATH(__nxsan_malloc(0xFFFFFFFFFFFF), "nxsan-alloc-fail");
}

// Check tag clear for single granule allocations.
TEST(AllocFree, TagClearSingleGranule) {
  if (__nxsan_check_init()) {
    __nxsan_terminate();
  }
  EXPECT_TRUE(__nxsan_init(TRACK_REGION_BASE, TRACK_REGION_SIZE));

  // Allocate & grab tag.
  uint8_t* pt = (uint8_t*)__nxsan_malloc(6);
  *pt = 4;
  uint8_t tag = __NXSAN_EXTRACT_TAG(pt);
  EXPECT_TRUE(tag > 0x0);

  // Ensure short granule for shadow region is set.
  uint8_t allocShadowTag = *__nxsan_get_shadow_address(pt);
  EXPECT_TRUE(allocShadowTag == 6);

  // Ensure tag is set within the final byte of allocated granule.
  uint8_t finalByteTag = *(pt + __NXSAN_TAG_GRANULARITY_BYTES - 1);
  EXPECT_TRUE(finalByteTag == tag);

  // Free & check shadow tag is zeroed.
  __nxsan_free(pt);
  uint8_t freedShadowTag = *__nxsan_get_shadow_address(pt);
  EXPECT_TRUE(freedShadowTag == 0x0);

  EXPECT_TRUE(__nxsan_terminate());
}

// Check tag clear for multi-granule (large) allocations.
TEST(AllocFree, TagClearMultiGranule) {
  if (__nxsan_check_init()) {
    __nxsan_terminate();
  }
  EXPECT_TRUE(__nxsan_init(TRACK_REGION_BASE, TRACK_REGION_SIZE));

  // Allocate & grab tag.
  uint8_t* pt = (uint8_t*)__nxsan_malloc(4096);
  uint8_t tag = __NXSAN_EXTRACT_TAG(pt);
  EXPECT_TRUE(tag > 0x0);

  // Ensure tag for shadow region is set.
  uint8_t allocShadowTag = *__nxsan_get_shadow_address(pt);
  EXPECT_TRUE(tag == allocShadowTag);

  // Ensure the tag for the following shadow region is set.
  uint8_t nextShadowTag = *(__nxsan_get_shadow_address(pt) + 1);
  EXPECT_TRUE(tag == nextShadowTag);

  // Free & check shadow tag is zeroed.
  __nxsan_free(pt);
  uint8_t freedShadowTag = *__nxsan_get_shadow_address(pt);
  EXPECT_TRUE(freedShadowTag == 0x0);

  // Check next shadow tag is zeroed.
  uint8_t nextFreedShadowTag = *(__nxsan_get_shadow_address(pt) + 1);
  EXPECT_TRUE(nextFreedShadowTag == 0x0);

  EXPECT_TRUE(__nxsan_terminate());
}

// Ensure that the tag generator creates unique tags.
TEST(AllocFree, UniqueTagGenerator) {
  if (__nxsan_check_init()) {
    __nxsan_terminate();
  }
  EXPECT_TRUE(__nxsan_init(TRACK_REGION_BASE, TRACK_REGION_SIZE));

  // Allocate & grab tags.
  constexpr int kNumTags = 5;
  uint8_t* ptrs[kNumTags];
  uint8_t tags[kNumTags];
  for (int i = 0; i < kNumTags; i++) {
    ptrs[i] = (uint8_t*)__nxsan_malloc(8);
    tags[i] = __NXSAN_EXTRACT_TAG(ptrs[i]);
  }

  // Make sure at least one tag is unique.
  bool diff = false;
  uint8_t firstTag = tags[0];
  for (int i=1; i<kNumTags; i++) {
    if (tags[i] != firstTag) {
      diff = true;
      break;
    }
  }
  EXPECT_TRUE(diff);

  // Free up allocated memory.
  for (int i = 0; i < kNumTags; i++) {
    __nxsan_free(ptrs[i]);
  }

  EXPECT_TRUE(__nxsan_terminate());
}

// Attempt to free pointer outside of heap bounds.
TEST(AllocFree, FreeOOB) {
  if (__nxsan_check_init()) {
    __nxsan_terminate();
  }
  EXPECT_TRUE(__nxsan_init(TRACK_REGION_BASE, TRACK_REGION_SIZE));
  ASSERT_DEATH(__nxsan_free((void*)0xFFFFFFFFFFFFFF8), "nxsan-oob-free");
}

// Attempt to free unaligned pointer.
TEST(AllocFree, FreeUnaligned) {
  if (__nxsan_check_init()) {
    __nxsan_terminate();
  }
  EXPECT_TRUE(__nxsan_init(TRACK_REGION_BASE, TRACK_REGION_SIZE));
  uint8_t* pt = (uint8_t*)__nxsan_malloc(16);
  ASSERT_DEATH(__nxsan_free(pt + 6), "nxsan-unaligned-free");
}

// Attempt to free shadow memory (lol).
TEST(AllocFree, FreeShadow) {
  if (__nxsan_check_init()) {
    __nxsan_terminate();
  }
  EXPECT_TRUE(__nxsan_init(TRACK_REGION_BASE, TRACK_REGION_SIZE));
  ASSERT_DEATH(__nxsan_free(__nxsan_shadow), "");
}


// Attempt to free untagged pointer.
TEST(AllocFree, FreeUntagged) {
  if (__nxsan_check_init()) {
    __nxsan_terminate();
  }
  EXPECT_TRUE(__nxsan_init(TRACK_REGION_BASE, TRACK_REGION_SIZE));
  uint8_t* pt = (uint8_t*)__nxsan_malloc(16);
  pt = (uint8_t*)__NXSAN_REMOVE_TAG(pt);
  ASSERT_DEATH(__nxsan_free(pt), "nxsan-notag-free");
}

// Attempt to free pointer with bad tag.
TEST(AllocFree, FreeBadTag) {
  if (__nxsan_check_init()) {
    __nxsan_terminate();
  }
  EXPECT_TRUE(__nxsan_init(TRACK_REGION_BASE, TRACK_REGION_SIZE));
  uint8_t* pt = (uint8_t*)__nxsan_malloc(16);
  uint8_t tag = __NXSAN_EXTRACT_TAG(pt);

  // poison tag
  uint8_t badtag = tag == 255 ? tag - 1 : tag + 1;
  pt = (uint8_t*)__NXSAN_REMOVE_TAG(pt);
  pt = (uint8_t*)__NXSAN_EMPLACE_TAG(pt, badtag);

  ASSERT_DEATH(__nxsan_free(pt), "nxsan-badtag-free");
}

// Attempt to free nullpage pointer.
TEST(AllocFree, FreeNullpage) {
  if (__nxsan_check_init()) {
    __nxsan_terminate();
  }
  EXPECT_TRUE(__nxsan_init(TRACK_REGION_BASE, TRACK_REGION_SIZE));
  ASSERT_DEATH(__nxsan_free((void*)__NXSAN_TAG_GRANULARITY_BYTES), "nxsan-nullpage-free");
}

// Attempt to double free pointer for single granule memory.
TEST(AllocFree, DoubleFreeSingleGranule) {
  if (__nxsan_check_init()) {
    __nxsan_terminate();
  }
  EXPECT_TRUE(__nxsan_init(TRACK_REGION_BASE, TRACK_REGION_SIZE));
  uint8_t* pt = (uint8_t*)__nxsan_malloc(__NXSAN_TAG_GRANULARITY_BYTES - 1);
  __nxsan_free(pt);
  ASSERT_DEATH(__nxsan_free(pt), "nxsan-double-free");
}

// Attempt to double free pointer for multi granule memory.
TEST(AllocFree, DoubleFreeMultiGranule) {
  if (__nxsan_check_init()) {
    __nxsan_terminate();
  }
  EXPECT_TRUE(__nxsan_init(TRACK_REGION_BASE, TRACK_REGION_SIZE));
  uint8_t* pt = (uint8_t*)__nxsan_malloc(__NXSAN_TAG_GRANULARITY_BYTES * 4);
  __nxsan_free(pt);
  ASSERT_DEATH(__nxsan_free(pt), "nxsan-double-free");
}
