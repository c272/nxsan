#pragma once

#include <optional>
#include <string>

namespace nxsan {

// Utility class for a simple result type holding a result or error.
template <typename ResT, typename ErrT> struct NxsResult {
public:
  NxsResult(const ResT &res) {
    result = res;
    error = std::nullopt;
  }

  NxsResult(const ErrT &err) {
    error = err;
    result = std::nullopt;
  }

  bool HasError() { return error.has_value(); }
  bool HasResult() { return result.has_value(); }
  const ErrT &Error() { return error.value(); }
  const ResT &Result() { return result.value(); }
  const ResT &ResultOr(const ResT &value) { return result.value_or(value); }

private:
  std::optional<ResT> result;
  std::optional<ErrT> error;
};

// Simple pass-fail error message utility.
using NxsError = std::optional<std::string>;

} // namespace nxsan
