#include "runtime/nxsan_internal.h"

// Defines for backtracing.
#define __NXSAN_BT_MAX_DEPTH 64
#define __NXSAN_BT_UNAVAILABLE_MSG "\nNOTE: NxSanitizer cannot provide additional information.\n"
#define __NXSAN_BT_UNK_SYMBOL_MSG "(missing symbol)"

// Cross-platform includes.
#include <cstdlib>
#include <string>

// Platform-specific includes.
#ifdef linux
#include <execinfo.h>
#endif

std::string __nxsan_bt() {
#ifdef linux
  void* btCallersBuf[__NXSAN_BT_MAX_DEPTH];

  // Get backtrace with hard limit.
  int numCallers = backtrace(btCallersBuf, __NXSAN_BT_MAX_DEPTH);
  if (numCallers == 0) {
    return __NXSAN_BT_UNAVAILABLE_MSG;
  }

  // Get backtrace symbols.
  char** btSymbols = backtrace_symbols(btCallersBuf, numCallers);

  // For each caller, output to the message buffer.
  std::string btMsg;
  for (int i = 0; i < numCallers; i++) {
    // Fetch symbol for backtrace.
    const char* symbol = btSymbols[i];
    if (!symbol) {
      symbol = "(missing symbol)";
    }

    btMsg += "   #" + std::to_string(i) + " " + std::string(symbol) + "\n";
  }

  // Free symbols buffer.
  std::free((void*)btSymbols);

  return btMsg;
#else
  return __NXSAN_BT_UNAVAILABLE_MSG;
#endif
}
