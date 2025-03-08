#include <filesystem>
#include <fstream>
#include <iostream>

#include "instrumentation/AccessInstrumenter.hpp"
#include "instrumentation/CliArguments.hpp"

int main(int argc, char **argv) {
  // Parse CLI arguments.
  auto argsRes = nxsan::CliArguments::Parse(argc, argv);
  if (argsRes.HasError()) {
    std::cout << "Failed to parse CLI arguments: " << argsRes.Error()
              << std::endl;
    return 1;
  }
  auto args = argsRes.Result();

  // Print manual if requested.
  if (args.IsHelpRequested()) {
      args.PrintManual();
      return 0;
  }

  // For each input file, attempt to parse LLVM.
  for (auto &inputFile : args.GetInputFiles()) {
    // Create instrumenter, run it on input file.
    nxsan::AccessInstrumenter acins(inputFile);
    auto result = acins.GenerateIR();

    // If there was an error instrumenting, report that.
    if (result.HasError()) {
      std::cout << "nxsan-instrumentation-cxx: " << result.Error() << std::endl;
      continue;
    }

    // Get the output file path to write to.
    std::filesystem::path inputPath = inputFile;
    std::string outputName = args.GetOutFileName(inputPath.filename().replace_extension());
    std::filesystem::path outPath = inputPath.replace_filename(outputName);

    // Write the IR to file.
    {
      std::ofstream ostr(outPath);
      if (!ostr.is_open()) {
        std::cout << "nxsan-instrumentation-cxx: Failed to open output file stream." << std::endl;
        continue;
      }
      ostr << result.Result().ir;
    }
  }

  return 0;
}
