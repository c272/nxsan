#include "runtime/nxsan_internal.h"

#include <cstdarg>
#include <cstdlib>
#include <iostream>
#include <stdio.h>

// Header/footer for error messages.
#define __NXSAN_ERR_HEADER "\n================================================="
#define __NXSAN_ERR_FOOTER "=== ABORTING ==="

// Aborts the application.
static inline __attribute__((always_inline)) void
__nxsan_abort(void) {
  std::abort();
}

void __nxsan_abort_with_access_err(void* ptr, const char* fmt, ...) {
  // Strip tag from pointer for display.
  ptr = __NXSAN_REMOVE_TAG(ptr);

  // Output header w/ location of illegal access.
  std::cerr << __NXSAN_ERR_HEADER << std::endl;
  std::cerr << "ERROR: NxSanitizer(" << ptr << "): ";

  // Output format string to stderr.
  va_list argptr;
  va_start(argptr, fmt);
  vfprintf(stderr, fmt, argptr);
  va_end(argptr);

  // End line.
  std::cerr << std::endl;

  // Show backtrace of illegal access.
  std::cerr << __nxsan_bt() << std::endl;

  // If available, show where memory was previously allocated/freed.
  // ... todo ...

  // Output footer.
  std::cerr << __NXSAN_ERR_FOOTER << std::endl;

  // Abort.
  __nxsan_abort();
}

void __nxsan_abort_with_err(const char* fmt, ...) {
  // Output header.
  std::cerr << __NXSAN_ERR_HEADER << std::endl;
  std::cerr << "ERROR: NxSanitizer: ";

  // Output format string to stderr.
  va_list argptr;
  va_start(argptr, fmt);
  vfprintf(stderr, fmt, argptr);
  va_end(argptr);

  // End line.
  std::cerr << std::endl;

  // Show backtrace for error.
  std::cerr << __nxsan_bt() << std::endl;

  // Output footer.
  std::cerr << __NXSAN_ERR_FOOTER << std::endl;

  // Abort.
  __nxsan_abort();
}
