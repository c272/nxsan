#include "runtime/nxsan_internal.h"
#include "runtime/nxsan_runtime.h"

// Access type codes for reporting.
#define NXSAN_ACCESS_TYPE_UNK 0
#define NXSAN_ACCESS_TYPE_LOAD 1
#define NXSAN_ACCESS_TYPE_STORE 2

// Called to report a single byte access to a memory region.
inline __attribute__((always_inline)) void
__nxsan_report_single_byte(void *p, uint8_t accessType) {
  if (!__nxsan_check_init()) { return; }
}

// Called to report a multi-byte access to a memory region.
inline __attribute__((always_inline)) void
__nxsan_report_multi_byte(void *p, uint8_t size, uint8_t accessType) {
  // ...
}

// External-facing instruments.
// clang-format off
void __nxsan_report_load8  (void *p) { __nxsan_report_single_byte (p,    NXSAN_ACCESS_TYPE_LOAD ); }
void __nxsan_report_load16 (void *p) { __nxsan_report_multi_byte  (p, 2, NXSAN_ACCESS_TYPE_LOAD ); }
void __nxsan_report_load32 (void *p) { __nxsan_report_multi_byte  (p, 4, NXSAN_ACCESS_TYPE_LOAD ); }
void __nxsan_report_load64 (void *p) { __nxsan_report_multi_byte  (p, 8, NXSAN_ACCESS_TYPE_LOAD ); }
void __nxsan_report_store8 (void *p) { __nxsan_report_single_byte (p,    NXSAN_ACCESS_TYPE_STORE); }
void __nxsan_report_store16(void *p) { __nxsan_report_multi_byte  (p, 2, NXSAN_ACCESS_TYPE_STORE); }
void __nxsan_report_store32(void *p) { __nxsan_report_multi_byte  (p, 4, NXSAN_ACCESS_TYPE_STORE); }
void __nxsan_report_store64(void *p) { __nxsan_report_multi_byte  (p, 8, NXSAN_ACCESS_TYPE_STORE); }
// clang-format on
