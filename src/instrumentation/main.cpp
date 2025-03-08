#include <iostream>

#include "instrumentation/AccessInstrumenter.hpp"
#include "instrumentation/CliArguments.hpp"

int main(int argc, char** argv) {
    // Parse CLI arguments.
    auto argsRes = nxsan::CliArguments::Parse(argc, argv);
    if (argsRes.HasError()) {
        std::cout << "Failed to parse CLI arguments: " << argsRes.Error() << std::endl;
        return 1;
    }
    auto args = argsRes.Result();

    // For each input file, attempt to parse LLVM.
    for (auto& inputFile : args.GetInputFiles()) {
        // Create instrumenter, run it on input file.
        nxsan::AccessInstrumenter acins(inputFile);
        auto result = acins.GenerateIR();

        // If there was an error instrumenting, report that.
        if (result.HasError()) {
            std::cout << "nxsan-instrumentation-cxx: " << result.Error() << std::endl;
            continue;
        }

        // Output module.
        std::cout << result.Result().ir << std::endl;
        std::cout << std::endl;
        std::cout << "Instrumented " << result.Result().numLoads << " loads." << std::endl;
        std::cout << "Instrumented " << result.Result().numStores << " stores." << std::endl;
    }

    return 0;
}
