#pragma once

#include <optional>
#include <string>
#include <vector>

#include "utils/NxsResult.hpp"

namespace nxsan {

// Class for handling CLI arguments passed to the NXSAN
// instrumentation tool.
class CliArguments {
public:
  // Parses command line arguments into a CLI arguments structure.
  static NxsResult<CliArguments, std::string> Parse(int argc, char **argv);

  // Prints the usage manual to stdout.
  static void PrintManual();

  // Returns input files configured for conversion.
  const std::vector<std::string> &GetInputFiles() const { return m_inputFiles; }

  // Returns whether the manual has been requested.
  bool IsHelpRequested() const { return m_printHelp; }

  // Returns the output file format, if configured.
  std::string GetOutFileFormat() const {
    return m_outFile.value_or("{}_nxsan.ll");
  }

  // Returns the output file name for a given input file.
  // Based on the output file format in the command line arguments.
  std::string GetOutFileName(const std::string& inFileName);

private:
  // Parses the given option out. Returns whether the next parameter was
  // consumed.
  NxsResult<bool, std::string> ParseOpt(std::string opt,
                                        std::optional<std::string> next);

  bool m_printHelp;
  std::vector<std::string> m_inputFiles;
  std::optional<std::string> m_outFile;
};

} // namespace nxsan
