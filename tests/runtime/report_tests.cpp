#include <gtest/gtest.h>

#define __NXSAN_OUTLINE_REPORTING
#include "runtime/nxsan_runtime.h"
#include "runtime/nxsan_internal.h"


#define TRACK_REGION_BASE (void*)0x0
#define TRACK_REGION_SIZE 0xFFFFFFFF

// todo. placeholder test
TEST(Reporting, OOB) {
  if (__nxsan_check_init()) {
    __nxsan_terminate();
  }
  EXPECT_TRUE(__nxsan_init(TRACK_REGION_BASE, TRACK_REGION_SIZE));

  uint8_t* pt = (uint8_t*)__nxsan_malloc(8);
  __nxsan_report_load64(pt);
  *pt = 4;
  __nxsan_free(pt);

  EXPECT_TRUE(__nxsan_terminate());
}
