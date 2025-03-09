#include <gtest/gtest.h>

#include "runtime/nxsan_runtime.h"

// Ensure initialisation does not work twice.
TEST(RuntimeInit, NoDoubleInit) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  EXPECT_TRUE(__nxsan_init((void*)0x0, 0xFFFF));
  EXPECT_FALSE(__nxsan_init((void*)0x0, 0xFFFF));
  EXPECT_TRUE(__nxsan_terminate());
}

// Ensure termination does not work twice.
TEST(RuntimeInit, NoDoubleTerminate) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  EXPECT_TRUE(__nxsan_init((void*)0x0, 0xFFFF));
  EXPECT_TRUE(__nxsan_terminate());
  EXPECT_FALSE(__nxsan_terminate());
}

// Ensure termination does not work when not initialised.
TEST(RuntimeInit, NoUselessTerminate) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  EXPECT_FALSE(__nxsan_terminate());
}

// Ensure initialisation does not work with an zero size.
TEST(RuntimeInit, SizeCheckZero) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  ASSERT_DEATH(__nxsan_init((void*)0x0, 0x0), "");
}

// Ensure initialisation does not work when the given heap extends into the
// tag region.
TEST(RuntimeInit, HeapTailCheck) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  ASSERT_DEATH(__nxsan_init((void*)0xFFFFFFFFFFFFFFFF, 0xFFFF), "");
}
