#include "instrumentation/CliArguments.hpp"

#include <iostream>

namespace nxsan {

NxsResult<CliArguments, std::string> CliArguments::Parse(int argc,
                                                         char **argv) {
  CliArguments out;

  // We start at 1 here to ignore the executable at argv[0].
  for (int i = 1; i < argc; i++) {
    std::string arg(argv[i]);

    // If the parameter is an option, parse that.
    if (arg.rfind("--", 0) == 0) {
      // Parse option.
      std::optional<std::string> next = std::nullopt;
      if ((i + 1) < argc) {
        next = std::string(argv[i + 1]);
      }
      auto parseRes = out.ParseOpt(arg.substr(2), next);

      // Exit early on error.
      if (parseRes.HasError()) {
        return parseRes.Error();
      }

      // Skip next parameter if required & continue.
      if (parseRes.Result()) {
        ++i;
      }
      continue;
    }

    out.m_inputFiles.push_back(argv[i]);
  }

  if (out.m_inputFiles.size() == 0 && !out.m_printHelp) {
    return std::string("No input files.");
  }

  return out;
}

void CliArguments::PrintManual() {
  // Overview.
  std::cout << "OVERVIEW: nxsan instrumentation tool" << std::endl;
  std::cout << std::endl;
  std::cout << "Generates instrumentation function calls to the nxsan runtime" << std::endl;
  std::cout << "for all store and load instructions to memory." << std::endl;
  std::cout << std::endl;

  // Usage.
  std::cout << "USAGE: nxsan-instrumentation-cxx [options] file..." << std::endl;
  std::cout << std::endl;

  // Options.
  std::cout << "OPTIONS:" << std::endl;
  std::cout << "  --help" << std::endl;
  std::cout << "      Prints this usage manual." << std::endl;
  std::cout << "  --out" << std::endl;
  std::cout << "      Output file pattern. The original file name will be substituted where '{}' is present." << std::endl;

}

std::string CliArguments::GetOutFileName(const std::string& inFileName) {
  std::string outFileName = GetOutFileFormat();
  std::string token = "{}";

  size_t start_pos = 0;
  while((start_pos = outFileName.find(token, start_pos)) != std::string::npos) {
    outFileName.replace(start_pos, token.length(), inFileName);
    start_pos += inFileName.length();
  }

  return outFileName;
}

NxsResult<bool, std::string>
CliArguments::ParseOpt(std::string opt, std::optional<std::string> next) {
  // Output file.
  if (opt == "out") {
    if (!next.has_value()) {
      return "No value provided for option '--out'.";
    }
    m_outFile = next.value();
    return true;
  }

  // Manual.
  if (opt == "help") {
    m_printHelp = true;
    return false;
  }

  return std::string("Unknown option '") + opt + "'.";
}

} // namespace nxsan
